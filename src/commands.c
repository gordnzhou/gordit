#include <string.h>

#include "commit.h"
#include "diff.h"
#include "dircache.h"
#include "filesystem.h"
#include "fileinfo.h"
#include "logging.h"
#include "status.h"
#include "refs.h"
#include "pathspec.h"
#include "utils.h"
#include "args.h"

int do_commit(const git_repo *repo, 
    git_dircache *dircache, 
    char *author_name, 
    char *author_email, 
    const char *msg
) {
    if (dircache_has_conflicts(dircache)) {
        return error("there are unmerged files in index, cannot commit");
    }
    
    git_obj_tree *tree = build_tree_from_index(dircache);
    if (tree->size == 0) {
        warn("commiting an empty tree..");
    }

    git_ref *head_ref = read_head(repo);
    if (is_head_detached(head_ref)) {
        warn("head is not pointing to any branch, commit will be easily lost!");
    }

    // TODO: read MERGE_HEAD for possibly 1+ other commit parents
    if (!head_ref->empty_hash) {
        git_obj_commit *parent_commit = create_commit_from_disk(repo, head_ref->hash);
        git_obj_tree *parent_tree = read_tree_from_disk(repo, parent_commit->tree_hash);
        if (is_tree_and_dc_same(dircache, parent_tree)) {
            free_ref(head_ref);
            free_tree(parent_tree);
            free_commit(parent_commit);
            return info("nothing new for me to commit!");
        }

        free_tree(parent_tree);
        free_commit(parent_commit);
    }

    int num_parents = head_ref->empty_hash ? 0 : 1;
    obj_hash *parents = head_ref->empty_hash ? NULL : &(head_ref->hash); 

    git_obj_commit *commit = create_commit(tree, num_parents, parents, author_name, author_email, msg);
    git_obj *commit_obj = create_commit_obj(commit);
    obj_hash *commit_hash = &(commit_obj->hash);

    write_obj_to_disk(repo, commit_obj);
    write_tree_to_disk(repo, tree, 1);
    // print_tree(tree);

    print_commit(commit);

    if (head_ref->type == DIRECT) {
        copy_hash(&(head_ref->hash), commit_hash);
        move_head(repo, head_ref);
    } else {
        write_ref(repo, head_ref->type, head_ref->name, commit_hash);
    }

    free_obj(commit_obj);
    free(commit);
    free_tree(tree);
    free_ref(head_ref);
    
    return 0;
}

void do_command_each_file(const git_repo *repo, git_dircache *dircache, 
    char *mode, const pathspec_result *in, 
    void (*cmd_cb)(const git_repo *, git_dircache *, const struct fileinfo *)) {
    
    for (int i = 0; i < in->size; i++) {
        struct fileinfo *info;
        char *norm_path = in->norm_paths[i];
        if ((info = start_fileinfo(repo, norm_path, mode)) == NULL) {
            error("could not open file: %s", norm_path);
            continue;
        }

        (*cmd_cb)(repo, dircache, info);
        
        end_fileinfo(info);
    }
}



void do_add(const git_repo *repo, git_dircache *dircache, const struct fileinfo *info) {
    if (dircache_matching_entry(dircache, info)) {
        DEBUG_PRINT("skipping add to '%s' to index", info->norm_path);
        return;
    }

    git_obj *blob = create_blob_from_file(info);
    if (!blob) {
        fatal("could not add '%s': blob creation failed", info->norm_path);
    }

    dircache_add(dircache, info, blob);

    write_obj_to_disk(repo, blob);
    printf("wrote %s to disk for %s\n", blob->hash, info->norm_path);

    free_obj(blob);
}

void do_remove(const git_repo *repo, git_dircache *dircache, const struct fileinfo *info) {
    git_index_entry **in_index = dircache_find_file(dircache, info->norm_path);
    if (!in_index) {
        return;
    }

    git_index_entry *entry = *in_index;
    if (is_file_changed_unstaged(entry, info)) {
        fatal("'%s' has unstaged changes", entry->name);
    }
    // TODO: also check that entry->hash == hash of latest commit's version or FAIL

    (void)repo;
    if (dircache_remove(dircache, info->norm_path) != 0) {
       DEBUG_PRINT("removing '%s' from index but it is not in dircache", info->abs_path); 
    }

    sremove(info->abs_path);
}

int run_init(const arg_list *args) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    if (create_repo_folder(cwd) == 1) {
        printf("Existing repository detected, reinitialized\n");
    } else {
        printf("Initalized empty repository in %s\n", cwd);
    }

    (void)args;
    return 0;
}

