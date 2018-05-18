/***
    This file is part of PulseAudio.

    Copyright 2015 Canonical Ltd.
    Written by David Henningsson <david.henningsson@canonical.com>

    PulseAudio is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License,
    or (at your option) any later version.

    PulseAudio is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/i18n.h>
#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulse/xmalloc.h>
#include <pulsecore/fdsem.h>
#include <pulsecore/thread.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dynarray.h>
#include <pulse/mainloop-api.h>

#include <sys/apparmor.h>

#include "module-trust-store-symdef.h"

PA_MODULE_AUTHOR("David Henningsson");
PA_MODULE_DESCRIPTION("Ubuntu touch trust store integration");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#include "truststore.h"

#define REQUEST_GRANTED 1
#define REQUEST_DENIED -1
#define REQUEST_PENDING 0

struct userdata;

struct per_client {
    uint32_t index;
    char *appname;
    uid_t uid;
    pid_t pid;
    struct userdata *userdata;
    pa_dynarray *pending_requests;
    pa_atomic_t result;
};

struct userdata {
    pa_core *core;
    pa_trust_store *ts;
    pa_hashmap *per_clients;
    pa_thread *thread;
    pa_fdsem *fdsem;
    pa_io_event *io_event;
    pa_hook_slot *connect_hook_slot;
};

static struct per_client *per_client_new(struct userdata *u, uint32_t client_index) {
    struct per_client *pc;
    pa_client *client = pa_idxset_get_by_index(u->core->clients, client_index);

    pa_assert(client);
    if (!client->creds_valid) {
        pa_log_warn("Client %d has no creds, cannot authenticate", client_index);
        return NULL;
    }

    pc = pa_xnew0(struct per_client, 1);

    pa_log_info("Client application name is: '%s'", pa_proplist_gets(client->proplist, PA_PROP_APPLICATION_NAME));

    // ask apparmor about the context of the client, later this will be used to decide if the
    // app should be or not using the trust store

    if (aa_is_enabled()) {
        char *context = NULL;
        char *mode = NULL;

       /* XXX: The mode string is *NOT* be freed since it forms
        * part of the allocation returned in context.
        *
        * See aa_gettaskcon(2) for details.
        */
        if (aa_gettaskcon (client->creds.pid, &context, &mode) >= 0) {
            pc->appname = pa_xstrdup(context);
            pa_log_info("Client apparmor profile is: '%s'", context);
        } else {
            pa_log_error("AppArmo profile could not be retrieved.");
        }

        if (context)
            free(context);

    } else {
        // while we do leave the appname as empty if we fail, if apparmor is not present we should
        // assume that the app is not confined
        pc->appname = pa_xstrdup("unconfined");
    }

    pc->index = client_index;
    pc->uid = client->creds.uid;
    pc->pid = client->creds.pid;
    pc->pending_requests = pa_dynarray_new(NULL);
    pc->userdata = u;

    pa_hashmap_put(u->per_clients, (void *) (size_t) client_index, pc);

    return pc;
}

static void per_client_free_from_hashmap(void *data) {
    struct per_client *pc = data;
    if (!pc)
        return;
    pa_xfree(pc->appname);
    pa_dynarray_free(pc->pending_requests);
    pa_xfree(pc);
}

static void thread_func(void *data) {
    struct per_client *pc = data;

    // there are 3 possible values for the app name that we will consider at this point
    //  * empty string: there was an error when retrieving the value and therefore we will
    //                  automatically deny access
    //  * unconfined: the app is unconfined and therefore we will automatically grant access
    //  * appname: we need the user to decide what to do.

    if (pc->appname == NULL) {
        pa_log_info("Client apparmor could not retrieved.");
        pa_atomic_store(&pc->result, REQUEST_DENIED);
    } else if (pa_streq(pc->appname, "unconfined")) {
        pa_log_info("Connected client is unconfined.");
        pa_atomic_store(&pc->result, REQUEST_GRANTED);
    } else {
        pa_log_info("User needs to authorize the application..");
        bool result = pa_trust_store_check(pc->userdata->ts, pc->appname, pc->uid, pc->pid,
            /// TRANSLATORS: The app icon and name appears above this string. If the phrase
            /// can't be translated in this language, translate the whole sentence
            /// 'This app wants to record audio.'
            _("wants to record audio."));
        pa_atomic_store(&pc->result, result ? REQUEST_GRANTED : REQUEST_DENIED);
    }

    pa_fdsem_post(pc->userdata->fdsem);
}

