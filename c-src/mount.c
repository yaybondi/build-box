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
#include <limits.h>
#include <stdio.h>
#include <mntent.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_mount_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                             \n"
        "Copyright (c) 2017-2020 Tobias Koch <tobias.koch@gmail.com>             \n"
        "                                                                        \n"
        "Usage: build-box-do mount [OPTIONS] <target-name>                       \n"
        "                                                                        \n"
        "OPTIONS:                                                                \n"
        "                                                                        \n"
        " -h,--help             Print this help message and exit immediately.    \n"
        "                                                                        \n"
        " -t,--targets <dir>    Search for targets in the given directory. The   \n"
        "                       default location is '~/.bolt/targets'.           \n"
        "                                                                        \n"
        " -w,--workspace <dir>  Use the given directory as the workspace instead \n"
        "                       of the default '~/BuildBox'.                     \n"
        "                                                                        \n"
        " -m,--mount <fstype>   Mount 'dev', 'proc', 'sys' or 'home'. For the    \n"
        "                       'mount' command, if this option is not specified,\n"
        "                       then the default is to mount all of them.        \n"
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
        {"help",      no_argument,       0, 'h'},
        {"targets",   required_argument, 0, 't'},
        {"workspace", required_argument, 0, 'w'},
        {"mount",     required_argument, 0, 'm'},
        { 0,          0,                 0,  0 }
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
            case 'w':
               if(bbox_config_set_workspace(conf, optarg) < 0)
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

    /*
     * Make sure we use the normalized path to compare against the entries in
     * /proc/mounts.
     */
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
        bbox_perror("mount", "out of memory?\n");
        rval = -1;
        goto cleanup_and_exit;
    }

    /*
     * Loop over the entries in /proc/mounts and compare against the given
     * directory.
     */
    while(1)
    {
        struct mntent *tmp_info = getmntent_r(fp, &info, buf, buf_len);

        if(!tmp_info) {
            if(errno == ERANGE) {
                buf_len *= 2;
                buf = realloc(buf, buf_len);
                if(!buf) {
                    bbox_perror("mount", "out of memory?\n");
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

int bbox_mount_verify_workspace(const char *workspace)
{
    struct stat st;

    /*
     * If if doesn't seem to exist, create it.
     */
    if(stat(workspace, &st) == -1) {
        if(bbox_mkdir_p("mount", workspace) == -1)
            return -1;
    }

    /*
     * The workspace needs to belong to the user.
     */
    if(bbox_isdir_and_owned_by("mount", workspace, getuid()) == -1)
        return -1;

    return 0;
}

int bbox_mount_bind(const char *sys_root, const char *source,
        const char *mount_point, int recursive)
{
    char *target = NULL;
    size_t buf_len = 0;
    struct stat st;
    int is_mounted = 0;

    if(!source)
        source = mount_point;

    bbox_path_join(&target, sys_root, mount_point, &buf_len);

    if((is_mounted = bbox_mount_is_mounted(target)) == -1) {
        free(target);
        return -1;
    }

    if(is_mounted) {
        free(target);
        return 0;
    }

    /*
     * Require that the normalized mountpoint is a directory owned by the user
     * who invoked `build-box` to mitigate the risk of misuse.
     */
    if(bbox_isdir_and_owned_by("mount", target, getuid()) == -1) {
        free(target);
        return -1;
    }

    int rval = 0;

    /*
     * We need to be running mount as root, so we briefly raise privileges to
     * drop them again immediately after.
     */
    if(bbox_raise_privileges() == -1) {
        free(target);
        return -1;
    }

    unsigned long mountflags = MS_BIND | (recursive ? MS_REC : 0);

    if(mount(source, target, NULL, mountflags, NULL) != 0)
    {
        bbox_perror("mount", "failed to mount %s on %s: %s.\n",
                source, target, strerror(errno));
        rval = -1;
    }
    else if(mount(NULL, target, NULL, MS_PRIVATE, NULL) != 0)
    {
        bbox_perror("mount", "failed to make mountpoint %s private: %s.\n",
                target, strerror(errno));
        /* Continue anyway. */
    }

    /*
     * We're done with mounting, lower privileges right away.
     */
    if(bbox_lower_privileges() == -1)
        rval = -1;

    free(target);
    return rval;
}

int bbox_mount_any(const bbox_conf_t *conf, const char *sys_root)
{
    /*
     * As an additional precaution, we require the normalized sys-root directory
     * to be owned by the user who invoked `build-box`.
     */
    if(bbox_isdir_and_owned_by("mount", sys_root, getuid()) == -1)
        return -1;

    if(bbox_config_get_mount_dev(conf)) {
        if(bbox_mount_bind(sys_root, NULL, "/dev", 0) < 0)
            return -1;
    }

    if(bbox_config_get_mount_proc(conf)) {
        if(bbox_mount_bind(sys_root, NULL, "/proc", 0) < 0)
            return -1;
    }

    if(bbox_config_get_mount_sys(conf)) {
        if(bbox_mount_bind(sys_root, NULL, "/sys", 0) < 0)
            return -1;
    }

    /*
     * Mounting the home directory requires extra pre-caution. The source path
     * has already been normalized and checked for ownership, so we should be
     * fine calling `bbox_mount_bind`, which in turn checks the target directory
     * before executing the mount.
     */
    if(bbox_config_get_mount_home(conf)) {
        const char *homedir = bbox_config_get_home_dir(conf);

        /*
         * We're not worried about this, because we are currently running with
         * lowered privileges.
         */
        if(bbox_sysroot_mkdir_p("mount", sys_root, homedir) == -1)
            return -1;

        char *workspace = bbox_config_get_workspace(conf);

        /*
         * Verify that workspace exists (or create it) and that it belongs to
         * the user.
         */
        if(bbox_mount_verify_workspace(workspace) == -1)
            return -1;

        /*
         * This internally checks the ownership of <sys_root>/<homedir>.
         */
        if(bbox_mount_bind(sys_root, workspace, homedir, 0) < 0)
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
