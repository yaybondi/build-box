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
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "bbox-do.h"

void bbox_main_usage()
{
    printf(
        "Copyright (c) 2017-2021 Tobias Koch <tobias.koch@gmail.com>         \n"
        "                                                                    \n"
        "This is the Build Box NG management utility.                        \n"
        "                                                                    \n"
        "USAGE:                                                              \n"
        "                                                                    \n"
        "  build-box [OPTIONS] <command> [ARGS]                              \n"
        "                                                                    \n"
        "COMMANDS:                                                           \n"
        "                                                                    \n"
        "  init     Initialize build box environment for new user.           \n"
        "  login    Chroot into a target.                                    \n"
        "  mount    Mount homedir and special file systems (dev, proc, sys). \n"
        "  umount   Unmount homedir and special file systems.                \n"
        "  run      Execute a command chrooted inside a target.              \n"
        "                                                                    \n"
        "OPTIONS:                                                            \n"
        "                                                                    \n"
        "  -h, --help    Print this help message and exit.                   \n"
        "                                                                    \n"
        "Type `build-box <command> --help` for more information about        \n"
        "individual commands.                                                \n"
    );
}

int main(int argc, char *argv[])
{
    /*
     * Build Box Do is not meant to be called directly, it should always be
     * invoked through the Build Box Python wrapper.
     */
    char *build_box_wrapper_token = getenv("BUILD_BOX_WRAPPER_A883DAFC");
    if(build_box_wrapper_token == NULL) {
        bbox_perror("main", "build-box-do should not be invoked directly.\n");
        return BBOX_ERR_INVOCATION;
    }

    /*
     * Build Box has been designed to give regular users just enough privileges
     * to work with "build boxed" chroots. Possibly, it would work fine for
     * root, but this isn't an intended use case.
     */
    if(getuid() == 0) {
        bbox_perror("main", "build-box must not be used by root.\n");
        return BBOX_ERR_INVOCATION;
    }

    /*
     * The `build-box` program is installed suid root, but privileges are
     * lowered immediately and only raised again when necessary (e.g. when
     * creating bind mounts for home, dev, sys, ...).
     */
    if(bbox_lower_privileges() == -1)
        return BBOX_ERR_RUNTIME;

    /*
     * Build Box has safe-guards in place to prevent misuse as much as possible.
     * But it is a suid root binary and gives developers extra powers (mostly
     * related to bind mounting stuff). Its use on a given machine SHOULD be
     * restricted to a dedicated build account operated by a trusted user. To
     * enforce this, we require all users of the `build-box` command to be in
     * group "build-box".
     */
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

    /*
     * Check which command was invoked and delegate to the appropriate
     * sub-module.
     */
    if(strcmp(command, "init") == 0)
        return bbox_init(argc-1, &argv[1]);
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
