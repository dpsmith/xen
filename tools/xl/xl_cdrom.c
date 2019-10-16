/*
 * Copyright 2009-2017 Citrix Ltd and other contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libxl.h>
#include <libxl_utils.h>
#include <libxlutil.h>

#include "xl.h"
#include "xl_utils.h"
#include "xl_parse.h"

static int cd_insert(uint32_t domid, const char *virtdev, char *phys)
{
    libxl_device_disk disk;
    char *buf = NULL;
    XLU_Config *config = 0;
    struct stat b;
    int r;
    uint32_t stubdomid = 0;
    int i, nb, devid = -1;
    libxl_device_disk *disks = NULL;
    libxl_device_disk olddisk;
    libxl_diskinfo diskinfo;
    char *dev = NULL;
    char *strdevid = NULL;
    char *xspath = NULL;

    memset(&diskinfo, 0, sizeof(libxl_diskinfo));
    memset(&olddisk, 0, sizeof(libxl_device_disk));

    xasprintf(&buf, "vdev=%s,access=r,devtype=cdrom,target=%s",
              virtdev, phys ? phys : "");

    parse_disk_config(&config, buf, &disk);

    stubdomid = libxl_get_stubdom_id(ctx, domid);

    /* If stubdom, protocol changes slightly. Retap new iso in dom0,
     * send qmp message to stubdom to change cdrom medium using blkfront
     * target */
    if (stubdomid > 0) {
        disks = libxl_device_disk_list(ctx, stubdomid, &nb);
        if (disks) {
            for (i=0; i<nb; i++) {
                if (!libxl_device_disk_getinfo(ctx, stubdomid, &disks[i], &diskinfo)) {
                    xasprintf(&xspath, "%s/dev", diskinfo.backend);
                    if (!xspath) {
                        r = 0;
                        goto out;
                    }
                    libxl_util_xs_read(ctx, xspath, &dev);
                    if (!dev) {
                        r = 0;
                        goto out;
                    }
                    if (!strcmp(dev, "hdc"))
                        devid = diskinfo.devid;
                    libxl_diskinfo_dispose(&diskinfo);
                }
                libxl_device_disk_dispose(&disks[i]);
            }
            free(disks);
        }
        xasprintf(&strdevid, "%d", devid);

        libxl_vdev_to_device_disk(ctx, stubdomid, strdevid, &olddisk);

        libxl_cdrom_change(ctx, domid, phys, &olddisk, strdevid, NULL);

    } else {
        /* ATM the existence of the backing file is not checked for qdisk
         * in libxl_cdrom_insert() because RAW is used for remote
         * protocols as well as plain files.  This will ideally be changed
         * for 4.4, but this work-around fixes the problem of "cd-insert"
         * returning success for non-existent files. */
        if (disk.format != LIBXL_DISK_FORMAT_EMPTY
            && stat(disk.pdev_path, &b)) {
            fprintf(stderr, "Cannot stat file: %s\n",
                    disk.pdev_path);
            r = 1;
            goto out;
        }

        if (libxl_cdrom_insert(ctx, domid, &disk, NULL)) {
            r = 1;
            goto out;
        }
    }
    r = 0;
out:
    libxl_device_disk_dispose(&disk);
    free(buf);
    if (dev)
        free(dev);
    if (strdevid)
        free(strdevid);
    if (xspath)
        free(xspath);
    return r;
}

int main_cd_eject(int argc, char **argv)
{
    uint32_t domid;
    int opt = 0;
    const char *virtdev;

    SWITCH_FOREACH_OPT(opt, "", NULL, "cd-eject", 2) {
        /* No options */
    }

    domid = find_domain(argv[optind]);
    virtdev = argv[optind + 1];

    if (cd_insert(domid, virtdev, NULL))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int main_cd_insert(int argc, char **argv)
{
    uint32_t domid;
    int opt = 0;
    const char *virtdev;
    char *file = NULL; /* modified by cd_insert tokenising it */

    SWITCH_FOREACH_OPT(opt, "", NULL, "cd-insert", 3) {
        /* No options */
    }

    domid = find_domain(argv[optind]);
    virtdev = argv[optind + 1];
    file = argv[optind + 2];

    if (cd_insert(domid, virtdev, file))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
