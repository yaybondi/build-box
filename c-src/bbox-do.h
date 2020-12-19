#ifndef BBOX_H_INCLUDED
#define BBOX_H_INCLUDED

#define BBOX_VERSION "1.0.0"
#define BBOX_ERR_INVOCATION 1
#define BBOX_ERR_RUNTIME    2

/* Config */

#define BBOX_DO_MOUNT_DEV  0x1
#define BBOX_DO_MOUNT_PROC 0x2
#define BBOX_DO_MOUNT_SYS  0x4
#define BBOX_DO_MOUNT_HOME 0x8
#define BBOX_DO_MOUNT_ALL  0xF

#define BBOX_GROUP_NAME "build-box"
#define BBOX_VAR_LIB "/var/lib/build-box"
#define BBOX_USER_DIR_TEMPLATE BBOX_VAR_LIB"/users/%lu"

#include <sys/types.h>

typedef struct {
    char *target_dir;
    char *home_dir;
    unsigned int do_mount;
    unsigned int do_file_updates;
} bbox_conf_t;

bbox_conf_t *bbox_config_new();

int bbox_config_set_target_dir(bbox_conf_t *conf, const char *path);
char *bbox_config_get_target_dir(const bbox_conf_t *conf);

int bbox_config_set_home_dir(bbox_conf_t *conf, const char *path);
char *bbox_config_get_home_dir(const bbox_conf_t *conf);

void bbox_config_clear_mount(bbox_conf_t *c);

void bbox_config_set_mount_all(bbox_conf_t *c);
void bbox_config_set_mount_dev(bbox_conf_t *c);
void bbox_config_set_mount_proc(bbox_conf_t *c);
void bbox_config_set_mount_sys(bbox_conf_t *c);
void bbox_config_set_mount_home(bbox_conf_t *c);

void bbox_config_unset_mount_dev(bbox_conf_t *c);
void bbox_config_unset_mount_proc(bbox_conf_t *c);
void bbox_config_unset_mount_sys(bbox_conf_t *c);
void bbox_config_unset_mount_home(bbox_conf_t *c);

unsigned int bbox_config_get_mount_any(const bbox_conf_t *c);
unsigned int bbox_config_get_mount_dev(const bbox_conf_t *c);
unsigned int bbox_config_get_mount_proc(const bbox_conf_t *c);
unsigned int bbox_config_get_mount_sys(const bbox_conf_t *c);
unsigned int bbox_config_get_mount_home(const bbox_conf_t *c);

void bbox_config_disable_file_updates(bbox_conf_t *c);
void bbox_config_enable_file_updates(bbox_conf_t *c);
unsigned int bbox_config_do_file_updates(const bbox_conf_t *c);
void bbox_config_free(bbox_conf_t *conf);

/* Utilities */

void bbox_sep_join(char **buf_ptr, const char *base, const char *sep,
        const char *sub, size_t *n_ptr);
void bbox_path_join(char **buf_ptr, const char *base, const char *sub,
        size_t *n_ptr);
void bbox_perror(const char *lead, const char *msg, ...);
int bbox_login_sh_chrooted(char *sys_root, char *home_dir);
int bbox_runas_user_chrooted(const char *sys_root, const char *home_dir,
        int argc, char * const argv[]);
int bbox_run_command_capture(uid_t uid, const char *cmd, char * const argv[],
        char **out_buf, size_t *out_buf_size);
void bbox_update_chroot_dynamic_config(const char *sys_root);
void bbox_sanitize_environment();

int bbox_lower_privileges();
int bbox_raise_privileges();
int bbox_drop_privileges();

int bbox_check_user_in_group_build_box();
int bbox_isdir_and_owned_by(const char *module, const char *dir, uid_t uid);
int bbox_mkdir_p(const char *module, const char *path);
int bbox_sysroot_mkdir_p(const char *module, const char *sysroot,
        const char *path);
int bbox_is_subdir_of(const char *path, const char *subdir);
int bbox_try_fix_pkg_cache_symlink(char *module);

char *bbox_get_user_dir(uid_t uid, size_t *n_ptr);

/* Mounting */

int bbox_mount_any(const bbox_conf_t *conf, const char *sys_root);
int bbox_mount_is_mounted(const char *path);

/* Setup */

int bbox_init_user_directory();

/* Commands */

int bbox_init(int argc, char * const argv[]);
int bbox_list(int argc, char * const argv[]);
int bbox_login(int argc, char * const argv[]);
int bbox_run(int argc, char * const argv[]);
int bbox_mount(int argc, char * const argv[]);
int bbox_umount(int argc, char * const argv[]);

#endif
