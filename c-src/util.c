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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <grp.h>
#include <errno.h>
#include "bbox-do.h"

#ifndef _GNU_SOURCE
extern char **environ;
#endif

#define BBOX_COPY_BUF_SIZE 4096

void bbox_sanitize_environment()
{
    char *start, *end, *name;

    for(int i = 0; (start = environ[i]) != NULL; i++)
    {
        if(!strncmp(start, "BOLT_", 5))
            continue;
        if(!strncmp(start, "DISPLAY=", 8))
            continue;
        if(!strncmp(start, "SSH_CONNECTION=", 15))
            continue;
        if(!strncmp(start, "SSH_CLIENT=", 11))
            continue;
        if(!strncmp(start, "SSH_TTY=", 8))
            continue;
        if(!strncmp(start, "USER=", 5))
            continue;
        if(!strncmp(start, "TERM=", 5))
            continue;
        if(!strncmp(start, "HOME=", 5))
            continue;
        if(!strncmp(start, "CFLAGS=", 7))
            continue;
        if(!strncmp(start, "CXXFLAGS=", 9))
            continue;
        if(!strncmp(start, "CPPFLAGS=", 9))
            continue;
        if(!strncmp(start, "LDFLAGS=", 8))
            continue;

        if((end = strchr(start, '=')) == NULL)
            continue;

        name = strndup(start, end - start);

        if(!name) {
            bbox_perror("bbox_sanitize_environment", "out of memory!\n");
            abort();
        }

        unsetenv(name);
        free(name);
        i = 0;
    }
}

int bbox_copy_file(const char *src, const char *dst)
{
    struct stat src_st, dst_st;
    char buf[BBOX_COPY_BUF_SIZE];
    char *dst_tmp1 = NULL;
    char *dst_tmp2 = NULL;
    char *filename = NULL;
    char *pathname = NULL;
    char *tmp_dst  = NULL;
    char *ptr      = NULL;
    size_t pathname_len = 0;
    size_t filename_len = 0;
    int in_fd = -1, out_fd = -1, rval = -1;
    int num_bytes_read, num_bytes_written;

    if(lstat(src, &src_st) == -1) {
        bbox_perror("bbox_copy_file", "could not stat '%s'.\n", src);
        goto cleanup_and_exit;
    }

    if(lstat(dst, &dst_st) ==  0) {
        if(S_ISLNK(dst_st.st_mode) || !S_ISREG(dst_st.st_mode)) {
            bbox_perror("bbox_copy_file",
                    "destination is not a regular file.\n");
            goto cleanup_and_exit;
        }
    }

    dst_tmp1 = strdup(dst);
    if(dst_tmp1 == NULL) {
        bbox_perror("bbox_copy_file", "out of memory!\n");
        abort();
    }

    dst_tmp2 = strdup(dst);
    if(dst_tmp2 == NULL) {
        bbox_perror("bbox_copy_file", "out of memory!\n");
        abort();
    }

    filename     = basename(dst_tmp1);
    filename_len = strlen(filename);
    pathname     = dirname(dst_tmp2);
    pathname_len = strlen(pathname);
    tmp_dst      = malloc(pathname_len + filename_len + 3);

    if(tmp_dst == NULL) {
        bbox_perror("bbox_copy_file", "out of memory!\n");
        abort();
    }

    strncpy(tmp_dst, pathname, pathname_len);
    tmp_dst[pathname_len + 0] = '/';
    tmp_dst[pathname_len + 1] = '.';
    strncpy(tmp_dst + pathname_len + 2, filename, filename_len);
    tmp_dst[pathname_len + 2 + filename_len] = '\0';

    if((in_fd = open(src, O_RDONLY)) == -1) {
        bbox_perror("bbox_copy_file", "failed to open '%s' for reading: %s\n",
                src, strerror(errno));
        goto cleanup_and_exit;
    }

    if((out_fd = creat(dst, src_st.st_mode)) == -1) {
        bbox_perror("bbox_copy_file", "failed to open '%s' for writing: %s\n",
                dst, strerror(errno));
        goto cleanup_and_exit;
    }

    while(1)
    {
        num_bytes_read = read(in_fd, (void*) buf, BBOX_COPY_BUF_SIZE);

        if(!num_bytes_read)
            break;

        if(num_bytes_read == -1) {
            if(errno != EINTR) {
                goto cleanup_and_exit;
            } else {
                continue;
            }
        }

        ptr = buf;

        while(num_bytes_read) {
            num_bytes_written = write(out_fd, ptr, num_bytes_read);

            if(num_bytes_written == -1) {
                if(errno != EINTR) {
                    goto cleanup_and_exit;
                } else {
                    continue;
                }
            }

            num_bytes_read -= num_bytes_written;
            ptr += num_bytes_written;
        }
    }

    rval = 0;

cleanup_and_exit:

    if(in_fd != -1)
        close(in_fd);
    if(out_fd != -1)
        close(out_fd);

    free(dst_tmp1);
    free(dst_tmp2);
    free(tmp_dst);

    return rval;
}

