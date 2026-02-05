#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "filesystem.h"
#include "objects.h" 
#include "repo.h"
#include "dircache.h"
#include "filespec.h"

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

    if (strcmp(command, "add") == 0 || strcmp(command, "rm") == 0) {
        if (argc <= 2) {
            printf("No files specified, nothing changed.\n");
            goto end;
        }
        int num_args = argc - 2;
        if (!is_file_args_valid(repo, argv + 2, num_args)) { 
            ret_code = 1;
            goto end;
        }

        git_dircache *dircache = create_dircache(repo);
        for (int i = 0; i < num_args; i++) {
            struct fileinfo *info;
            if ((info = start_fileinfo(repo, argv[2 + i], "rb")) == NULL) {
                ret_code = 1;
                printf("ERROR: could not open file: %s\n", argv[2 + i]);
                goto add_end;
            }

            if (strcmp(command, "add") == 0 && is_file_ignored(repo, info)) {
                printf("NOTE: file is ignored: %s\n", argv[2 + i]);
                continue;
            }
            
            if (strcmp(command, "add") == 0) {
                if (add_file_to_dc(dircache, info) != 0) {
                    printf("ERROR: could not add file: %s\n", argv[2 + i]);
                    end_fileinfo(info);
                    goto add_end;
                }
            } else {
                if (remove_file_from_dc(dircache, info) != 0) {
                    printf("ERROR: could not find file: %s\n", argv[2 + i]);
                    end_fileinfo(info);
                    goto add_end;
                }
            }

            end_fileinfo(info);
        }

        write_index(repo, dircache); 
        print_dircache(dircache); 

add_end:;  
        free_dircache(dircache);  
    } else if (strcmp(command, "commit") == 0) {

    } else {
        printf("%s is not a git command.", command);
    }

end:;
    free((void *)repo);
    return ret_code;
}