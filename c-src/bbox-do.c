#include <stdio.h>
#include <string.h>
#include "bbox-do.h"

void bbox_main_usage()
{
    printf(
        "Build Box NG Management Utility, Version %s                         \n"
        "Copyright (c) 2017 Tobias Koch <tobias.koch@nonterra.com>           \n"
        "                                                                    \n"
        "Usage: bbox-do <command> [ARGS]                                     \n"
        "                                                                    \n"
        "COMMANDS:                                                           \n"
        "                                                                    \n"
        "  list     List all existing targets.                               \n"
        "  login    Chroot into a target.                                    \n"
        "  mount    Mount /home and special file systems (dev, proc, sys).   \n"
        "  umount   Unmount /home and special file systems.                  \n"
        "  run      Execute a command inside a target.                       \n"
        "                                                                    \n",
        BBOX_VERSION
    );
}

int main(int argc, char *argv[])
{
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

    if(strcmp(command, "list") == 0)
        return bbox_list(argc-1, &argv[1]);
    if(strcmp(command, "login") == 0)
        return bbox_login(argc-1, &argv[1]);
    if(strcmp(command, "mount") == 0)
        return bbox_mount(argc-1, &argv[1]);
    if(strcmp(command, "umount") == 0)
        return bbox_umount(argc-1, &argv[1]);
    if(strcmp(command, "run") == 0)
        return bbox_run(argc-1, &argv[1]);

    bbox_perror("main", "bbox-do: unknown command '%s'.\n", command);
    return BBOX_ERR_INVOCATION;
}

