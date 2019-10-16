/*
 * Copyright (C) 2010      Advanced Micro Devices
 * Author Christoph Egger <Christoph.Egger@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include "libxl_osdeps.h" /* must come before any other headers */
#include "libxl_internal.h"

#include "tap-ctl.h"

int libxl__blktap_enabled(libxl__gc *gc)
{
    const char *msg;
    return !tap_ctl_check(&msg);
}

char *libxl__blktap_devpath(libxl__gc *gc,
                            const char *disk,
                            libxl_disk_format format)
{
    const char *type;
    char *params, *devname = NULL;
    tap_list_t tap;
    int err;

    type = libxl__device_disk_string_of_format(format);
    err = tap_ctl_find(type, disk, &tap);
    if (err == 0) {
        devname = GCSPRINTF("/dev/xen/blktap-2/tapdev%d", tap.minor);
        if (devname)
            return devname;
    }

    params = GCSPRINTF("%s:%s", type, disk);
    err = tap_ctl_create(params, &devname);
    if (!err) {
        libxl__ptr_add(gc, devname);
        return devname;
    }

    free(devname);
    return NULL;
}

static bool tapdev_is_shared(libxl__gc *gc, const char *params)
{
    char **domids, **vbds;
    char *tp;
    unsigned int count1, count2, i, j;

    /* List all the domids that have vhd backends */
    domids = libxl__xs_directory(gc, XBT_NULL, "backend/vbd", &count1);
    if (domids) {
        for (i = 0; i < count1; ++i) {
            /* List all the vbds for that domid */
            vbds = libxl__xs_directory(gc, XBT_NULL, libxl__sprintf(gc, "backend/vbd/%s", domids[i]), &count2);
            if (vbds) {
                for (j = 0; j < count2; ++j) {
                    /* If the params are the same, we have a match */
                    tp = libxl__xs_read(gc, XBT_NULL, libxl__sprintf(gc, "backend/vbd/%s/%s/tapdisk-params", domids[i], vbds[j]));
                    if (tp != NULL && !strcmp(tp, params))
                        return true;
                }
            }
        }
    }

    return false;
}

int libxl__device_destroy_tapdisk(libxl__gc *gc, const char *params, uint32_t domid)
{
    char *type, *disk;
    int err;
    tap_list_t tap;

    /* We're using the tapdev. If anybody else also is, we can't destroy it! */
    if (tapdev_is_shared(gc, params)) {
        LOG(DEBUG, "Not destroying tapdev%d, another VM uses it", tap.minor);
        return 0;
    }

    type = libxl__strdup(gc, params);

    disk = strchr(type, ':');
    if (!disk) {
        LOG(ERROR, "Unable to parse params %s", params);
        return ERROR_INVAL;
    }

    *disk++ = '\0';

    err = tap_ctl_find(type, disk, &tap);
    if (err < 0) {
        /* returns -errno */
        LOGEV(ERROR, -err, "Unable to find type %s disk %s", type, disk);
        return ERROR_FAIL;
    }

    err = tap_ctl_destroy(tap.id, tap.minor);
    if (err < 0) {
        LOGEV(ERROR, -err, "Failed to destroy tap device id %d minor %d",
              tap.id, tap.minor);
        return ERROR_FAIL;
    }

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
