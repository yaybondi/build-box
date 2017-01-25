/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Nonterra Software Solutions
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

void bbox_mount_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                             \n"
        "Copyright (c) 2017 Tobias Koch <tobias.koch@nonterra.com>               \n"
        "                                                                        \n"
        "Usage: bbox-do mount [OPTIONS] <target>                                 \n"
        "                                                                        \n"
        "OPTIONS:                                                                \n"
        "                                                                        \n"
        " -h,--help            Print this help message and exit immediately.     \n"
        "                                                                        \n"
        " -t,--targets <dir>   Search for targets in the given directory. The    \n"
        "                      default location is '~/.bolt/targets'.            \n"
        "                                                                        \n"
        " -m,--mount <fstype>  Mount 'dev', 'proc', 'sys' or 'home'. For the     \n"
        "                      'mount' command, if this option is not specified, \n"
        "                      then the default is to mount all of them.         \n"
        "                                                                        \n",
        BBOX_VERSION
    );
}

int bbox_mount_getopt(bbox_conf_t *conf, int argc, char * const argv[])
{
    int c;
    int option_index = 0;
    int do_mount_all = 1;

    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"targets",  required_argument, 0, 't'},
        {"mount",    required_argument, 0, 'm'},
        { 0,         0,                 0,  0 }
    };

    bbox_config_clear_mount(conf);
    optind = 1;

    while(1) {
        c = getopt_long(argc, argv, ":ht:m:", long_options, &option_index);

        if(c == -1)
            break;

        switch(c) {
            case 'h':
                bbox_mount_usage();
                return -1;
            case 't':
                if(bbox_config_set_target_dir(conf, optarg) < 0)
                    return -2;
                break;
            case 'm':
                do_mount_all = 0;

                if(!strcmp(optarg, "dev")) {
                    bbox_config_set_mount_dev(conf);
                } else if(!strcmp(optarg, "proc")) {
                    bbox_config_set_mount_proc(conf);
                } else if(!strcmp(optarg, "sys")) {
                    bbox_config_set_mount_sys(conf);
                } else if(!strcmp(optarg, "home")) {
                    bbox_config_set_mount_home(conf);
                } else {
                    bbox_perror("mount", "unknown file system specifier "
                            "'%s'.\n", optarg);
                    return -2;
                }

                break;
            case '?':
                bbox_perror("mount", "unknown option '%s'.\n", argv[optind-1]);
                return -2;
            case ':':
                bbox_perror("mount", "option '%s' needs an argument.\n",
                        argv[optind-1]);
                return -2;
            default:
                /* impossible, ignore */
                break;
        }
    }

    if(do_mount_all)
        bbox_config_set_mount_all(conf);

    return optind;
}

int bbox_mount_is_mounted(const char *path)
{
    struct mntent info;
    size_t buf_len = 4096;
    char *buf = NULL;
    char *mount_point = NULL;
    int rval = 0;
    FILE *fp = NULL;

    if(!(mount_point = realpath(path, NULL))) {
        bbox_perror("mount", "could not resolve '%s': '%s'.\n",
                path, strerror(errno));
        rval = -1;
        goto cleanup_and_exit;
    }

    if(!(fp = setmntent("/proc/mounts", "re"))) {
        bbox_perror("mount", "failed to open /proc/mounts.\n");
        rval = -1;
        goto cleanup_and_exit;
    }

    if(!(buf = malloc(buf_len))) {
        bbox_perror("mount", "out of memory? %s.\n", strerror(errno));
        rval = -1;
        goto cleanup_and_exit;
    }

    while(1)
    {
        struct mntent *tmp_info = getmntent_r(fp, &info, buf, buf_len);

        if(!tmp_info) {
            if(errno == ERANGE) {
                buf_len *= 2;
                buf = realloc(buf, buf_len);
                if(!buf) {
                    bbox_perror("mount", "out of memory? %s.\n",
                            strerror(errno));
                    rval = -1;
                    goto cleanup_and_exit;
                }
                continue;
            }

            break;
        }

        if(!strcmp(tmp_info->mnt_dir, mount_point)) {
            rval = 1;
            break;
        }
    }

cleanup_and_exit:

    if(fp)
        endmntent(fp);
    free(mount_point);
    free(buf);

    return rval;
}

