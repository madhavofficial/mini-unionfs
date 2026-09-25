#include "path_util.h"
#include "state.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>

void get_whiteout_path(const char *path, char *wh_path) {
    char temp_path[PATH_MAX];
    snprintf(temp_path, PATH_MAX, "%s", path);
    
    char *dir = dirname(temp_path);
    char temp_path2[PATH_MAX];
    snprintf(temp_path2, PATH_MAX, "%s", path);
    char *base = basename(temp_path2);
    
    if (strcmp(dir, "/") == 0) {
        snprintf(wh_path, PATH_MAX, "%s/.wh.%s", UNIONFS_DATA->upper_dir, base);
    } else {
        snprintf(wh_path, PATH_MAX, "%s%s/.wh.%s", UNIONFS_DATA->upper_dir, dir, base);
    }
}

int resolve_path(const char *path, char *resolved_path, int *is_lower) {
    char wh_path[PATH_MAX];
    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];

    snprintf(upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, path);
    snprintf(lower_path, PATH_MAX, "%s%s", UNIONFS_DATA->lower_dir, path);
    get_whiteout_path(path, wh_path);

    // 1. Check if whiteouted (must check FIRST to hide lower files)
    if (access(wh_path, F_OK) == 0) {
        return -ENOENT;
    }

    // 2. Check upper dir
    if (access(upper_path, F_OK) == 0) {
        snprintf(resolved_path, PATH_MAX, "%s", upper_path);
        if (is_lower) *is_lower = 0;
        return 0;
    }

    // 3. Check lower dir
    if (access(lower_path, F_OK) == 0) {
        snprintf(resolved_path, PATH_MAX, "%s", lower_path);
        if (is_lower) *is_lower = 1;
        return 0;
    }

    return -ENOENT;
}

int ensure_parent_dirs(const char *path) {
    char dir_path[PATH_MAX];
    char full_upper_path[PATH_MAX];
    snprintf(dir_path, PATH_MAX, "%s", path);
    
    char *parent = dirname(dir_path);
    if (strcmp(parent, "/") == 0 || strcmp(parent, ".") == 0) return 0;

    snprintf(full_upper_path, PATH_MAX, "%s%s", UNIONFS_DATA->upper_dir, parent);
    
    if (access(full_upper_path, F_OK) == 0) return 0;

    // Recursive call to create grandparents
    int res = ensure_parent_dirs(parent);
    if (res != 0) return res;
    
    log_msg("Creating parent directory in upper: %s\n", full_upper_path);
    if (mkdir(full_upper_path, 0755) == -1 && errno != EEXIST) {
        return -errno;
    }
    return 0;
}