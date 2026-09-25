#define FUSE_USE_VERSION 31
#include "fs_ops.h"
#include <sys/stat.h>
#include "state.h"
#include "path_util.h"
#include "cow.h"
#include "logger.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>

static int unionfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char resolved_path[PATH_MAX];
    int res = resolve_path(path, resolved_path, NULL);
    if (res != 0) return res;

    res = lstat(resolved_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char upper_dir_path[PATH_MAX];
    char lower_dir_path[PATH_MAX];
    
    snprintf(upper_dir_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    snprintf(lower_dir_path, PATH_MAX, "%s%s", UNIONFS_DATA->lower_dir, path);

    DIR *dp;
    struct dirent *de;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Track seen files to avoid duplicates
    // For a simple implementation, we can just use the fact that 
    // upper layer files should take precedence.
    // We will collect names in a simple array or just check against upper.

    // 1. Read upper layer (Precedence)
    dp = opendir(upper_dir_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strncmp(de->d_name, ".wh.", 4) == 0) continue; 
            if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
                filler(buf, de->d_name, NULL, 0, 0);
            }
        }
        closedir(dp);
    }

    // 2. Read lower layer
    dp = opendir(lower_dir_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

            char wh_path[PATH_MAX];
            char child_path[PATH_MAX];
            char upper_child_path[PATH_MAX];

            if (strcmp(path, "/") == 0) snprintf(child_path, PATH_MAX, "/%s", de->d_name);
            else snprintf(child_path, PATH_MAX, "%s/%s", path, de->d_name);
            
            get_whiteout_path(child_path, wh_path);
            snprintf(upper_child_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, child_path);
            
            // Skip if whiteouted OR if it exists in upper layer (duplicate)
            if (access(wh_path, F_OK) != 0 && access(upper_child_path, F_OK) != 0) {
                filler(buf, de->d_name, NULL, 0, 0);
            }
        }
        closedir(dp);
    }
    return 0;
}

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    // Trigger CoW if writing to a lower layer file
    if (is_lower && (fi->flags & (O_WRONLY | O_RDWR | O_APPEND))) {
        res = execute_cow(path);
        if (res != 0) return res;
        snprintf(resolved_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    }

    int fd = open(resolved_path, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return res;
}

static int unionfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return res;
}

static int unionfs_unlink(const char *path) {
    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];

    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    snprintf(lower_path, PATH_MAX, "%s%s", UNIONFS_DATA->lower_dir, path);

    int upper_exists = (access(upper_path, F_OK) == 0);
    int lower_exists = (access(lower_path, F_OK) == 0);

    // Case 1: exists in upper → delete it
    if (upper_exists) {
        if (unlink(upper_path) == -1) return -errno;
    }

    // Case 2: exists in lower → create whiteout
    if (lower_exists) {
        char wh_path[PATH_MAX];
        get_whiteout_path(path, wh_path);

        int res = ensure_parent_dirs(path);
        if (res != 0) return res;

        int fd = open(wh_path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) return -errno;
        close(fd);
    }

    return 0;
}

static int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    
    int res = ensure_parent_dirs(path);
    if (res != 0) return res;
    
    // Cleanup whiteout if it exists
    char wh_path[PATH_MAX];
    get_whiteout_path(path, wh_path);
    unlink(wh_path);

    int fd = open(upper_path, fi->flags, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    
    int res = ensure_parent_dirs(path);
    if (res != 0) return res;
    
    // Cleanup whiteout if it exists
    char wh_path[PATH_MAX];
    get_whiteout_path(path, wh_path);
    unlink(wh_path);

    res = mkdir(upper_path, mode);
    if (res == -1) return -errno;
    return 0;
}

static int unionfs_rmdir(const char *path) {
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    if (is_lower) {
        char wh_path[PATH_MAX];
        get_whiteout_path(path, wh_path);
        int res2 = ensure_parent_dirs(path);
        if (res2 != 0) return res2;
        int fd = open(wh_path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) return -errno;
        close(fd);
    } else {
        res = rmdir(resolved_path);
        if (res == -1) return -errno;
        
        char lower_path[PATH_MAX];
        snprintf(lower_path, PATH_MAX, "%s%s", UNIONFS_DATA->lower_dir, path);
        if (access(lower_path, F_OK) == 0) {
            char wh_path[PATH_MAX];
            get_whiteout_path(path, wh_path);
            int fd = open(wh_path, O_CREAT | O_WRONLY, 0644);
            if (fd != -1) close(fd);
        }
    }
    return 0;
}

static int unionfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    if (is_lower) {
        res = execute_cow(path);
        if (res != 0) return res;
        snprintf(resolved_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    }

    if (fi != NULL) return ftruncate(fi->fh, size);
    else return truncate(resolved_path, size);
}

static int unionfs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);
    return 0;
}

int unionfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    if (is_lower) {
        res = execute_cow(path);
        if (res != 0) return res;
        snprintf(resolved_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    }

    res = chmod(resolved_path, mode);
    if (res == -1) return -errno;
    return 0;
}

int unionfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void) fi;
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    if (is_lower) {
        res = execute_cow(path);
        if (res != 0) return res;
        snprintf(resolved_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    }

    res = chown(resolved_path, uid, gid);
    if (res == -1) return -errno;
    return 0;
}

int unionfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char resolved_path[PATH_MAX];
    int is_lower;
    int res = resolve_path(path, resolved_path, &is_lower);
    if (res != 0) return res;

    if (is_lower) {
        res = execute_cow(path);
        if (res != 0) return res;
        snprintf(resolved_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    }

    res = utimensat(AT_FDCWD, resolved_path, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) return -errno;
    return 0;
}

int unionfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    
    int res = ensure_parent_dirs(path);
    if (res != 0) return res;
    
    // Cleanup whiteout if it exists
    char wh_path[PATH_MAX];
    get_whiteout_path(path, wh_path);
    unlink(wh_path);

    if (S_ISREG(mode)) {
        res = open(upper_path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0) {
            close(res);
            return 0;
        }
    } else {
        res = mknod(upper_path, mode, rdev);
    }

    if (res == -1) return -errno;
    return 0;
}

struct fuse_operations unionfs_oper = {
    .getattr  = unionfs_getattr,
    .readdir  = unionfs_readdir,
    .open     = unionfs_open,
    .read     = unionfs_read,
    .write    = unionfs_write,
    .unlink   = unionfs_unlink,
    .create   = unionfs_create,
    .mkdir    = unionfs_mkdir,
    .rmdir    = unionfs_rmdir,
    .truncate = unionfs_truncate,
    .release  = unionfs_release,
    .chmod    = unionfs_chmod,
    .chown    = unionfs_chown,
    .utimens  = unionfs_utimens,
    .mknod    = unionfs_mknod,
};
