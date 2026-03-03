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
        snprintf(head_ref->hash, OBJ_HASH_SIZE, "%s", *commit_hash);
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
    git_obj *blob;
    if (add_file_to_dc(dircache, info, &blob) != 0) {
        error("could not add file: %s", info->norm_path);
        return;
    }

    if (blob) {
        write_obj_to_disk(repo, blob);
    }
}

void do_remove(const git_repo *repo, git_dircache *dircache, const struct fileinfo *info) {
    (void)repo;
    if (remove_file_from_dc(dircache, info) != 0) {
       DEBUG_PRINT("removing %s from index but it is not in dircache", info->abs_path); 
    }
}

int run_init(arg_list *args) {
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

int run_add(arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));
    
    const git_repo *repo = get_working_repo(cwd);
    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    filter_ignores(result, repo);
    git_dircache *dircache = create_dircache(repo);
    do_command_each_file(repo, dircache, "rb", result, do_add);
    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return 0;
}

int run_rm(arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }
    
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
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

int run_commit(arg_list *args) {
    if (args->message == NULL) {
        return error("No commit message");
    }

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    git_dircache *dircache = create_dircache(repo);
    do_commit(repo, dircache, "Walter White", "wwhite@hotmail.com", args->message);
    free_dircache(dircache);
    free((void *)repo);

    return 0;
}

int run_status(arg_list *args) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    print_repo_status(repo);

    free((void *)repo);

    (void)args;
    return 0;
}

int run_log(arg_list *args) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    print_commit_tree(repo);
    free((void *)repo);

    (void)args;
    return 0;
}

int run_branch(arg_list *args) {
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

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
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

int run_checkout(arg_list *args) {
    if (args->cmd_args_count < 1) {
        printf("usage: %s %s <branch-name> [<pathspec>]\n", args->all[0], args->all[1]);
        return 0;
    }

    if (args->cmd_args_count <= 1) {
        return error("no branch name");
    }

    char *branch_name = args->cmd_args[0];

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));
    
    const git_repo *repo = get_working_repo(cwd);
    git_dircache *dircache = create_dircache(repo);

    pathspec_result *result = init_pathspec_result();
    for (int i = 1; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    filter_ignores(result, repo);

    // create new branch and switch to it 
    if (result->size) {
        // checkout files
        // get branch's commit's tree
        // get norm_path's hash in tree (end if is not in tree)
        // CHECK if file will be overwritten
        // restore blob using: create_file_from_blob()
        // replace its version in index
    } else {
        // checkout branch
        // do above for all files in branch's commit tree
        // CHECK for all files if they will be overwritten
        // move HEAD to point to branch
        // index made to match branch's tree
    }
 
    free((void *)repo);
    free_dircache(dircache);

    return 0;
}

int run_no_command(arg_list *args) {
    printf("usage: %s <command> [<args>]\nhelp page in progress..\n", args->all[0]);
    (void)args;
    return 0;
}