void bbox_sep_join(char **buf_ptr, const char *base, const char *sep,
        const char *sub, size_t *n_ptr)
{
    size_t base_len     = strlen(base);
    size_t sub_len      = strlen(sub);
    size_t sep_len      = strlen(sep);
    size_t req_buf_size = base_len + sep_len + sub_len + 1;
    int base_is_buffer  = 0;

    if(base == *buf_ptr)
        base_is_buffer = 1;

    if(req_buf_size > *n_ptr)
    {
        *buf_ptr = realloc(*buf_ptr, req_buf_size);

        if(!*buf_ptr) {
            bbox_perror("bbox_sep_join", "out of memory? %s\n",
                    strerror(errno));
            abort();
        }

        if(base_is_buffer)
            base = *buf_ptr;

        *n_ptr = req_buf_size;
    }

    memmove((void*) *buf_ptr, base, base_len + 1);
    memmove((void*) *buf_ptr + base_len, sep, sep_len + 1);
    memmove((void*) *buf_ptr + base_len + sep_len, sub, sub_len + 1);
}

void bbox_path_join(char **buf_ptr, const char *base, const char *sub,
        size_t *n_ptr)
{
    size_t base_len = strlen(base);
    size_t sub_len  = strlen(sub);
    size_t req_buf_size = base_len + sub_len + 1;
    int base_is_buffer = 0;

    if(base == *buf_ptr)
        base_is_buffer = 1;

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

        if(base_is_buffer)
            base = *buf_ptr;

        *n_ptr = req_buf_size;
    }

    memmove((void*) *buf_ptr, base, base_len + 1);

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

int bbox_login_sh_chrooted(char *sys_root, char *home_dir)
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    char *sh = NULL;
    struct stat st;
    int uid = getuid();

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

    if(bbox_raise_privileges() == -1)
        return -1;

    if(chroot(".") == -1) {
        bbox_perror("bbox_login_sh_chrooted",
                "chroot to system root failed: %s.\n",
                strerror(errno));
        return -1;
    }

    if(bbox_drop_privileges() == -1)
        _exit(BBOX_ERR_RUNTIME);

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

