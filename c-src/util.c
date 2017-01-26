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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "bbox-do.h"

void bbox_sep_join(char **buf_ptr, const char *base, const char *sep,
        const char *sub, size_t *n_ptr)
{
    size_t base_len     = strlen(base);
    size_t sub_len      = strlen(sub);
    size_t sep_len      = strlen(sep);
    size_t req_buf_size = base_len + sep_len + sub_len + 1;

    if(req_buf_size > *n_ptr)
    {
        *buf_ptr = realloc(*buf_ptr, req_buf_size);
        if(!*buf_ptr) {
            bbox_perror("bbox_sep_join", "out of memory? %s\n",
                    strerror(errno));
            abort();
        }

        *n_ptr = req_buf_size;
    }

    memmove((void*) *buf_ptr, base, base_len);
    memmove((void*) *buf_ptr + base_len, sep, sep_len);
    memmove((void*) *buf_ptr + base_len + sep_len, sub, sub_len + 1);
}

void bbox_path_join(char **buf_ptr, const char *base, const char *sub,
        size_t *n_ptr)
{
    size_t base_len = strlen(base);
    size_t sub_len  = strlen(sub);
    size_t req_buf_size = base_len + sub_len + 1;

    if(base[base_len-1] != '/')
        req_buf_size++;

    if(req_buf_size > *n_ptr)
    {
        *buf_ptr = realloc(*buf_ptr, req_buf_size);
        if(!*buf_ptr) {
            bbox_perror("bbox_path_join", "out of memory? %s\n",
                    strerror(errno));
            abort();
        }

        *n_ptr = req_buf_size;
    }

    memmove((void*) *buf_ptr, base, base_len);

    if(base[base_len-1] != '/')
        (*buf_ptr)[base_len++] = '/';

    while(sub[0] == '/') {
        sub++;
        sub_len--;
    }

    memmove((void*) *buf_ptr + base_len, sub, sub_len + 1);
}

void bbox_perror(char *lead, char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "bbox-do %s: ", lead);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

int bbox_login_sh_chrooted(char *sys_root, char *home_dir, uid_t uid)
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    char *sh = NULL;
    struct stat st;

    /* change into system folder. */
    if(chdir(sys_root) == -1) {
        bbox_perror("bbox_login_sh_chrooted",
                "could not chdir to '%s': %s.\n",
                sys_root, strerror(errno));
        return -1;
    }

    /* do a few sanity checks before chrooting. */
    if(lstat(".", &st) == -1) {
        bbox_perror("bbox_login_sh_chrooted",
                "failed to stat '%s': %s.\n", sys_root,
                strerror(errno));
        return -1;
    }
    if(st.st_uid != uid) {
        bbox_perror("bbox_login_sh_chrooted",
                "system root is not owned by user.\n");
        return -1;
    }

    if(chroot(".") == -1) {
        bbox_perror("bbox_login_sh_chrooted",
                "chroot to system root failed: %s.\n",
                strerror(errno));
        return -1;
    }
    if(setuid(uid) == -1) {
        bbox_perror("bbox_login_sh_chrooted",
                "could not drop privileges: %s.\n",
                strerror(errno));
        return -1;
    }

    if(home_dir)
        if(chdir(home_dir) == -1);

    /* search for a shell. */
    for(int i = 0; (sh = shells[i]) != NULL; i++) {
        if(lstat(sh, &st) == 0)
            break;
        else
            sh = NULL;
    }

    if(!sh) {
        bbox_perror("bbox_login_sh_chrooted",
                "could not find a shell.\n");
        return -1;
    }

    execlp(sh, "sh", "-l", (char*) NULL);

    bbox_perror("bbox_login_sh_chrooted",
            "failed to invoke shell: %s.\n", strerror(errno));
    return -1;
}

