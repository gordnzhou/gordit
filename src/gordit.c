#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "filesystem.h"

#define GIT_FOLDER_NAME ".gordit"

int git_init_repo(char *root_folder);
int git_find_root(char *path, char *repo_root);

int git_init_repo(char *root_folder) {
    (void)root_folder;
    return 0;
}

int git_find_root(char *path, char *repo_root) {
    char path_copy[PATH_MAX];
    strcpy(path_copy, path);

    DIR *dir;
    if ((dir = fs_opendir(path_copy)) == NULL) {
        printf("Could not open file: %s\n", path);
        return 1;
    }

    printf("files in %s:\n", path);
    struct dirent *ent;
    while ((ent = fs_readdir(dir)) != NULL) {
        printf("%s\n", ent->d_name);
    }

    fs_closedir(dir);

    char parent[PATH_MAX];
    if (fs_path_dirname(path, parent) == 0) {
        git_find_root(parent, repo_root);
    }

    (void)repo_root;
    return 0;
}

int main(int argc, char* argv[]) {
    char cwd[PATH_MAX];
    char repo_root[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (git_find_root(cwd, repo_root) == 0) {
        perror("fatal: already in a repository");
        return 1;
    }

    if (git_init_repo(cwd)) {
        perror("could not initalize repository");
        return 1;
    }

    printf("Initalized empty repository in %s\n", cwd);

    (void)argc;
    (void)argv;
    return 0;
}