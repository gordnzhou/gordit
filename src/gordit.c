#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "filesystem.h"
#include "objects.h" 
#include "repo.h"
#include "dircache.h"

int main(int argc, char* argv[]) {
    int is_error;
    const git_repo *repo = get_working_repo(&is_error);
    if (repo == NULL) {
        if (is_error) {
            printf("Could not find repository.\n");
            return 1;
        } else {
            printf("fatal: not in a repository.\n");
            return 0;
        }
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

    free((void *)repo);

    (void)argc;
    (void)argv;
    return 0;
}