int run_add(const arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }

    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    // show warning for filtered ignores
    filter_ignores(result, repo);
    do_command_each_file(repo, dircache, "rb", result, do_add);
    print_dircache(dircache);
    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return 0;
}

int run_rm(const arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }
    
    const git_repo *repo = repo_init_context();
    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    git_dircache *dircache = create_dircache(repo);
    do_command_each_file(repo, dircache, "rb", result, do_remove);
    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return 0;
}

int run_commit(const arg_list *args) {
    if (args->message == NULL) {
        return error("No commit message");
    }

    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);
    do_commit(repo, dircache, "Walter White", "wwhite@hotmail.com", args->message);
    free_dircache(dircache);
    free((void *)repo);

    return 0;
}

int run_status(const arg_list *args) {
    const git_repo *repo = repo_init_context();
    print_repo_status(repo);

    free((void *)repo);

    (void)args;
    return 0;
}

int run_log(const arg_list *args) {
    const git_repo *repo = repo_init_context();
    print_commit_tree(repo);
    free((void *)repo);

    (void)args;
    return 0;
}

int run_branch(const arg_list *args) {
    // TODO: change to use args
    typedef enum {
        LIST,
        NEW,
        DELETE,
    } branch_op;
    branch_op op = LIST;
    if (args->cmd_args_count) {
        char *subop = args->cmd_args[0];
        if (strcmp(subop, "add") == 0) {
            op = NEW;
        } else if (strcmp(subop, "rm") == 0) {
            op = DELETE;
        } else if (strcmp(subop, "list") == 0) {
            op = LIST;
        } else {
            fatal("invalid branch arg, must be one of: add, rm, list");
        }
    } 
    char *branch_name = NULL;
    if (args->cmd_args_count > 1)  {
        branch_name = args->cmd_args[1];
    }

    const git_repo *repo = repo_init_context();
    git_ref *head_ref = read_head(repo);

    switch (op) {
        case LIST: {
            strarr_t *ref_names = refs_all_names(repo, REF_LOCAL);
            for (size_t i = 0; i < ref_names->len; i++) {
                char *ref_name = ref_names->data[i];
                int is_current = head_ref->type != DIRECT ? strcmp(ref_name, head_ref->name) == 0 : 0;
                printf("%s %s\n", is_current ? "*" : " ", ref_name);
            } 
            strarr_free(ref_names);
        }
            break;
        case NEW:
            if (branch_name == NULL) {
                error("specify name of branch to add!");
                break;
            }
            if (!is_valid_branch_name(branch_name)) {
                error("invalid branch name '%s'", branch_name);
                break;
            }
            if (head_ref->empty_hash) {
                fatal("cannot create new branch: HEAD has no commits");
            }
            write_ref(repo, REF_LOCAL, branch_name, &(head_ref->hash));
            printf("created branch '%s' -> %s\n", branch_name, head_ref->hash);
            break;
        case DELETE:
            if (branch_name == NULL) {
                error("specify name of branch to remove!");
                break;
            }
            if (strcmp(head_ref->name, branch_name) == 0) {
                fatal("cannot remove current branch HEAD is pointing to!");
            }
            // TODO: check that branch's tip commit is reachable from other branches
            if (del_ref(repo, REF_LOCAL, branch_name)) {
                printf("removed branch '%s'\n", branch_name);
            } else {
                error("no branch named '%s' to remove", branch_name);
            }
            break;
    }

    free((void *)repo);
    free_ref(head_ref);

    return 0;
}

