#include <string.h>
#include <stdio.h>

#include "filesystem.h"
#include "objects.h"
#include "global.h"

#define GIT_FOLDER  ".gordit"
#define REFS_NAME "refs"
#define OBJS_NAME "objects"
#define HEAD_NAME "HEAD"
#define INDEX_NAME "index"

static repo REPO;

// Walks up `path` until it finds a directory with git folder. Fills `repo_root` with that directory path. 
// @returns 1 on successful find and 0 if unsuccessful.
int git_find_root(char *path, char *repo_root) {
    DIR *dir;
    if ((dir = fs_opendir(path)) == NULL) {
        return 0;
    }

    fs_dirent *ent;
    while ((ent = fs_readdir(dir)) != NULL) {
        if (strcmp(ent->de_name, GIT_FOLDER) == 0) {
            strcpy(repo_root, path);
            fs_closedir(dir);
            return 1;
        }
    }
    fs_closedir(dir);

    char parent[PATH_MAX];
    if (fs_path_dirname(path, parent) != 0) {
        return 0;
    }

    return git_find_root(parent, repo_root);
}

int get_working_repo() {
    char cwd[PATH_MAX], repo_root[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return -1;
    }

    if (git_find_root(cwd, repo_root) == 0) {
        return 1;
    }

    strncpy(REPO.root_path, repo_root, PATH_MAX);

    char git_folder_path[PATH_MAX];
    fs_path_join(repo_root, GIT_FOLDER, git_folder_path);
    fs_path_join(git_folder_path, REFS_NAME, REPO.ref_path);
    fs_path_join(git_folder_path, OBJS_NAME, REPO.objects_path);
    fs_path_join(git_folder_path, INDEX_NAME, REPO.index_path);
    fs_path_join(git_folder_path, HEAD_NAME, REPO.head_path);

    return 0;
}

void hash_to_path(const obj_hash hash, char *out) {
    char path2[HASH_SIZE + 1];

    for (size_t i = HASH_SIZE + 1; i > 2; --i) {
        path2[i] = hash[i - 1];
    }
    path2[0] = hash[0];
    path2[1] = hash[1];
    path2[2] = '/';

    fs_path_join(REPO.objects_path, path2, out);
}

void free_repo() {

}