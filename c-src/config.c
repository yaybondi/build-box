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
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

bbox_conf_t *bbox_config_new()
{
    bbox_conf_t *conf = calloc(sizeof(bbox_conf_t), 1);
    if(!conf) {
        bbox_perror("bbox_config_new", "out of memory?\n");
        return NULL;
    }

    unsigned long uid = (unsigned long) getuid();

    /*
     * Always take the home directory from the password database, because it
     * seems risky to let the user specify arbitrary locations in the `$HOME`
     * environment variables.
     */
    char *homedir = NULL;

    struct passwd *pwd = getpwuid(uid);
    if(pwd)
        homedir = pwd->pw_dir;

    /*
     * Normalize the path to mitigate the risk of any hypothetical symlink
     * attacks.
     */
    if(!homedir || !(conf->home_dir = realpath(homedir, NULL))) {
        bbox_perror(
            "bbox_config_new", "could not determine user home directory.\n"
        );
        goto failure;
    }

    /*
     * The home directory worked out above MUST be owned by the user who
     * invoked the program. Anything else is fishy and there is no reason to
     * allow it.
     */

    size_t buf_size = 0;

    if(bbox_isdir_and_owned_by("bbox_config_new", conf->home_dir, uid) == -1)
        goto failure;

    conf->target_dir = bbox_get_user_dir(uid, &buf_size);

    if(conf->target_dir == NULL) {
        bbox_perror(
            "bbox_config_new", "failed to get user directory.\n"
        );
        goto failure;
    }

    bbox_path_join(&conf->target_dir, conf->target_dir, "targets", &buf_size);

    /*
     * Start with an empty set of actions and explicitly add them when needed.
     */
    conf->do_mount = 0;
    conf->do_file_updates = 0;
    conf->have_ns = 0;

success:
    return conf;

failure:
    bbox_config_free(conf);
    return NULL;
}

int bbox_config_set_target_dir(bbox_conf_t *conf, const char *path)
{
    if(conf->target_dir)
        free(conf->target_dir);
    conf->target_dir = strdup(path);

    if(!conf->target_dir) {
        bbox_perror("bbox_config_new", "out of memory?\n");
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
        bbox_perror("bbox_config_new", "out of memory?\n");
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

void bbox_config_set_have_pid_ns(bbox_conf_t *c)
{
    c->have_ns |= BBOX_HAVE_PID_NS;
}

void bbox_config_unset_have_pid_ns(bbox_conf_t *c)
{
    c->have_ns &= ~BBOX_HAVE_PID_NS;
}

unsigned int bbox_config_get_have_pid_ns(const bbox_conf_t *c)
{
    return (c->have_ns & BBOX_HAVE_PID_NS);
}

void bbox_config_free(bbox_conf_t *conf)
{
    free(conf->home_dir);
    free(conf->target_dir);
    free(conf);
}