int bbox_runas_user_chrooted(const char *sys_root, const char *home_dir,
        int argc, char * const argv[])
{
    static char *shells[] = {"/tools/bin/sh", "/usr/bin/sh", NULL};

    int pid;
    int child_status;
    char *sh = NULL;
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;
    int uid = getuid();

    if(argc == 0) {
        bbox_perror("bbox_runas_user_chrooted",
                "missing arguments, nothing to run.\n");
        return -1;
    }

    if((pid = fork()) == -1) {
        bbox_perror("bbox_runas_user_chrooted",
                "failed to start subprocess: %s.\n",
                strerror(errno));
        return -1;
    }

    /* this is the child exec'ing the new process. */
    if(pid == 0) {

        /* change into system folder. */
        if(chdir(sys_root) == -1) {
            bbox_perror("bbox_runas_user_chrooted",
                    "could not chdir to '%s': %s.\n",
                    sys_root, strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }

        /* do a few sanity checks before chrooting. */
        if(lstat(".", &st) == -1) {
            bbox_perror("bbox_runas_user_chrooted", "failed to stat '%s': %s.\n",
                    sys_root, strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }
        if(st.st_uid != uid) {
            bbox_perror("bbox_runas_user_chrooted",
                    "chroot is not owned by user.\n");
            _exit(BBOX_ERR_RUNTIME);
        }

        if(bbox_raise_privileges() == -1)
            return -1;

        /* now do actual chroot call. */
        if(chroot(".") == -1) {
            bbox_perror("bbox_runas_user_chrooted",
                    "chroot to system root failed: %s.\n",
                    strerror(errno));
            _exit(BBOX_ERR_RUNTIME);
        }

        if(bbox_drop_privileges() == -1)
            _exit(BBOX_ERR_RUNTIME);

        /* this is non-critical. */
        if(home_dir)
            if(chdir(home_dir));

        /* search for a shell. */
        for(int i = 0; (sh = shells[i]) != NULL; i++) {
            if(lstat(sh, &st) == 0)
                break;
            else
                sh = NULL;
        }

        if(!sh) {
            bbox_perror("bbox_runas_user_chrooted", "could not find a shell.\n");
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
    char buf[BBOX_COPY_BUF_SIZE];

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

        if(uid == 0) {
            if(bbox_raise_privileges() == -1)
                _exit(BBOX_ERR_RUNTIME);
        }
        execvp(cmd, argv);

        /* if we make it here exec failed. */
        _exit(BBOX_ERR_RUNTIME);
    }

    close(pipefd[1]);

    while((bytes_read = read(pipefd[0], buf, BBOX_COPY_BUF_SIZE)) > 0) {
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

void bbox_update_chroot_dynamic_config(const char *sys_root)
{
    char *file;
    char *buf = NULL;
    size_t buf_len = 0;
    struct stat st;

    char *file_list[] = {
        "/etc/passwd",
        "/etc/group",
        "/etc/resolv.conf",
        "/etc/hosts",
        NULL
    };

    for(int i = 0; file_list[i] != NULL; i++) {
        file = file_list[i];

        if(lstat(file, &st) == -1)
            continue;

        bbox_path_join(&buf, sys_root, file, &buf_len);
        bbox_copy_file(file, buf);
    }
}

int bbox_lower_privileges()
{
    if(seteuid(getuid()) == -1) {
        bbox_perror("bbox_lower_privileges",
                "failed to lower privileges: %s.\n",
                    strerror(errno));
        return -1;
    }

    return 0;
}

int bbox_raise_privileges()
{
    if(setuid(0) == -1) {
        bbox_perror("bbox_raise_privileges",
                "failed to restore root privileges: %s.\n",
                    strerror(errno));
        return -1;
    }

    return 0;
}

int bbox_drop_privileges()
{
    if(setuid(getuid()) == -1) {
        bbox_perror("bbox_drop_privileges",
                "could not drop privileges: %s.\n",
                    strerror(errno));
        return -1;
    }

    return 0;
}

int bbox_check_user_in_group_build_box()
{
    struct group grp, *result = NULL;

    int    rval   = -1;
    char  *buf    = NULL;
    size_t buflen = 1024;
    gid_t *groups = NULL;

    while(1) {
        buf = realloc(buf, buflen);

        if(buf == NULL) {
            bbox_perror("bbox_check_user_in_group_build_box",
                    "out of memory?\n");
            goto cleanup_and_exit;
        }

        errno = 0;

        int rval = getgrnam_r(BBOX_GROUP_NAME, &grp, buf, buflen, &result);

        if(result)
            break;

        if(rval == 0) {
            bbox_perror("bbox_check_user_in_group_build_box",
                    "group '" BBOX_GROUP_NAME "' not found.\n");
            goto cleanup_and_exit;
        }

        if(rval == ERANGE) {
            buflen *= 2;
            continue;
        }

        if(rval == EINTR)
            continue;

        bbox_perror("bbox_check_user_in_group_build_box",
                "error retrieving group info: %s.\n", strerror(errno));
        goto cleanup_and_exit;
    }

    gid_t gid = grp.gr_gid;

    /* Someone might think using the primary groups is a good idea. */
    if(getegid() == gid) {
        rval = 0;
        goto cleanup_and_exit;
    }

    int ngroups = getgroups(0, NULL);

    if(ngroups == -1) {
        bbox_perror("bbox_check_user_in_group_build_box",
                "error getting number of suplementary groups.\n");
        goto cleanup_and_exit;
    }

    groups = realloc(groups, sizeof(gid_t) * ngroups);

    if(groups == NULL) {
        bbox_perror("bbox_check_user_in_group_build_box",
                "out of memory?\n");
        goto cleanup_and_exit;
    }

    if(getgroups(ngroups, groups) != ngroups) {
        bbox_perror("bbox_check_user_in_group_build_box",
                "error fetching group list: %s", strerror(errno));
        goto cleanup_and_exit;
    }

    for(int i = 0; i < ngroups; i++) {
        if(gid == groups[i]) {
            rval = 0;
            break;
        }
    }

    if(rval != 0) {
        bbox_perror("bbox_check_user_in_group_build_box",
                "user is not in group '" BBOX_GROUP_NAME "'.\n");
    }

cleanup_and_exit:

    free(buf);
    free(groups);
    return rval;
}

