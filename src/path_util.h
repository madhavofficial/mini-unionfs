#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <limits.h>

void get_whiteout_path(const char *path, char *wh_path);
int resolve_path(const char *path, char *resolved_path, int *is_lower);
int ensure_parent_dirs(const char *path);

#endif // PATH_UTIL_H