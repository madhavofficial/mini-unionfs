#ifndef FS_OPS_H
#define FS_OPS_H

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

extern struct fuse_operations unionfs_oper;

int unionfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
int unionfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
int unionfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int unionfs_mknod(const char *path, mode_t mode, dev_t rdev);

#endif // FS_OPS_H