#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "filesystem.h"
#include "repo.h"
#include "dircache.h"
#include "filespec.h"
#include "logging.h"
#include "utils.h"
#include "refs.h"
#include "commit.h"
#include "diff.h"

int do_commit(const git_repo *repo, 
    git_dircache *dircache, 
    char *author_name, char *author_email, 
    char *msg
) {
    if (dircache_has_conflicts(dircache)) {
        return error("there are unmerged files in index, cannot commit");
    }

    obj_hash parent_hash;
    int num_parents = 1;
    char *head_content = malloc(256);
    int is_detached = read_head(repo, head_content, 256);
    char *head_ref_path = NULL;
    if (is_detached) {
        warn("head is not pointing to any branch, commit will be easily lost!");
        string_to_hash(&parent_hash, head_content);
    } else {
        head_ref_path = head_content;
        if (read_ref(repo, head_ref_path, &parent_hash) < 0) {
            num_parents = 0;
        }
    }

    if (num_parents == 1) {
        git_obj_commit *parent_commit = create_commit_from_disk(repo, parent_hash);
        git_obj_tree *parent_tree = create_tree_from_disk(repo, parent_commit->tree_hash);
        if (is_tree_and_dc_same(dircache, parent_tree)) {
            free(head_content);
            free_tree(parent_tree);
            free_commit(parent_commit);
            return info("no changes added to commit");
        }

        free_tree(parent_tree);
        free_commit(parent_commit);
    }

    git_obj_tree *tree = build_tree_from_index(dircache);
    if (tree->size == 0) {
        free(head_content);
        free_tree(tree);
        return info("empty tree, nothing to commit.");
    } 

    git_obj_commit *commit = create_commit(tree, num_parents, &parent_hash,
        author_name, author_email, msg);
    git_obj *commit_obj = create_commit_obj(commit);
    obj_hash *commit_hash = &(commit_obj->hash);

    write_obj_to_disk(repo, commit_obj);
    write_tree_to_disk(repo, tree, 1);
    print_tree(tree);
    print_commit(commit);
    printf("COMMIT: %s\n", *commit_hash);

    if (is_detached) {
        detach_head(repo, commit_hash);
    } else {
        write_ref(repo, head_ref_path, commit_hash);
    }

    free_obj(commit_obj);
    free(commit);
    free_tree(tree);
    free(head_content);
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("usage: gordit <command> [<args>]\n");
        return 0;
    }

    char *command = argv[1];

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    if (strcmp(command, "init") == 0) {
        int res = create_repo_folder(cwd);
        if (res == -1) {
            fatal("could not initalize repository");
        } else if (res == 1) {
            printf("Existing repository detected, reinitialized\n");
        } else {
            printf("Initalized empty repository in %s\n", cwd);
        }
        return 0;
    }
    
    int ret_code = 0;

    const git_repo *repo = get_working_repo(cwd);
    if (repo == NULL) {
        fatal("not in a repository");
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
                error("could not open file: %s", argv[2 + i]);
                goto add_end;
            }

            if (strcmp(command, "add") == 0 && is_file_ignored(repo, info)) {
                continue;
            }
            
            if (strcmp(command, "add") == 0) {
                if (add_file_to_dc(dircache, info) != 0) {
                    error("could not add file: %s", argv[2 + i]);
                    end_fileinfo(info);
                    goto add_end;
                }
            } else {
                if (remove_file_from_dc(dircache, info) != 0) {
                    error("'%s' is not in index", argv[2 + i]);
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
        if (argc <= 2) {
            fatal("no commit message");
        }

        git_dircache *dircache = create_dircache(repo);
        do_commit(repo, dircache, "Walter White", "wwhite@hotmail.com", argv[2]);
        free_dircache(dircache);
   } else {
        info("%s is not a git command.", command);
    }

end:;
    free((void *)repo);
    return ret_code;
}