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

    char out[PATH_MAX];

    #define TEST_PATH_PARENT(in, exp, ok) \
        do { \
        int res = fs_path_dirname(in, out); \
        assert(res == ok); \
        } while (0)

    TEST_PATH_PARENT("C:\\home", "C:", 0);
    TEST_PATH_PARENT("C:\\a\\b\\", "C:\\a", 0);
    TEST_PATH_PARENT("/", ".", 1);
    TEST_PATH_PARENT(".\\bob", ".", 0);
    TEST_PATH_PARENT(".\\a\\b\\c/d", ".\\a\\b\\c", 0);
    TEST_PATH_PARENT("C:/A\\b\\", "C:/A", 0);
    TEST_PATH_PARENT("C:", ".", 1);
    TEST_PATH_PARENT("C:\\", ".", 1);
    TEST_PATH_PARENT("\\\\share\\a", "\\\\share", 0);
    TEST_PATH_PARENT("file.txt", ".", 1);
    
    TEST_PATH_PARENT("", ".", 1);
    TEST_PATH_PARENT(".", ".", 1);
    TEST_PATH_PARENT("\\\\share", ".", 1);
    TEST_PATH_PARENT("/home", ".", 1);

    #define TEST_PATH_BASENAME(in, exp) \
        do { \
        fs_path_basename(in, out); \
        assert(strcmp(out, exp) == 0); \
        } while (0)

    TEST_PATH_BASENAME("C:\\home", "home");
    TEST_PATH_BASENAME("C:\\a\\b\\", "b");
    TEST_PATH_BASENAME("/", "/");
    TEST_PATH_BASENAME(".\\bob", "bob");
    TEST_PATH_BASENAME(".\\a\\b\\c/d", "d");
    TEST_PATH_BASENAME("C:/A\\b\\", "b");
    TEST_PATH_BASENAME("C:", "C:");
    TEST_PATH_BASENAME("C:\\", "C:");
    TEST_PATH_BASENAME("\\\\share\\a", "a");
    TEST_PATH_BASENAME("file.txt", "file.txt");

    TEST_PATH_BASENAME("", "");
    TEST_PATH_BASENAME(".", ".");
    TEST_PATH_BASENAME("\\\\share", "share");
    TEST_PATH_BASENAME("/home", "home");

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