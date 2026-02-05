
#include "filespec.h"

int is_file_args_valid(const git_repo *repo, char **args, int num_args) {
    for (int i = 0; i < num_args; i++) {
        char *temp = malloc(PATH_MAX);
        int ok = 1;
        if (strlen(args[i]) + 1 > PATH_MAX || fs_path_abs(args[i], temp) || !fs_file_exists(temp)) {
            printf("ERROR: could not find file: %s\n", args[i]);
            ok = 0;
        }
        if (!is_path_in_repo(repo, temp)) {
            printf("ERROR: file is not part of repo: %s\n", args[i]);
            ok = 0;
        }
        free(temp);

        if (!ok) {
            return 0;
        }
    }

    return 1;
}

struct fileinfo *start_fileinfo(const git_repo *repo, const char *path, const char *mode) {
    static struct fileinfo info;
    assert(fs_path_abs(path, info.path) == 0);
    repo_rel_path(repo, info.path, info.name);

    if (fs_getinfo(path, &(info.stat)) != 0) {
        return NULL;
    }

    if ((info.fptr = fs_fopen(path, mode)) == NULL) {
        return NULL;
    }

    return &info;
}

void end_fileinfo(struct fileinfo *info) {
    fs_fclose(info->fptr);
}

// TODO: expand spec syntax beyond just filename
// regex: ".", "*.txt", "src/*.html", 
// path base: "src/"
int file_matches_spec(const char *cwd, const char *filepath, const char *spec) {
    assert(strlen(spec) < PATH_MAX);

    char name[PATH_MAX];
    fs_path_basename(filepath, name);

    (void)cwd;
    return strcmp(spec, name) == 0;
}

int check_ignores(const char *folder, const struct fileinfo *info, const git_repo *repo) {
    int retval = 0;
    DIR *dir;
    if ((dir = fs_opendir(folder)) == NULL) {
        return -1;
    }

    fs_dirent *ent;
    char line[PATH_MAX];
    while ((ent = fs_readdir(dir, folder)) != NULL) {
        if (strcmp(ent->de_name, GIT_IGNORE_NAME) == 0) {
            FILE *fptr = fs_fopen(ent->de_path, "r");
            if (fptr == NULL) {
                retval = -1;
                goto cleanup;
            }

            while (fs_readline(line, PATH_MAX, fptr) != NULL) {
                if (file_matches_spec(folder, info->path, line)) {
                    fs_fclose(fptr);
                    return 1;
                }
            }

            fs_fclose(fptr);
        }
    }

cleanup:
    fs_closedir(dir);

    if (retval == -1) {
        return retval;
    }
    if (strcmp(repo->root_path, folder) == 0) {
        return 0;
    }
    char parent[PATH_MAX];
    fs_path_dirname(folder, parent);
    return check_ignores(parent, info, repo);
}

int is_file_ignored(const git_repo *repo, struct fileinfo *info) {
    char immediate_parent[PATH_MAX];
    fs_path_dirname(info->path, immediate_parent);
    return check_ignores(immediate_parent, info, repo);
    // goes down file's path until it hits root. at each path check .gorditignore and see if it matches file
}