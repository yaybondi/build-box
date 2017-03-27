/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Tobias Koch <tobias.koch@nonterra.com>
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
#include <limits.h>
#include <stdio.h>
#include <mntent.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_umount_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                             \n"
        "Copyright (c) 2017 Tobias Koch <tobias.koch@nonterra.com>               \n"
        "                                                                        \n"
        "Usage: bbox-do umount [OPTIONS] <target>                                \n"
        "                                                                        \n"
        "OPTIONS:                                                                \n"
        "                                                                        \n"
        " -h,--help            Print this help message and exit immediately.     \n"
        "                                                                        \n"
        " -t,--targets <dir>   Search for targets in the given directory. The    \n"
        "                      default location is '~/.bolt/targets'.            \n"
        "                                                                        \n"
        " -u,--umount <fstype> Mount 'dev', 'proc', 'sys' or 'home'. For the     \n"
        "                      'mount' command, if this option is not specified, \n"
        "                      then the default is to umount all of them.        \n"
        "                                                                        \n",
        BBOX_VERSION
    );
}

int bbox_umount_getopt(bbox_conf_t *conf, int argc, char * const argv[])
{
    int c;
    int option_index = 0;
    int do_umount_all = 1;

    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"targets",  required_argument, 0, 't'},
        {"umount",   required_argument, 0, 'u'},
        { 0,         0,                 0,  0 }
    };

    bbox_config_set_mount_all(conf);
    optind = 1;

    while(1) {
        c = getopt_long(argc, argv, ":ht:u:", long_options, &option_index);

        if(c == -1)
            break;

        switch(c) {
            case 'h':
                bbox_umount_usage();
                return -1;
            case 't':
                if(bbox_config_set_target_dir(conf, optarg) < 0)
                    return -2;
                break;
            case 'u':
                do_umount_all = 0;

                if(!strcmp(optarg, "dev")) {
                    bbox_config_unset_mount_dev(conf);
                } else if(!strcmp(optarg, "proc")) {
                    bbox_config_unset_mount_proc(conf);
                } else if(!strcmp(optarg, "sys")) {
                    bbox_config_unset_mount_sys(conf);
                } else if(!strcmp(optarg, "home")) {
                    bbox_config_unset_mount_home(conf);
                } else {
                    bbox_perror("umount", "unknown file system specifier "
                            "'%s'.\n", optarg);
                    return -2;
                }

                break;
            case '?':
                bbox_perror("umount", "unknown option '%s'.\n", argv[optind-1]);
                return -2;
            case ':':
                bbox_perror("umount", "option '%s' needs an argument.\n",
                        argv[optind-1]);
                return -2;
            default:
                /* impossible, ignore */
                break;
        }
    }

    if(do_umount_all)
        bbox_config_clear_mount(conf);

    return optind;
}

int bbox_umount_unbind(const char *sys_root, const char *mount_point)
{
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;
    int is_mounted = 0;

    bbox_path_join(&buf, sys_root, mount_point, &buf_len);

    if(lstat(buf, &st) < 0) {
        bbox_perror("umount", "could not stat '%s': %s.\n", buf,
                strerror(errno));
        free(buf);
        return -1;
    }

    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("umount", "%s is not a directory.\n", buf);
        free(buf);
        return -1;
    }

    if((is_mounted = bbox_mount_is_mounted(buf)) == -1) {
        free(buf);
        return -1;
    }

    if(!is_mounted)
        return 0;

    char *out_buf = NULL;
    size_t out_buf_len = 0;
    char * const argv[] = {"umount", buf, NULL};
    int rval = 0;

    if(bbox_runas_fetch_output(0, "umount", argv, &out_buf,
                &out_buf_len) != 0)
    {
        if(out_buf) {
            bbox_perror("umount", "failed to unmount %s: \"%s\".\n",
                    mount_point, out_buf);
        }
        rval = -1;
    }

    free(buf);
    free(out_buf);
    return rval;
}

int bbox_umount_any(const bbox_conf_t *conf, const char *sys_root)
{
    struct stat st;

    if(lstat(sys_root, &st) < 0) {
        bbox_perror("umount", "could not stat '%s': %s.\n", sys_root,
                strerror(errno));
        return -1;
    }
    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("umount", "'%s' is not a directory.\n", sys_root);
        return -1;
    }
    if(st.st_uid != getuid()) {
        bbox_perror("umount", "directory '%s' is not owned by user.\n",
                sys_root);
        return -1;
    }

    if(!bbox_config_get_mount_dev(conf)) {
        if(bbox_umount_unbind(sys_root, "/dev") < 0)
            return -1;
    }

    if(!bbox_config_get_mount_proc(conf)) {
        if(bbox_umount_unbind(sys_root, "/proc") < 0)
            return -1;
    }

    if(!bbox_config_get_mount_sys(conf)) {
        if(bbox_umount_unbind(sys_root, "/sys") < 0)
            return -1;
    }

    if(!bbox_config_get_mount_home(conf)) {
        if(bbox_umount_unbind(sys_root, "/home") < 0)
            return -1;
    }

    return 0;
}

int bbox_umount(int argc, char * const argv[])
{
    char *buf = NULL;
    size_t buf_len = 0;
    int non_optind;
    int rval = 0;

    bbox_conf_t *conf = bbox_config_new();
    if(!conf) {
        bbox_perror("umount", "creating configuration context failed.");
        return BBOX_ERR_RUNTIME;
    }

    /* illusion: getopt itself is not thread-safe, probably. */
    if((non_optind = bbox_umount_getopt(conf, argc, argv)) < 0) {
        bbox_config_free(conf);

        if(non_optind < -1) {
            return BBOX_ERR_INVOCATION;
        } else {
            /* user asked for --help */
            return 0;
        }
    }

    if(non_optind >= argc) {
        bbox_perror("umount", "no target specified.\n");
        bbox_config_free(conf);
        return BBOX_ERR_INVOCATION;
    }

    char *target = argv[non_optind];
    bbox_path_join(&buf, bbox_config_get_target_dir(conf), target, &buf_len);

    if(bbox_umount_any(conf, buf) < 0)
        rval = BBOX_ERR_RUNTIME;

    bbox_config_free(conf);
    free(buf);
    return rval;
}

