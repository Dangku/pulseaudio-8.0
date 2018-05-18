/***
  This file is part of PulseAudio.

  Copyright 2016 Simon Fels <simon.fels@canonical.com>

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

#include <pulsecore/core-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/module.h>

#include <hybris/properties/properties.h>

#include "module-droid-discover-symdef.h"

PA_MODULE_AUTHOR("Simon Fels");
PA_MODULE_DESCRIPTION("Detect on which Android version we're running and load correct implementation for it");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define ANDROID_PROPERTY_VERSION_RELEASE "ro.build.version.release"

#define MODULE_DROID_CARD_PREFIX "module-droid-card-"

struct userdata {
    uint32_t module_idx;
};

int pa__init(pa_module* m) {
    struct userdata *u;
    pa_module *mm;
    char version[50];
    char *module_name = NULL;

    pa_assert(m);

    if (property_get(ANDROID_PROPERTY_VERSION_RELEASE, version, "") < 0 || !version) {
        pa_log("Failed to determine Android platform version we're running on");
        return -1;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);

	if (!strncmp(version, "5.1", 3) && pa_module_exists(MODULE_DROID_CARD_PREFIX "22"))
		module_name = MODULE_DROID_CARD_PREFIX "22";
	else if (!strncmp(version, "4.4", 3) && pa_module_exists(MODULE_DROID_CARD_PREFIX "19"))
		module_name = MODULE_DROID_CARD_PREFIX "19";
    else {
        pa_log("Unsupported Android version %s", version);
        pa_xfree(u);
        return -1;
    }

    mm = pa_module_load(m->core, module_name, m->argument);
    if (mm)
        u->module_idx = mm->index;

    if (u->module_idx == PA_INVALID_INDEX) {
        pa_xfree(u);
        pa_log("Failed to load droid card module for Android version %s", version);
        return -1;
    }

    return 0;
}

void pa__done(pa_module* m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->module_idx != PA_INVALID_INDEX)
        pa_module_unload_by_index(m->core, u->module_idx, true);

    pa_xfree(u);
}
