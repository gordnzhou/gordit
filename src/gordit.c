#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "filesystem.h"
#include "objects.h" 


#define GIT_FOLDER  ".gordit"
#define REFS_NAME "refs"
#define OBJS_NAME "objects"
#define HEAD_NAME "HEAD"

// If git folder exists in cwd do nothing.
// Otherwise create git folder in cwd, including subfolders, index and HEAD.
// @returns 0 on success, 1 if git folder already in cwd, -1 otherwise. 
int git_init_repo() {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return -1;
    }

    char head_path[PATH_MAX];
    if (fs_path_join(cwd, ".gordit/HEAD", head_path) == -1) {
        return -1;
    }

    if (fs_file_exists(head_path) == 1) {
        return 1;
    }

    mkdir(".gordit");
    mkdir(".gordit/refs");
    mkdir(".gordit/objects");

    FILE *f_ptr;
    f_ptr = fs_fopen(".gordit/HEAD", "w");
    fs_fclose(f_ptr);
    f_ptr = fs_fopen(".gordit/index", "w");
    fs_fclose(f_ptr);
   
    return 0;
}

// Walks up `path` until it finds a directory with git folder. Fills `repo_root` with that directory path. 
// @returns 1 on successful find and 0 if unsuccessful.
int git_find_root(char *path, char *repo_root) {
    char path_copy[PATH_MAX];
    strcpy(path_copy, path);

    DIR *dir;
    if ((dir = fs_opendir(path_copy)) == NULL) {
        return 0;
    }

    struct dirent *ent;
    while ((ent = fs_readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, GIT_FOLDER) == 0) {
            strcpy(path, repo_root);
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

int main(int argc, char* argv[]) {
    char cwd[PATH_MAX];
    char repo_root[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (git_find_root(cwd, repo_root)) {
        printf("In a repository with its root path: %s\n", repo_root);
    } else {
        printf("Not in a repository.\n");
    }

    int res = git_init_repo();
    if (res == -1) {
        perror("could not initalize repository");
        return 0;
    } else if (res == 1) {
        printf("Repository already exists at current working directory.\n");
        return 0;
    } else {
        printf("Initalized empty repository in current working directory.");
    }

    (void)argc;
    (void)argv;
    return 0;
}