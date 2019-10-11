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
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_list_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                            \n"
        "Copyright (c) 2017 Tobias Koch <tobias.koch@gmail.com>                 \n"
        "                                                                       \n"
        "Usage: build-box-do list [OPTIONS]                                     \n"
        "                                                                       \n"
        "OPTIONS:                                                               \n"
        "                                                                       \n"
        " -h,--help            Print this help message and exit immediately.    \n"
        "                                                                       \n"
        " -t,--targets <dir>   Search for targets in the given directory. The   \n"
        "                      default location is '~/.bolt/targets'.           \n"
        "                                                                       \n",
        BBOX_VERSION
    );
}

int bbox_list_getopt(bbox_conf_t *conf, int argc, char * const argv[])
{
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"targets",  required_argument, 0, 't'},
        { 0,         0,                 0,  0 }
    };

    optind = 1;

    while(1) {
        c = getopt_long(argc, argv, ":ht:n", long_options, &option_index);

        if(c == -1)
            break;

        switch(c) {
            case 'h':
                bbox_list_usage();
                return -1;
            case 't':
                if(bbox_config_set_target_dir(conf, optarg) == -1)
                    return -2;
                break;
            case '?':
                bbox_perror("list", "unknown option '%s'.\n", argv[optind-1]);
                return -2;
            case ':':
                bbox_perror("list", "option '%s' needs an argument.\n",
                        argv[optind-1]);
                return -2;
            default:
                /* impossible, ignore */
                break;
        }
    }

    return optind;
}

int bbox_list_targets(const bbox_conf_t *conf)
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    DIR *dp = NULL;
    struct stat st;
    struct dirent *ep;
    char *buf1 = NULL;
    size_t buf1_len = 0;
    char *buf2 = NULL;
    size_t buf2_len = 0;
    char *sh = NULL;

    const char *target_dir = bbox_config_get_target_dir(conf);

    if(lstat(target_dir, &st) == -1) {
        bbox_perror("list", "could not stat '%s': %s.\n", target_dir,
                strerror(errno));
        return -1;
    }
    if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        bbox_perror("list", "'%s' is not a directory.\n", target_dir);
        return -1;
    }

    dp = opendir(target_dir);
    if(!dp) {
        bbox_perror("list", "failed to read directory '%s': %s.\n",
                target_dir, strerror(errno));
        return -1;
    }

    while((ep = readdir(dp)) != NULL) {
        if(!strcmp(".", ep->d_name) || !strcmp("..", ep->d_name))
            continue;

        bbox_path_join(&buf1, target_dir, ep->d_name, &buf1_len);

        if(lstat(buf1, &st) == -1)
            continue;
        if(S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode))
            continue;

        /* check that there is a shell. */
        for(int i = 0; (sh = shells[i]) != NULL; i++) {
            bbox_path_join(&buf2, buf1, sh, &buf2_len);

            if(lstat(buf2, &st) == 0)
                break;
            else
                sh = NULL;
        }

        if(!sh)
            continue;

        fprintf(stdout, "* %s\n", ep->d_name);
    }
    closedir(dp);

    free(buf1);
    free(buf2);
    return 0;
}

int bbox_list(int argc, char * const argv[])
{
    int non_optind;
    int rval = 0;

    bbox_conf_t *conf = bbox_config_new();
    if(!conf) {
        bbox_perror("list", "creating configuration context failed.");
        return BBOX_ERR_RUNTIME;
    }

    /* illusion: getopt itself is not thread-safe, probably. */
    if((non_optind = bbox_list_getopt(conf, argc, argv)) < 0) {
        bbox_config_free(conf);

        if(non_optind < -1) {
            return BBOX_ERR_INVOCATION;
        } else {
            /* user asked for --help */
            return 0;
        }
    }

    if(bbox_list_targets(conf) < 0)
        rval = BBOX_ERR_RUNTIME;

    bbox_config_free(conf);
    return rval;
}

