#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include "filesystem.h"
#include "objects.h"
#include "repo.h"
#include "utils.h"
#include "logging.h"
#include "refs.h"
#include "dircache.h"
#include "commit.h"
#include "inifile.h"

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int stat_mode_to_git(unsigned int st_mode) {
    if (S_ISDIR(st_mode)) {
        return GIT_MODE_DIR;
    } 
    if (st_mode & S_IXUSR) {
        return GIT_MODE_FILE_X;
    } 
    return GIT_MODE_FILE_R;
}

unsigned int git_mode_to_stat(unsigned int git_mode) {
    return git_mode & 0x1FF;
}

// Walks up `path` until it finds a directory with git folder. Fills `repo_root` with that directory path. 
// @returns 1 on successful find and 0 if unsuccessful.
int git_find_root(const char *path, char *repo_root) {
    char git_path[PATH_MAX];
    fs_path_join(path, HEAD_PATH, git_path);
    if (fs_file_exists(git_path)) {
        snprintf(repo_root, PATH_MAX, "%s", path);
        return 1;
    }

    char parent[PATH_MAX];
    fs_path_dirname(path, parent);
    if (strcmp(path, parent) == 0) {
        return 0;
    }

    return git_find_root(parent, repo_root);
}

void init_repo_context(git_repo *repo, const char *repo_root) {
    snprintf(repo->root_path, PATH_MAX, "%s", repo_root);

#define X(name, path) fs_path_join(repo_root, path, repo->name);
    REPO_PATHS
#undef X
}

const git_repo *git_repo_init() {
    char cwd[PATH_MAX];
    sgetcwd(cwd, PATH_MAX);
    git_path_clean(cwd);

    char repo_root[PATH_MAX];
    if (!git_find_root(cwd, repo_root)) {
        fatal("not in a repository");
    }

    git_repo *repo = smalloc(sizeof(*repo));
    init_repo_context(repo, repo_root);
    return repo;
}

int repo_folder_initialize(char *repo_root, char *start_branch, int bare) {
    git_path_clean(repo_root);
    git_repo *repo = smalloc(sizeof *repo);
    init_repo_context(repo, repo_root);

    int exists = fs_file_exists(repo->head_path);

    mode_t mode = 0700;
    if (fs_mkdir(repo->git_path, mode) == -1 ||
        fs_mkdir(repo->objects_path, mode) == -1 ||
        fs_mkdir(repo->refs_path, mode) == -1 ||
        fs_mkdir(repo->local_refs_path, mode) == -1 ||
        fs_mkdir(repo->tag_refs_path, mode) == -1) {
        fatal("could not initialize repository: %s", strerror(errno));
    }

    inifile *config = inifile_read(repo->config_path);
    if (config->items_size == 0) {
        inifile_update(config, "core", "bare", bare ? "true" : "false");
        inifile_write(config, repo->config_path);
    }
    inifile_free(config);

    if (!exists) {
        git_ref start_ref = { 
            .type = REF_LOCAL,
            .name = start_branch ? start_branch : START_BRANCH_DEFAULT,
        };
        move_head(repo, &start_ref);
    }

    free(repo);
   
    return exists;
}


int obj_store_path(const git_repo *repo, const obj_hash hash, char *out) {
    char path2[sizeof(obj_hash) + 1];

    for (size_t i = sizeof(obj_hash); i > 2; --i) {
        path2[i] = hash[i - 1];
    }
    path2[0] = hash[0];
    path2[1] = hash[1];
    path2[2] = '/';
    fs_path_join(repo->objects_path, path2, out);
    
    if (fs_file_exists(out)) {
        return 1;
    }
    
    char parent_path[PATH_MAX];
    fs_path_dirname(out, parent_path);
    if (fs_mkdir(parent_path, 0700) == -1) {
        fatal("could not create %s: %s", parent_path, strerror(errno));
    }

    return 0;
}

void repo_rel_path(const git_repo *repo, const char *abs_path, char *out) {
    int root_len = strlen(repo->root_path);

    int bad = 0;
    for (int i = 0; i < root_len; i++) {
        if (repo->root_path[i] == '/' || repo->root_path[i] == '\\') {
            bad = abs_path[i] != '/' && abs_path[i] != '\\';
        } else {
            bad = tolower(abs_path[i]) != tolower(repo->root_path[i]);
        }
    }
    if (bad) {
        fatal("'%s' is outside of current repository (%s)", abs_path, repo->root_path);
    }

    snprintf(out, PATH_MAX, "%s", abs_path + root_len + 1);
     
    char *ptr = out;
    while (*ptr != '\0') {
        if (*ptr == '\\') {
            *ptr = '/';
        }
        ptr++;
    }
}

void repo_full_path(const git_repo *repo, const char *norm_path, char *out) {
    fs_path_join(repo->root_path, norm_path, out);

    char *ptr = out;
    while (*ptr != '\0') {
        if (*ptr == '\\') {
            *ptr = '/';
        }
        ptr++;
    }
}

void git_path_clean(char *path) {
    int len = strlen(path);
    for (int i = 0; i < PATH_MAX && i < len; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        } else {
            path[i] = tolower(path[i]);
        }
    }
}

const char *git_global_config_path() {
    static char path[PATH_MAX];
    const char *home_path = fs_user_homepath();
    if (!home_path) {
        fatal("could not get home directory");
    }

    fs_path_join(home_path, GLOBAL_CONFIG_NAME, path);

    return path;
}