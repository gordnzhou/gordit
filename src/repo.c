#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "filesystem.h"
#include "objects.h"
#include "repo.h"
#include "utils.h"
#include "logging.h"
#include "refs.h"

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

const git_repo *repo_init_context() {
    char cwd[PATH_MAX];
    sgetcwd(cwd, PATH_MAX);
    path_clean_seps(cwd);

    char repo_root[PATH_MAX];
    if (!git_find_root(cwd, repo_root)) {
        fatal("not in a repository");
    }

    git_repo *repo = smalloc(sizeof(*repo));
    init_repo_context(repo, repo_root);
    return repo;
}

int create_repo_folder(char *repo_root) {
    path_clean_seps(repo_root);
    git_repo *repo = smalloc(sizeof *repo);
    init_repo_context(repo, repo_root);

    int exists = fs_file_exists(repo->head_path);

    if (exists) {
        // TODO: reinitalize repo
        free(repo);
        return exists;
    }

    mode_t mode = 0700;
    if (fs_mkdir(repo->git_path, mode) == -1 ||
        fs_mkdir(repo->objects_path, mode) == -1 ||
        fs_mkdir(repo->refs_path, mode) == -1 ||
        fs_mkdir(repo->local_refs_path, mode) == -1 ||
        fs_mkdir(repo->tag_refs_path, mode) == -1) {
        fatal("could not initialize repository: %s", strerror(errno));
    }

    git_ref start_ref = { 0 };
    start_ref.type = REF_LOCAL;
    start_ref.name = START_BRANCH;
    move_head(repo, &start_ref);

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
    int path_len = strlen(abs_path);
    int rel_len = path_len - root_len;

    assert(rel_len > 0 && memcmp(repo->root_path, abs_path, root_len) == 0);

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