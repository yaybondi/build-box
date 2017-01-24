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
        "Build Box NG Management Utility, Version %s                            \n"
        "Copyright (c) 2017 Tobias Koch <tobias.koch@nonterra.com>              \n"
        "                                                                       \n"
        "Usage: bbox-do login [OPTIONS] <target>                                \n"
        "                                                                       \n"
        "OPTIONS:                                                               \n"
        "                                                                       \n"
        " -h,--help            Print this help message and exit immediately.    \n"
        "                                                                       \n"
        " -t,--targets <dir>   Search for targets in the given directory. The   \n"
        "                      default location is '~/.bolt/targets'.           \n"
        "                                                                       \n"
        " -n,--no-mount        Do not bind mount /home and special filesystems. \n"
        "                      This does *not* unmount previously mounted       \n"
        "                      file systems. Use 'bbox-do umount' for that.     \n"
        "                                                                       \n",
        BBOX_VERSION
    );
}

int bbox_login_getopt(bbox_conf_t *conf, int argc, char * const argv[])
{
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"targets",  required_argument, 0, 't'},
        {"no-mount", no_argument,       0, 'n'},
        { 0,         0,                 0,  0 }
    };

    bbox_config_set_mount_all(conf);
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

int bbox_login_chroot(char *sys_root, char *home_dir, uid_t uid)
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    char *sh = NULL;
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;

    if(lstat(sys_root, &st) == -1) {
        bbox_perror("login", "failed to stat '%s': %s.\n", sys_root,
                strerror(errno));
        return BBOX_ERR_RUNTIME;
    }
    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("login", "'%s' is not a directory.\n", sys_root);
        return BBOX_ERR_RUNTIME;
    }
    if(st.st_uid != uid) {
        bbox_perror("login", "directory '%s' is not owned by user.\n",
                sys_root);
        return BBOX_ERR_RUNTIME;
    }

    for(int i = 0; (sh = shells[i]) != NULL; i++) {
        bbox_path_join(&buf, sys_root, sh, &buf_len);

        if(lstat(buf, &st) == 0)
            break;
        else
            sh = NULL;
    }

    free(buf);

    if(!sh) {
        bbox_perror("login", "could not find a shell.\n");
        return BBOX_ERR_RUNTIME;
    }

    if(chdir(sys_root) == -1) {
        bbox_perror("login", "chdir to system root failed: %s.\n",
                strerror(errno));
        return BBOX_ERR_RUNTIME;
    }
    if(chroot(".") == -1) {
        bbox_perror("login", "chroot to system root failed: %s.\n",
                strerror(errno));
        return BBOX_ERR_RUNTIME;
    }
    if(setuid(uid) == -1) {
        bbox_perror("login", "could not drop privileges: %s.\n",
                strerror(errno));
        return BBOX_ERR_RUNTIME;
    }

    if(home_dir)
        chdir(home_dir);

    execlp(sh, sh, "-l", (char*) NULL);
    bbox_perror("login", "failed to invoke shell: %s.\n", strerror(errno));
    return BBOX_ERR_RUNTIME;
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

    /* illusion: getopt itself is not thread-safe, probably. */
    if((non_optind = bbox_login_getopt(conf, argc, argv)) < 0) {
        bbox_config_free(conf);

        if(non_optind < -1) {
            return BBOX_ERR_INVOCATION;
        } else {
            /* user asked for --help */
            return 0;
        }
    }

    if(non_optind >= argc) {
        bbox_perror("login", "no target specified.\n");
        bbox_config_free(conf);
        return BBOX_ERR_INVOCATION;
    }

    char *target = argv[non_optind];
    bbox_path_join(&buf, bbox_config_get_target_dir(conf), target, &buf_len);

    if(bbox_config_get_mount_any(conf)) {
        if(bbox_mount_any(conf, buf) == -1) {
            free(buf);
            bbox_config_free(conf);
            return BBOX_ERR_RUNTIME;
        }
    }

    int rval = bbox_login_chroot(buf, bbox_config_get_home_dir(conf), getuid());

    bbox_config_free(conf);
    free(buf);
    return rval;
}

