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

#ifndef footruststorehfoo
#define footruststorehfoo

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/creds.h>

#ifdef HAVE_TRUST_STORE
PA_C_DECL_BEGIN
typedef struct pa_trust_store pa_trust_store;

pa_trust_store* pa_trust_store_new();
void pa_trust_store_free(pa_trust_store *ts);
bool pa_trust_store_check(pa_trust_store *ts, const char *appname,
    uid_t uid, pid_t pid, const char *description);
PA_C_DECL_END
#endif

#endif
