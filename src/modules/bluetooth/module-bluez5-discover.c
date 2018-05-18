/***
  This file is part of PulseAudio.

  Copyright 2008-2013 João Paulo Rechi Vita

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/shared.h>

#include "bluez5-util.h"

#include "module-bluez5-discover-symdef.h"

PA_MODULE_AUTHOR("João Paulo Rechi Vita");
PA_MODULE_DESCRIPTION("Detect available BlueZ 5 Bluetooth audio devices and load BlueZ 5 Bluetooth audio drivers");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
    "headset=ofono|native|auto|both "
    "profile=<a2dp|hsp|hfgw> "
    "sco_sink=<name of sink> "
    "sco_source=<name of source> "
);

static const char* const valid_modargs[] = {
    "headset",
    "profile",
    "sco_sink",
    "sco_source",
    NULL
};

struct userdata {
    pa_module *module;
    pa_modargs *modargs;
    pa_core *core;
    pa_hashmap *loaded_device_paths;
    pa_hook_slot *device_connection_changed_slot;
    pa_bluetooth_discovery *discovery;
};

static pa_hook_result_t device_connection_changed_cb(pa_bluetooth_discovery *y, const pa_bluetooth_device *d, struct userdata *u) {
    bool module_loaded;

    pa_assert(d);
    pa_assert(u);

    module_loaded = pa_hashmap_get(u->loaded_device_paths, d->path) ? true : false;

    if (module_loaded && !pa_bluetooth_device_any_transport_connected(d)) {
        /* disconnection, the module unloads itself */
        pa_log_debug("Unregistering module for %s", d->path);
        pa_hashmap_remove(u->loaded_device_paths, d->path);
        return PA_HOOK_OK;
    }

    if (!module_loaded && pa_bluetooth_device_any_transport_connected(d)) {
        /* a new device has been connected */
        pa_module *m;
        char *args = pa_sprintf_malloc("path=%s", d->path);

        if (pa_modargs_get_value(u->modargs, "profile", NULL)) {
            char *profile;

            profile = pa_sprintf_malloc("%s profile=\"%s\"", args,
                                    pa_modargs_get_value(u->modargs, "profile", NULL));
            pa_xfree(args);
            args = profile;
        }

        if (pa_modargs_get_value(u->modargs, "sco_sink", NULL) &&
            pa_modargs_get_value(u->modargs, "sco_source", NULL)) {
            char *tmp;

            tmp = pa_sprintf_malloc("%s sco_sink=\"%s\" sco_source=\"%s\"", args,
                                    pa_modargs_get_value(u->modargs, "sco_sink", NULL),
                                    pa_modargs_get_value(u->modargs, "sco_source", NULL));
            pa_xfree(args);
            args = tmp;
        }

        pa_log_debug("Loading module-bluez5-device %s", args);
        m = pa_module_load(u->module->core, "module-bluez5-device", args);
        pa_xfree(args);

        if (m)
            /* No need to duplicate the path here since the device object will
             * exist for the whole hashmap entry lifespan */
            pa_hashmap_put(u->loaded_device_paths, d->path, d->path);
        else
            pa_log_warn("Failed to load module for device %s", d->path);

        return PA_HOOK_OK;
    }

    return PA_HOOK_OK;
}

#ifdef HAVE_BLUEZ_5_NATIVE_HEADSET
#ifdef HAVE_BLUEZ_5_OFONO_HEADSET
const char *default_headset_backend = "both";
#else
const char *default_headset_backend = "native";
#endif
#else
const char *default_headset_backend = "ofono";
#endif

int pa__init(pa_module *m) {
    struct userdata *u;
    pa_modargs *ma;
    const char *headset_str;
    int headset_backend;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments.");
        goto fail;
    }

    pa_assert_se(headset_str = pa_modargs_get_value(ma, "headset", default_headset_backend));
    if (pa_streq(headset_str, "ofono"))
        headset_backend = HEADSET_BACKEND_OFONO;
    else if (pa_streq(headset_str, "native"))
        headset_backend = HEADSET_BACKEND_NATIVE;
    else if (pa_streq(headset_str, "auto"))
        headset_backend = HEADSET_BACKEND_AUTO;
    else if (pa_streq(headset_str, "both"))
        headset_backend = HEADSET_BACKEND_BOTH;
    else {
        pa_log("headset parameter must be either ofono, native, auto or both (found %s)", headset_str);
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;
    u->modargs = ma;
    u->core = m->core;
    u->loaded_device_paths = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);

    if (!(u->discovery = pa_bluetooth_discovery_get(u->core, headset_backend)))
        goto fail;

    u->device_connection_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(u->discovery, PA_BLUETOOTH_HOOK_DEVICE_CONNECTION_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) device_connection_changed_cb, u);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);
    pa__done(m);
    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->device_connection_changed_slot)
        pa_hook_slot_free(u->device_connection_changed_slot);

    if (u->discovery)
        pa_bluetooth_discovery_unref(u->discovery);

    if (u->loaded_device_paths)
        pa_hashmap_free(u->loaded_device_paths);

    if (u->modargs)
        pa_modargs_free(u->modargs);

    pa_xfree(u);
}
