#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "filesystem.h"
#include "objects.h" 
#include "global.h"

// If git folder exists in cwd do nothing.
// Otherwise create git folder in cwd, including subfolders, index and HEAD.
// @returns 0 on success, 1 if git folder already in cwd, -1 otherwise. 
int git_init_repo() {
    char cwd[PATH_MAX];

    if (fs_getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return -1;
    }

    char head_path[PATH_MAX];
    fs_path_join(cwd, ".gordit/HEAD", head_path);

    if (fs_file_exists(head_path) == 1) {
        return 1;
    }

    fs_mkdir(".gordit", 0700);
    fs_mkdir(".gordit/refs", 0700);
    fs_mkdir(".gordit/objects", 0700);

    FILE *f_ptr;
    f_ptr = fs_fopen(".gordit/HEAD", "w");
    fs_fclose(f_ptr);
    f_ptr = fs_fopen(".gordit/index", "w");
    fs_fclose(f_ptr);
   
    return 0;
}

int main(int argc, char* argv[]) {
    int res = get_working_repo();
    if (res == 1) {
        printf("fatal: not in a repository.\n");
        return 0;
    } else if (res == -1) {
        printf("Could not find repository.\n");
        return 1;
    }

    // int res = git_init_repo();
    // if (res == -1) {
    //     perror("could not initalize repository");
    //     return 0;
    // } else if (res == 1) {
    //     printf("Repository already exists at current working directory.\n");
    //     return 0;
    // } else {
    //     printf("Initalized empty repository in current working directory.\n");
    // }

    (void)argc;
    (void)argv;
    return 0;
}