int bbox_mount_bind(const char *sys_root, const char *mount_point)
{
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;
    int is_mounted = 0;

    bbox_path_join(&buf, sys_root, mount_point, &buf_len);

    if(lstat(buf, &st) < 0) {
        bbox_perror("mount", "could not stat '%s': %s.\n", buf,
                strerror(errno));
        free(buf);
        return -1;
    }

    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("mount", "%s is not a directory.\n", buf);
        free(buf);
        return -1;
    }

    if((is_mounted = bbox_mount_is_mounted(buf)) == -1) {
        free(buf);
        return -1;
    }

    if(is_mounted)
        return 0;

    char *out_buf = NULL;
    size_t out_buf_len = 0;
    char * const argv[] = 
        {"mount", "-o", "bind", (char*const) mount_point, buf, NULL};
    int rval = 0;

    if(bbox_runas_fetch_output(0, "mount", argv, &out_buf, &out_buf_len) != 0) {
        if(out_buf) {
            bbox_perror("mount", "failed to mount %s: \"%s\".\n",
                    mount_point, out_buf);
        }
        rval = -1;
    }

    free(buf);
    free(out_buf);
    return rval;
}

int bbox_mount_any(const bbox_conf_t *conf, const char *sys_root)
{
    struct stat st;

    if(lstat(sys_root, &st) < 0) {
        bbox_perror("mount", "could not stat '%s': %s.\n", sys_root,
                strerror(errno));
        return -1;
    }
    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("mount", "'%s' is not a directory.\n", sys_root);
        return -1;
    }
    if(st.st_uid != getuid()) {
        bbox_perror("mount", "directory '%s' is not owned by user.\n",
                sys_root);
        return -1;
    }

    if(bbox_config_get_mount_dev(conf)) {
        if(bbox_mount_bind(sys_root, "/dev") < 0)
            return -1;
    }

    if(bbox_config_get_mount_proc(conf)) {
        if(bbox_mount_bind(sys_root, "/proc") < 0)
            return -1;
    }

    if(bbox_config_get_mount_sys(conf)) {
        if(bbox_mount_bind(sys_root, "/sys") < 0)
            return -1;
    }

    if(bbox_config_get_mount_home(conf)) {
        if(bbox_mount_bind(sys_root, "/home") < 0)
            return -1;
    }

    return 0;
}

int bbox_mount(int argc, char * const argv[])
{
    char *buf = NULL;
    size_t buf_len = 0;
    int non_optind;
    int rval = 0;

    bbox_conf_t *conf = bbox_config_new();
    if(!conf) {
        bbox_perror("mount", "creating configuration context failed.");
        return BBOX_ERR_RUNTIME;
    }

    /* illusion: getopt itself is not thread-safe, probably. */
    if((non_optind = bbox_mount_getopt(conf, argc, argv)) < 0) {
        bbox_config_free(conf);

        if(non_optind < -1) {
            return BBOX_ERR_INVOCATION;
        } else {
            /* user asked for --help */
            return 0;
        }
    }

    if(non_optind >= argc) {
        bbox_perror("mount", "no target specified.\n");
        bbox_config_free(conf);
        return BBOX_ERR_INVOCATION;
    }

    char *target = argv[non_optind];
    bbox_path_join(&buf, bbox_config_get_target_dir(conf), target, &buf_len);

    if(bbox_mount_any(conf, buf) < 0)
        rval = BBOX_ERR_RUNTIME;

    bbox_config_free(conf);
    free(buf);
    return rval;
}