static void check_queue(struct userdata *u) {
    struct per_client *pc;
    void *dummy;

    pa_log_debug("Checking queue, size: %d, thread object: %s",
                 pa_hashmap_size(u->per_clients), pa_yes_no(u->thread));
    if (u->thread) {
        pa_access_data *ad;
        unsigned i;
        int result;
        pc = pa_thread_get_data(u->thread);
        result = pa_atomic_load(&pc->result);
        if (result == REQUEST_PENDING)
            return; /* Still processing */
        pa_thread_free(u->thread);
        u->thread = NULL;

        pa_log_debug("Letting client %d (%s) know the result (%s)",
            pc->index, pc->appname, result == REQUEST_GRANTED ? "granted" : "rejected");
        PA_DYNARRAY_FOREACH(ad, pc->pending_requests, i) {
            ad->async_finish_cb(ad, result == REQUEST_GRANTED);
        }
        pa_hashmap_remove(u->per_clients, (void*) (size_t) pc->index);
    }

    /* Find a client that needs asking */
    PA_HASHMAP_FOREACH(pc, u->per_clients, dummy) {
        if (u->thread)
            return;
        pa_assert(pa_atomic_load(&pc->result) == REQUEST_PENDING);
        pa_log_debug("Starting thread to ask about client %d (%s)", pc->index,
                     pc->appname);
        u->thread = pa_thread_new("trust-store", thread_func, pc);
    }
}

static void check_fdsem(pa_mainloop_api *a, pa_io_event *e, int fd, pa_io_event_flags_t ee,
                        void *userdata) {
    struct userdata *u = userdata;
    pa_fdsem_after_poll(u->fdsem);
    check_queue(u);
    pa_fdsem_before_poll(u->fdsem);
}

static pa_hook_result_t connect_record_hook(pa_core *core, pa_access_data *d, struct userdata *u) {
    struct per_client *pc = pa_hashmap_get(u->per_clients, (void*) (size_t) d->client_index);
    int r;
    if (!pc)
        pc = per_client_new(u, d->client_index);

    r = pa_atomic_load(&pc->result);
    pa_log_debug("Checking permission to record for client %d (%s) ", d->client_index,
        r == REQUEST_GRANTED ? "granted" : r != REQUEST_PENDING ? "rejected" : "unknown");
    if (r == REQUEST_GRANTED) return PA_HOOK_OK;
    if (r != REQUEST_PENDING) return PA_HOOK_STOP;

    pa_dynarray_append(pc->pending_requests, d);
    check_queue(u);
    return PA_HOOK_CANCEL;
}

int pa__init(pa_module *m) {
    struct userdata *u;
    pa_trust_store *ts = pa_trust_store_new();
    if (ts == NULL)
        return -1;
    u = pa_xnew0(struct userdata, 1);
    u->ts = ts;
    u->core = m->core;
    u->per_clients = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func,
        NULL, per_client_free_from_hashmap);
    m->userdata = u;
    u->connect_hook_slot = pa_hook_connect(&m->core->access[PA_ACCESS_HOOK_CONNECT_RECORD],
        PA_HOOK_NORMAL, (pa_hook_cb_t) connect_record_hook, u);

    pa_assert_se(u->fdsem = pa_fdsem_new());
    pa_assert_se(u->io_event = m->core->mainloop->io_new(m->core->mainloop,
                 pa_fdsem_get(u->fdsem), PA_IO_EVENT_INPUT, check_fdsem, u));
    pa_fdsem_before_poll(u->fdsem);

    return 0;
}

void pa__done(pa_module *m) {
    struct userdata *u = m->userdata;
    if (u) {
        if (u->connect_hook_slot)
            pa_hook_slot_free(u->connect_hook_slot);
        if (u->thread)
            pa_thread_free(u->thread);
        if (u->io_event)
            m->core->mainloop->io_free(u->io_event);
        if (u->fdsem)
            pa_fdsem_free(u->fdsem);
        if (u->per_clients)
            pa_hashmap_free(u->per_clients);
        if (u->ts)
            pa_trust_store_free(u->ts);
        pa_xfree(u);
    }
}