int bbox_runas_sh_chrooted(const char *sys_root, const char *home_dir,
        uid_t uid, int argc, char * const argv[])
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    int pid;
    int child_status;
    char *sh = NULL;
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;

    if(argc == 0) {
        bbox_perror("bbox_runas_sh_chrooted",
                "missing arguments, nothing to run.\n");
        return -1;
    }

    if((pid = fork()) == -1) {
        bbox_perror("bbox_runas_sh_chrooted",
                "failed to start subprocess: %s.\n",
                strerror(errno));
        return -1;
    }

    /* this is the child exec'ing the new process. */
    if(pid == 0) {

        /* change into system folder. */
        if(chdir(sys_root) == -1) {
            bbox_perror("bbox_runas_sh_chrooted",
                    "could not chdir to '%s': %s.\n",
                    sys_root, strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }

        /* do a few sanity checks before chrooting. */
        if(lstat(".", &st) == -1) {
            bbox_perror("bbox_runas_sh_chrooted", "failed to stat '%s': %s.\n",
                    sys_root, strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }
        if(st.st_uid != uid) {
            bbox_perror("bbox_runas_sh_chrooted",
                    "chroot is not owned by user.\n");
            _exit(BBOX_ERR_RUNTIME);
        }

        /* now do actual chroot call. */
        if(chroot(".") == -1) {
            bbox_perror("bbox_runas_sh_chrooted",
                    "chroot to system root failed: %s.\n",
                    strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }

        /* drop privileges. */
        if(setuid(uid) == -1) {
            bbox_perror("bbox_runas_sh_chrooted", "failed to setuid: %s.\n",
                    strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }

        if(home_dir)
            if(chdir(home_dir) == -1);

        /* search for a shell. */
        for(int i = 0; (sh = shells[i]) != NULL; i++) {
            if(lstat(sh, &st) == 0)
                break;
            else
                sh = NULL;
        }

        if(!sh) {
            bbox_perror("bbox_runas_sh_chrooted", "could not find a shell.\n");
            return -1;
        }

        /* prepare the command line. */
        if(argc > 1) {
            for(int i = 0; i < argc; i++) {
                if(i > 0)
                    bbox_sep_join(&buf, buf, " ", argv[i], &buf_len);
                else
                    bbox_sep_join(&buf, "", "", argv[i], &buf_len);
            }
        } else {
            buf = argv[0];
        }

        char *command[6] = {"sh", "-l", "-c", "--", buf, NULL};
        execvp(sh, command);

        /* if we make it here exec failed. */
        _exit(BBOX_ERR_RUNTIME);
    }

    if(waitpid(pid, &child_status, 0) == -1) {
        bbox_perror("bbox_runas_chrooted",
                "unable to retrieve child exit status: %s.\n",
                strerror(errno));
        return -1;
    }

    return WEXITSTATUS(child_status);
}

int bbox_runas_fetch_output(uid_t uid, const char *cmd, char * const argv[],
        char **out_buf, size_t *out_buf_size)
{
    int pid;
    int child_status;
    int pipefd[2];
    int bytes_read;
    int total_read = 0;
    int req_space;
    char buf[1024];

    if(pipe(pipefd) == -1) {
        bbox_perror("bbox_runas_fetch_output",
                "failed to construct pipe: %s.\n",
                strerror(errno));
        return -1;
    }

    if((pid = fork()) == -1) {
        bbox_perror("bbox_runas_fetch_output",
                "failed to start subprocess: %s.\n",
                strerror(errno));
        return -1;
    }

    /* this is the child exec'ing for example 'mount'. */
    if(pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if(setuid(uid) == -1) {
            bbox_perror("bbox_runas_fetch_output",
                    "failed to setuid: %s.\n",
                    strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }
        execvp(cmd, argv);

        /* if we make it here exec failed. */
        _exit(BBOX_ERR_RUNTIME);
    }

    close(pipefd[1]);

    while((bytes_read = read(pipefd[0], buf, 1024)) > 0) {
        req_space = total_read + bytes_read + 1;

        if(req_space > *out_buf_size) {
            *out_buf_size = req_space * 2;
            *out_buf = realloc(*out_buf, *out_buf_size);

            if(!*out_buf) {
                bbox_perror("bbox_runas_fetch_output",
                        "out of memory? %s.\n",
                        strerror(errno));
                return -1;
            }
        }

        memcpy(*out_buf + total_read, buf, bytes_read);
        total_read += bytes_read;
        (*out_buf)[total_read] = '\0';
    }

    int done = 0;

    if(*out_buf) {
        // rtrim string.
        while(!done) {
            switch((*out_buf)[total_read]) {
                case '\r':
                case '\n':
                case ' ':
                case 127:
                case '\0':
                    (*out_buf)[total_read--] = '\0';
                    break;
                default:
                    done = 1;
                    break;
            }
        }
    }

    if(waitpid(pid, &child_status, 0) == -1) {
        bbox_perror("bbox_runas_fetch_output",
                "unable to retrieve child exit status: %s.\n",
                strerror(errno));
        return -1;
    }

    return WEXITSTATUS(child_status);
}

