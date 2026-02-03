#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "filesystem.h"
#include "objects.h" 
#include "repo.h"
#include "dircache.h"

char **parse_file_args(const char **args, int num_args) {
    
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("gordit: no command\n");
        return 0;
    }

    char *command = argv[1];

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (strcmp(command, "init") == 0) {
        int res = git_init_repo(cwd);
        if (res == -1) {
            perror("could not initalize repository");
            return 0;
        } else if (res == 1) {
            printf("Repository already exists at current working directory.\n");
            return 0;
        } else {
            printf("Initalized empty repository in current working directory.\n");
        }

        return 0;
    }
    
    int ret_code = 0;

    const git_repo *repo = get_working_repo(cwd);
    if (repo == NULL) {
        printf("fatal: not in a repository.\n");
        return 0;
    }

    if (strcmp(command, "add") == 0) {
        if (argc <= 2) {
            printf("Nothing was specified to be added.\n");
            goto end;
        }

        int num_args = argc - 2;
        char **args = argv + 2;
        char **paths = malloc(num_args * sizeof(char *));
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

            if (!ok) {
                for (int j = 0; j < i; j++) {
                    free(paths[j]);
                }
                free(temp);
                free(paths);
                ret_code = 1;
                goto end;
            }

            paths[i] = temp;
        }

        git_dircache *dircache = create_dircache(repo);
        for (int i = 0; i < num_args; i++) {
            // TODO: move file and stat handling, especially those from paths passed as arguments
            // to outside as much as possible so that file errors get handled ASAP
            if (add_file_to_dc(repo, dircache, paths[i]) != 0) {
                printf("ERROR: could not add file: %s\n", paths[i]);
                goto add_end;
            }
        }

        write_index(repo, dircache); 
        print_dircache(dircache); 

add_end:;  
        for (int j = 0; j < num_args; j++) {
            free(paths[j]);
        }
        free(paths);
        free_dircache(dircache);  
    } else if (strcmp(command, "rm")) {
        
    } else if (strcmp(command, "commit") == 0) {

    } else {
        printf("%s is not a git command.", command);
    }

end:;
    free((void *)repo);
    return ret_code;
}