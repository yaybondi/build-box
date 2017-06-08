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
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

bbox_conf_t *bbox_config_new()
{
    size_t i, n = 0, s = 0;
    char *lineptr = NULL;
    FILE *fp = fopen("/etc/passwd", "re");

    if(!fp) {
        bbox_perror("bbox_config_new", "error fetching user info: %s.\n",
                strerror(errno));
        return NULL;
    }

    uid_t ruid = getuid();
    bbox_conf_t *conf = calloc(sizeof(bbox_conf_t), 1);

    if(!conf) {
        bbox_perror("bbox_config_new", "out of memory?", strerror(errno));
        return NULL;
    }

    while(1)
    {
        ssize_t count = getline(&lineptr, &n, fp);

        if(count == -1) {
            free(lineptr);

            if(!feof(fp)) {
                bbox_perror("bbox_config_new", "error reading user info: %s.\n",
                        strerror(errno));
                fclose(fp);
                free(conf);
                return NULL;
            }

            fclose(fp);
            break;
        }

        char *start = NULL;
        char *end   = NULL;

        for(i=0, start=lineptr; (end=index(start, ':')); start=end+1, i++) {
            *end = '\0';

            if(i == 2) {
                uid_t pwuid = strtol(start, NULL, 10);
                if(ruid != pwuid)
                    break;
            }

            if(i == 5) {
                conf->home_dir = strdup(start);
                bbox_path_join(&(conf->target_dir), start, ".bolt/targets", &s);
            }
        }
    }

    if(!conf->home_dir)
        conf->home_dir = strdup("/");
    if(!conf->target_dir)
        conf->target_dir = strdup(".");
    conf->do_mount = 0;
    conf->do_file_updates = 0;

    return conf;
}

int bbox_config_set_target_dir(bbox_conf_t *conf, const char *path)
{
    if(conf->target_dir)
        free(conf->target_dir);
    conf->target_dir = strdup(path);

    if(!conf->target_dir) {
        bbox_perror("bbox_config_new", "out of memory? %s.\n", strerror(errno));
        return -1;
    }

    return 0;
}

char *bbox_config_get_target_dir(const bbox_conf_t *conf)
{
    return conf->target_dir;
}

int bbox_config_set_home_dir(bbox_conf_t *conf, const char *path)
{
    if(conf->home_dir)
        free(conf->home_dir);
    conf->home_dir = strdup(path);

    if(!conf->home_dir) {
        bbox_perror("bbox_config_new", "out of memory? %s.\n", strerror(errno));
        return -1;
    }

    return 0;
}

char *bbox_config_get_home_dir(const bbox_conf_t *conf)
{
    return conf->home_dir;
}

void bbox_config_clear_mount(bbox_conf_t *c)
{
    c->do_mount = 0;
}

void bbox_config_set_mount_all(bbox_conf_t *c)
{
    c->do_mount = BBOX_DO_MOUNT_ALL;
}

void bbox_config_set_mount_dev(bbox_conf_t *c)
{
    c->do_mount |= BBOX_DO_MOUNT_DEV;
}

void bbox_config_set_mount_proc(bbox_conf_t *c)
{
    c->do_mount |= BBOX_DO_MOUNT_PROC;
}

void bbox_config_set_mount_sys(bbox_conf_t *c)
{
    c->do_mount |= BBOX_DO_MOUNT_SYS;
}

void bbox_config_set_mount_home(bbox_conf_t *c)
{
    c->do_mount |= BBOX_DO_MOUNT_HOME;
}

void bbox_config_unset_mount_dev(bbox_conf_t *c)
{
    c->do_mount &= ~BBOX_DO_MOUNT_DEV;
}

void bbox_config_unset_mount_proc(bbox_conf_t *c)
{
    c->do_mount &= ~BBOX_DO_MOUNT_PROC;
}

void bbox_config_unset_mount_sys(bbox_conf_t *c)
{
    c->do_mount &= ~BBOX_DO_MOUNT_SYS;
}

void bbox_config_unset_mount_home(bbox_conf_t *c)
{
    c->do_mount &= ~BBOX_DO_MOUNT_HOME;
}

unsigned int bbox_config_get_mount_any(const bbox_conf_t *c)
{
    return c->do_mount;
}

unsigned int bbox_config_get_mount_dev(const bbox_conf_t *c)
{
    return (c->do_mount & BBOX_DO_MOUNT_DEV);
}

unsigned int bbox_config_get_mount_proc(const bbox_conf_t *c)
{
    return (c->do_mount & BBOX_DO_MOUNT_PROC);
}

unsigned int bbox_config_get_mount_sys(const bbox_conf_t *c)
{
    return (c->do_mount & BBOX_DO_MOUNT_SYS);
}

unsigned int bbox_config_get_mount_home(const bbox_conf_t *c)
{
    return (c->do_mount & BBOX_DO_MOUNT_HOME);
}

void bbox_config_disable_file_updates(bbox_conf_t *c)
{
    c->do_file_updates = 0;
}

void bbox_config_enable_file_updates(bbox_conf_t *c)
{
    c->do_file_updates = 1;
}

unsigned int bbox_config_do_file_updates(const bbox_conf_t *c)
{
    return (c->do_file_updates);
}

void bbox_config_free(bbox_conf_t *conf)
{
    if(conf->target_dir)
        free(conf->target_dir);
    free(conf);
}