int run_checkout(const arg_list *args) {
    if (args->cmd_args_count < 1) {
        printf("usage: %s %s <branch-name> [<pathspec>]\n", args->all[0], args->all[1]);
        return 0;
    }

    if (args->cmd_args_count < 1) {
        return error("no branch name");
    }

    const git_repo *repo = repo_init_context();
    pathspec_result *result = init_pathspec_result();
    for (int i = 1; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }

    char *branch_name = args->cmd_args[0];
        
    int status = 0;

    git_ref* ref = read_ref(repo, REF_LOCAL, branch_name, 1);
    git_obj_commit *target_commit = create_commit_from_disk(repo, ref->hash);
    git_obj_tree *target_tree = read_tree_from_disk(repo, target_commit->tree_hash);

    char full_path[PATH_MAX];
    if (result->size) {
        for (int i = 0; i < result->size; i++) {
            git_tree_entry **in_other = tree_find_blob(target_tree, result->norm_paths[i]);
            if (in_other == NULL) {
                error("'%s' is not in %s's snapshot", result->norm_paths[i], branch_name);
                status = 1;
                continue;
            }
            git_tree_entry *entry = *in_other;
            assert(entry->type == OBJ_TYPE_BLOB);
           
            repo_full_path(repo, entry->name, full_path);
            git_obj *blob = smalloc(sizeof(*blob));
            create_obj_from_disk(blob, repo, entry->u.blob_hash, OBJ_TYPE_BLOB);

            if (create_file_from_blob(full_path, blob)) {
                fatal("failed to restore '%s' to object %s", full_path, blob->hash);
            }

            DEBUG_PRINT("restored '%s' to object %s", full_path, blob->hash);
            free_obj(blob);
        }
    } else {
        git_ref *head_ref = read_head(repo);
        if (head_ref->type != DIRECT) {
            if (strcmp(head_ref->name, ref->name) == 0) {
                warn("already on branch '%s'", branch_name);
            }
        }

        git_dircache *dircache = create_dircache(repo);
        git_obj_commit *head_commit = head_ref->empty_hash ? NULL : create_commit_from_disk(repo, head_ref->hash);
        git_obj_tree *head_tree = head_commit ? read_tree_from_disk(repo, head_commit->tree_hash) : NULL;

        int head_size = tree_num_blobs(head_tree);
        int target_size = tree_num_blobs(target_tree);

        git_diff *diff = diff_new(head_size + target_size);
        diff_trees(diff, target_tree, target_size, head_tree, head_size);
        for (int i = 0; i < diff->size; i++) {
            diff_entry *entry = &(diff->entries[i]);
            git_index_entry **in_index = dircache_find_file(dircache, entry->name);
            if (in_index) {
                git_index_entry *index_entry = *in_index;
                if (entry->type == MODIFIED && strcmp(*(entry->new_hash), index_entry->hash) != 0 && strcmp(*(entry->old_hash), index_entry->hash) != 0) {
                    fatal("cannot checkout: '%s' still has uncommitted changes!", entry->name);
                }
                if (diff_file_repo_index(repo, index_entry) == 1) {
                    fatal("cannot checkout: '%s' still has unstaged changes!", entry->name);
                }
            }
        }

        char full_path[PATH_MAX];
        for (int i = 0; i < diff->size; i++) {
            diff_entry *entry = &(diff->entries[i]);
            fs_path_join(repo->root_path, entry->name, full_path);

            if (entry->type == REMOVED) {
                if (fs_file_exists(full_path)) {
                    sremove(full_path);
                }
                dircache_remove(dircache, entry->name);
            } else {
                git_obj *blob = smalloc(sizeof(*blob));
                create_obj_from_disk(blob, repo, *(entry->new_hash), OBJ_TYPE_BLOB);

                if (create_file_from_blob(full_path, blob)) {
                    fatal("failed to restore '%s' to object %s", entry->name, blob->hash);
                }

                struct fileinfo *info;
                if ((info = start_fileinfo(repo, entry->name, "rb")) == NULL) {
                    fatal("could not open file: %s", entry->name);
                }
                
                dircache_add(dircache, info, blob);
                DEBUG_PRINT("restored '%s' to object %s", full_path, blob->hash);
                free_obj(blob);
                end_fileinfo(info);
            }
        }

        write_index(repo, dircache);
        move_head(repo, ref);

        free_dircache(dircache);
        free_ref(head_ref);
        if (head_commit) free_commit(head_commit);
        if (head_tree) free_tree(head_tree);
    }
 
    free((void *)repo);
    free_ref(ref);
    free_commit(target_commit);
    free_tree(target_tree);

    return status;
}

int run_diff(const arg_list *args) {
    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    strarr_t *files_all = repo_all_files(repo, 1);
    git_diff *diff = diff_new(dircache->num_entries + files_all->len);
    diff_repo_index(diff, repo, files_all, dircache);

    for (int i = 0; i < diff->size; i++) {
        diff_entry *entry = diff->entries + i;
        if (entry->type == REMOVED) {
            
        } else if (entry->type == ADDED) {

        } else if (entry->type == MODIFIED) {

        }
    }

    diff_free(diff);
    strarr_free(files_all);
    free_dircache(dircache);
    free((void *)repo);

    (void)args;
    return 0;
}

int run_no_command(const arg_list *args) {
    printf("usage: %s <command> [<args>]\nhelp page in progress..\n", args->all[0]);
    (void)args;
    return 0;
}