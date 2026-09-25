#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "state.h"
#include "fs_ops.h"
#include "logger.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lower_dir> <upper_dir> <mount_point> [FUSE options]\n", argv[0]);
        exit(1);
    }

    struct mini_unionfs_state *unionfs_data = malloc(sizeof(struct mini_unionfs_state));
    if (!unionfs_data) {
        perror("Failed to allocate memory");
        exit(1);
    }

    unionfs_data->lower_dir = realpath(argv[1], NULL);
    unionfs_data->upper_dir = realpath(argv[2], NULL);

    if (!unionfs_data->lower_dir || !unionfs_data->upper_dir) {
        fprintf(stderr, "Error resolving paths. Make sure lower and upper directories exist.\n");
        free(unionfs_data->lower_dir);
        free(unionfs_data->upper_dir);
        free(unionfs_data);
        exit(1);
    }

    log_init();
    log_msg("Starting Mini-UnionFS\nLower: %s\nUpper: %s\n", unionfs_data->lower_dir, unionfs_data->upper_dir);

    // Adjust argc/argv for fuse_main (remove lower/upper dir args)
    int fuse_argc = argc - 2;
    char **fuse_argv = malloc(fuse_argc * sizeof(char *));
    fuse_argv[0] = argv[0];
    for (int i = 3; i < argc; i++) {
        fuse_argv[i - 2] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &unionfs_oper, unionfs_data);

    log_close();
    free(unionfs_data->lower_dir);
    free(unionfs_data->upper_dir);
    free(unionfs_data);
    free(fuse_argv);

    return ret;
}