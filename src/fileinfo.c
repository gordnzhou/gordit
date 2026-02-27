#include <string.h>

#include "fileinfo.h"
#include "logging.h"
#include "utils.h"

struct fileinfo *start_fileinfo(const git_repo *repo, const char *norm_path, const char *mode) {
    static struct fileinfo info;

    fs_path_join(repo->root_path, norm_path, info.abs_path);
    snprintf(info.norm_path, PATH_MAX, "%s", norm_path);

    if (fs_getinfo(info.abs_path, &(info.stat)) != 0) {
        return NULL;
    }

    if ((info.fptr = fopen(info.abs_path, mode)) == NULL) {
        return NULL;
    }

    return &info;
}

void end_fileinfo(struct fileinfo *info) {
    fclose(info->fptr);
}
