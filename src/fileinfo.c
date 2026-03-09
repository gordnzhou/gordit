#include <string.h>

#include "fileinfo.h"
#include "logging.h"
#include "utils.h"

struct fileinfo *start_fileinfo(const git_repo *repo, const char *norm_path, const char *mode) {
    struct fileinfo *info = smalloc(sizeof(*info));

    fs_path_join(repo->root_path, norm_path, info->abs_path);
    snprintf(info->norm_path, PATH_MAX, "%s", norm_path);

    if (fs_getinfo(info->abs_path, &(info->stat)) != 0) {
        fatal("could get stat of git file %s", norm_path);
    }

    if ((info->fptr = fopen(info->abs_path, mode)) == NULL) {
        fatal("could not open git file %s", norm_path);
    }

    return info;
}

void end_fileinfo(struct fileinfo *info) {
    fclose(info->fptr);
    free(info);
}
