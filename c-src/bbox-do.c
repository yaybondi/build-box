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

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "bbox-do.h"

void bbox_main_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                         \n"
        "Copyright (c) 2017-2020 Tobias Koch <tobias.koch@gmail.com>         \n"
        "                                                                    \n"
        "Usage: build-box-do <command> [ARGS]                                \n"
        "                                                                    \n"
        "COMMANDS:                                                           \n"
        "                                                                    \n"
        "  login    Chroot into a target.                                    \n"
        "  mount    Mount homedir and special file systems (dev, proc, sys). \n"
        "  umount   Unmount homedir and special file systems.                \n"
        "  run      Execute a command chrooted inside a target.              \n"
        "                                                                    \n",
        BBOX_VERSION
    );
}

int main(int argc, char *argv[])
{
    if(getuid() == 0) {
        bbox_perror("main", "build-box must not be used by root.\n");
        return BBOX_ERR_INVOCATION;
    }

    if(bbox_lower_privileges() == -1)
        return BBOX_ERR_RUNTIME;

    if(bbox_check_user_in_group_build_box() == -1)
        return BBOX_ERR_INVOCATION;

    if(argc < 2)
    {
        bbox_main_usage();
        return BBOX_ERR_INVOCATION;
    }

    const char *command = argv[1];

    if(!strcmp(command, "-h") || !strcmp(command, "--help")) {
        bbox_main_usage();
        return 0;
    }

    if(strcmp(command, "login") == 0)
        return bbox_login(argc-1, &argv[1]);
    if(strcmp(command, "mount") == 0)
        return bbox_mount(argc-1, &argv[1]);
    if(strcmp(command, "umount") == 0)
        return bbox_umount(argc-1, &argv[1]);
    if(strcmp(command, "run") == 0)
        return bbox_run(argc-1, &argv[1]);

    bbox_perror("main", "unknown command '%s'.\n", command);
    return BBOX_ERR_INVOCATION;
}

