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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_run_usage()
{
    printf(
        "                                                                        \n"
        "USAGE:                                                                  \n"
        "                                                                        \n"
        "  build-box run [OPTIONS] <target-name> -- <command>                    \n"
        "                                                                        \n"
        "OPTIONS:                                                                \n"
        "                                                                        \n"
        "  -h, --help           Print this help message and exit immediately.    \n"
        "                                                                        \n"
        "  -n, --no-mount       Don't bind mount homedir and special filesystems.\n"
        "                       This does *not* unmount previously mounted       \n"
        "                       file systems. Use 'bbox-do umount' for that.     \n"
        "                                                                        \n"
        "  --no-file-copy       Don't copy /etc/passwd, group and resolv.conf    \n"
        "                       from host.                                       \n"
        "                                                                        \n"
    );
}

int bbox_run_getopt(bbox_conf_t *conf, int argc, char * const argv[])
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

    bbox_config_set_mount_all(conf);
    bbox_config_enable_file_updates(conf);
    optind = 1;

    while(1) {
        c = getopt_long(argc, argv, ":ht:n", long_options, &option_index);

        if(c == -1)
            break;

        switch(c) {
            case 'h':
                bbox_run_usage();
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
            case ':':
                bbox_run_usage();
                return -2;
            default:
                /* impossible, ignore */
                break;
        }
    }

    return optind;
}

int bbox_run(int argc, char * const argv[])
{
    char *buf = NULL;
    size_t buf_len = 0;

    int rval = BBOX_ERR_INVOCATION;

    bbox_conf_t *conf = bbox_config_new();
    if(!conf) {
        bbox_perror("run", "creating configuration context failed.");
        return BBOX_ERR_RUNTIME;
    }

    int non_optind;

    if((non_optind = bbox_run_getopt(conf, argc, argv)) < 0) {
        /* user asked for --help */
        if(non_optind == -1)
            rval = 0;
        goto cleanup_and_exit;
    }

    if(non_optind >= argc) {
        bbox_perror("run", "no target specified.\n");
        goto cleanup_and_exit;
    }

    char *target = argv[non_optind++];

    if(validate_target_name("run", target) == -1)
        goto cleanup_and_exit;

    bbox_path_join(
        &buf, bbox_config_get_target_dir(conf), target, &buf_len
    );

    struct stat st;

    if(lstat(buf, &st) == -1) {
        bbox_perror("login", "target '%s' not found.\n", target);
        goto cleanup_and_exit;
    }

    rval = BBOX_ERR_RUNTIME;

    /*
     * Mount special directories and home if configured (default).
     */
    if(bbox_mount_any(conf, buf) == -1)
        goto cleanup_and_exit;

    /*
     * We're not worried about this block, because we're currently running with
     * lowered privileges.
     */
    if(bbox_config_do_file_updates(conf))
        bbox_update_chroot_dynamic_config(buf);

    /*
     * We clean out most of the environment except for variables starting with
     * BOLT_ and a few select, such as CFLAGS. Then we log into the target and
     * execute what's left on the command line.
     */
    bbox_sanitize_environment();

    if(bbox_runas_user_chrooted(buf, bbox_config_get_home_dir(conf),
            argc-non_optind, &argv[non_optind]) == 0)
        rval = 0;

cleanup_and_exit:

    bbox_config_free(conf);
    free(buf);
    return rval;
}
