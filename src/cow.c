#include "cow.h"
#include "state.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include "path_util.h"

#define BUF_SIZE 4096

int execute_cow(const char *path) {
    char lower_path[PATH_MAX];
    char upper_path[PATH_MAX];
    
    snprintf(lower_path, PATH_MAX, "%s%s", UNIONFS_DATA->lower_dir, path);
    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);

    log_msg("Executing CoW for %s\n", path);

    int res = ensure_parent_dirs(path);
    if (res != 0) return res;

    int fd_lower = open(lower_path, O_RDONLY);
    if (fd_lower == -1) return -errno;

    struct stat st;
    if (fstat(fd_lower, &st) == -1) {
        close(fd_lower);
        return -errno;
    }

    int fd_upper = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_upper == -1) {
        close(fd_lower);
        return -errno;
    }

    char buf[BUF_SIZE];
    ssize_t bytes_read, bytes_written;

    while ((bytes_read = read(fd_lower, buf, BUF_SIZE)) > 0) {
        char *ptr = buf;
        while (bytes_read > 0) {
            bytes_written = write(fd_upper, ptr, bytes_read);
            if (bytes_written <= 0) {
                close(fd_lower);
                close(fd_upper);
                return -errno;
            }
            bytes_read -= bytes_written;
            ptr += bytes_written;
        }
    }

    close(fd_lower);
    close(fd_upper);
    
    // Copy permissions and ownership
    chmod(upper_path, st.st_mode);
    chown(upper_path, st.st_uid, st.st_gid);

    return 0;
}
