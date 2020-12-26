/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Tobias Koch <tobias.koch@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_login_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                             \n"
        "Copyright (c) 2017-2020 Tobias Koch <tobias.koch@gmail.com>             \n"
        "                                                                        \n"
        "Usage: build-box login [OPTIONS] <target-name>                          \n"
        "                                                                        \n"
        "OPTIONS:                                                                \n"
        "                                                                        \n"
        " -h,--help             Print this help message and exit immediately.    \n"
        "                                                                        \n"
        " -n,--no-mount         Don't bind mount homedir and special filesystems.\n"
        "                       This does *not* unmount previously mounted       \n"
        "                       file systems. Use 'bbox-do umount' for that.     \n"
        "                                                                        \n"
        " --no-file-copy        Don't copy /etc/passwd, group and resolv.conf    \n"
        "                       from host.                                       \n"
        "                                                                        \n",
        BBOX_VERSION
    );
}

int bbox_login_getopt(bbox_conf_t *conf, int argc, char * const argv[])
{
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",         no_argument,       0, 'h'},
        {"targets",      required_argument, 0, 't'},
        {"no-mount",     no_argument,       0, 'n'},
        {"no-file-copy", no_argument,       0, '1'},
        { 0,             0,                 0,  0 }
    };

    /*
     * Per default we turn on mounting of /proc /sys /dev, home and enable
     * the copying of passwd, group and hosts files from the host.
     */
    bbox_config_set_mount_all(conf);
    bbox_config_enable_file_updates(conf);
    optind = 1;

    while(1) {
        c = getopt_long(argc, argv, ":ht:n", long_options, &option_index);

        if(c == -1)
            break;

        switch(c) {
            case 'h':
                bbox_login_usage();
                return -1;
            case 't':
                if(bbox_config_set_target_dir(conf, optarg) == -1)
                    return -2;
                break;
            case 'n':
                bbox_config_clear_mount(conf);
                break;
            case '1':
                bbox_config_disable_file_updates(conf);
                break;
            case '?':
                bbox_perror("login", "unknown option '%s'.\n", argv[optind-1]);
                return -2;
            case ':':
                bbox_perror("login", "option '%s' needs an argument.\n",
                        argv[optind-1]);
                return -2;
            default:
                /* impossible, ignore */
                break;
        }
    }

    return optind;
}

int bbox_login(int argc, char * const argv[])
{
    char *buf = NULL;
    size_t buf_len = 0;
    int non_optind;

    bbox_conf_t *conf = bbox_config_new();
    if(!conf) {
        bbox_perror("login", "creating configuration context failed.");
        return BBOX_ERR_RUNTIME;
    }

    if((non_optind = bbox_login_getopt(conf, argc, argv)) < 0) {
        bbox_config_free(conf);

        if(non_optind < -1) {
            return BBOX_ERR_INVOCATION;
        } else {
            /* user asked for --help */
            return 0;
        }
    }

    /*
     * TODO: we're currently ignoring extra arguments on the command line. This
     *       should probably be an error instead.
     */
    if(non_optind >= argc) {
        bbox_perror("login", "no target specified.\n");
        bbox_config_free(conf);
        return BBOX_ERR_INVOCATION;
    }

    char *target = argv[non_optind];
    bbox_path_join(&buf, bbox_config_get_target_dir(conf), target, &buf_len);

    /*
     * Mount special directories and home if configured (default).
     */
    if(bbox_config_get_mount_any(conf)) {
        if(bbox_mount_any(conf, buf) == -1) {
            free(buf);
            bbox_config_free(conf);
            return BBOX_ERR_RUNTIME;
        }
    }

    /*
     * Copy passwd, group and hosts information from the host to the target.
     */
    if(bbox_config_do_file_updates(conf))
        bbox_update_chroot_dynamic_config(buf);

    /*
     * We clean out most of the environment except for variables starting with
     * BOLT_ and a few select, such as CFLAGS. Then we log into the target and
     * change into the home directory.
     */
    bbox_sanitize_environment();
    int rval = bbox_login_sh_chrooted(buf, bbox_config_get_home_dir(conf));

    bbox_config_free(conf);
    free(buf);

    if(rval == -1)
        return BBOX_ERR_RUNTIME;
    else
        return rval;
}
