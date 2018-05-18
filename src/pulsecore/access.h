#ifndef fooaccesshfoo
#define fooaccesshfoo

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
            2015 Wim Taymans <wtaymans@redhat.com>

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#include <sys/types.h>

#include <pulsecore/client.h>

typedef enum pa_access_hook {
    /* context */
    PA_ACCESS_HOOK_EXIT_DAEMON,
    PA_ACCESS_HOOK_SET_DEFAULT_SINK,
    PA_ACCESS_HOOK_SET_DEFAULT_SOURCE,

    /* introspection */
    PA_ACCESS_HOOK_GET_SINK_INFO,
    PA_ACCESS_HOOK_SET_SINK_VOLUME,
    PA_ACCESS_HOOK_SET_SINK_MUTE,
    PA_ACCESS_HOOK_SUSPEND_SINK,
    PA_ACCESS_HOOK_SET_SINK_PORT,

    PA_ACCESS_HOOK_GET_SOURCE_INFO,
    PA_ACCESS_HOOK_SET_SOURCE_VOLUME,
    PA_ACCESS_HOOK_SET_SOURCE_MUTE,
    PA_ACCESS_HOOK_SUSPEND_SOURCE,
    PA_ACCESS_HOOK_SET_SOURCE_PORT,

    PA_ACCESS_HOOK_GET_SERVER_INFO,

    PA_ACCESS_HOOK_GET_MODULE_INFO,
    PA_ACCESS_HOOK_LOAD_MODULE,
    PA_ACCESS_HOOK_UNLOAD_MODULE,

    PA_ACCESS_HOOK_GET_CLIENT_INFO,
    PA_ACCESS_HOOK_KILL_CLIENT,

    PA_ACCESS_HOOK_GET_CARD_INFO,
    PA_ACCESS_HOOK_SET_CARD_PROFILE,
    PA_ACCESS_HOOK_SET_PORT_LATENCY_OFFSET,

    PA_ACCESS_HOOK_GET_SINK_INPUT_INFO,
    PA_ACCESS_HOOK_MOVE_SINK_INPUT,
    PA_ACCESS_HOOK_SET_SINK_INPUT_VOLUME,
    PA_ACCESS_HOOK_SET_SINK_INPUT_MUTE,
    PA_ACCESS_HOOK_KILL_SINK_INPUT,

    PA_ACCESS_HOOK_GET_SOURCE_OUTPUT_INFO,
    PA_ACCESS_HOOK_MOVE_SOURCE_OUTPUT,
    PA_ACCESS_HOOK_SET_SOURCE_OUTPUT_VOLUME,
    PA_ACCESS_HOOK_SET_SOURCE_OUTPUT_MUTE,
    PA_ACCESS_HOOK_KILL_SOURCE_OUTPUT,

    PA_ACCESS_HOOK_STAT,

    PA_ACCESS_HOOK_GET_SAMPLE_INFO,
    /* sample cache */
    PA_ACCESS_HOOK_CONNECT_UPLOAD,
    PA_ACCESS_HOOK_REMOVE_SAMPLE,
    PA_ACCESS_HOOK_PLAY_SAMPLE,
    /* stream */
    PA_ACCESS_HOOK_CONNECT_PLAYBACK,
    PA_ACCESS_HOOK_CONNECT_RECORD,
    /* extension */
    PA_ACCESS_HOOK_EXTENSION,

    PA_ACCESS_HOOK_FILTER_SUBSCRIBE_EVENT,

    PA_ACCESS_HOOK_MAX
} pa_access_hook_t;

typedef struct pa_access_data pa_access_data;

struct pa_access_data {
    pa_access_hook_t hook;
    uint32_t client_index;
    uint32_t object_index;
    pa_subscription_event_type_t event;
    const char *name;

    void (*async_finish_cb) (pa_access_data *data, bool res);
};

#endif
