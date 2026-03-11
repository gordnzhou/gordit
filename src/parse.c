#include <string.h>

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
#include "textdiff.h"
#include "commit.h"
#include "commands.h"

int run_init(const arglist_init_t *arglist) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    if (create_repo_folder(cwd) == 1) {
        printf("Existing repository detected, reinitialized\n");
    } else {
        printf("Initalized empty repository in %s\n", cwd);
    }

    (void)arglist;
    return 0;
}

int run_add(const arglist_add_t *arglist) {
    if (arglist->ps_size <= 0) {
        return info("No files specified, nothing added.");
    }

    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < arglist->ps_size; i++) {
        expand_arg(result, repo, arglist->pathspecs[i]);
    }

    int status = cmd_add(repo, dircache, result);

    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return status;
}

int run_rm(const arglist_rm_t *arglist) {
    if (arglist->ps_size <= 0) {
        return info("No files specified, nothing added.");
    }
    
    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    git_ref *head_ref = read_head(repo);
    git_obj_commit *head_commit = head_ref->empty_hash ? NULL : read_commit_from_disk(repo, head_ref->hash);
    git_obj_tree *head_tree = head_commit ? read_tree_from_disk(repo, head_commit->tree_hash) : NULL;

    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < arglist->ps_size; i++) {
        expand_arg(result, repo, arglist->pathspecs[i]);
    }

    int status = cmd_rm(repo, dircache, head_tree, result);

    write_index(repo, dircache);

    free_ref(head_ref);
    if (head_tree) free_tree(head_tree);
    if (head_commit) free_commit(head_commit);
    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return status;
}

int run_commit(const arglist_commit_t *arglist) {
    if (arglist->message == NULL) {
        return error("No commit message");
    }

    // TODO: get default from global config
    const char *author = arglist->name ? arglist->name : "Walter White";
    const char *auth_email = arglist->email ? arglist->email : "wwhite@hotmail.com";

    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    int status = cmd_commit_index(repo, dircache, author, auth_email, arglist->message);

    free_dircache(dircache);
    free((void *)repo);

    return status;
}

int run_status(const arglist_status_t *arglist) {
    const git_repo *repo = repo_init_context();
    print_repo_status(repo);

    free((void *)repo);

    (void)arglist;
    return 0;
}

int run_log(const arglist_log_t *arglist) {
    const git_repo *repo = repo_init_context();
    print_commit_tree(repo);
    free((void *)repo);

    (void)arglist;
    return 0;
}

int run_branch(const arglist_branch_t *arglist) {
    const git_repo *repo = repo_init_context();
    git_ref *head_ref = read_head(repo);

    int status = 0;
    if (arglist->new_branch != NULL) {
        status = cmd_branch_new(repo, head_ref, arglist->new_branch);
    } else if (arglist->del_branch != NULL) {
        status = cmd_branch_delete(repo, head_ref, arglist->del_branch);
    } else {
        status = cmd_branch_list(repo, head_ref);
    }

    free((void *)repo);
    free_ref(head_ref);

    return status;
}

int run_checkout(const arglist_checkout_t *arglist) {
    if (arglist->branch && arglist->ps_size) {
        return error("cannot specify both a branch and file(s) to checkout");
    } else if (arglist->branch == NULL && arglist->ps_size == 0) {
        return error("nothing to checkout");
    }
    char *branch_name = arglist->branch;

    const git_repo *repo = repo_init_context();
    git_ref* target_ref = read_ref(repo, REF_LOCAL, branch_name, 1);
    git_obj_commit *target_commit = read_commit_from_disk(repo, target_ref->hash);
    git_obj_tree *target_tree = read_tree_from_disk(repo, target_commit->tree_hash);

    int status = 0;
    if (arglist->ps_size) {
        pathspec_result *result = init_pathspec_result();
        for (int i = 1; i < arglist->ps_size; i++) {
            expand_arg(result, repo, arglist->pathspecs[i]);
        }
        
        status = cmd_restore_files(repo, target_tree, result);
    } else {
        status = cmd_checkout_branch(repo, target_tree, target_ref);
    }
 
    free((void *)repo);
    free_ref(target_ref);
    free_commit(target_commit);
    free_tree(target_tree);

    return status;
}

int run_diff(const arglist_diff_t *arglist) {
    const git_repo *repo = repo_init_context();
    git_dircache *dircache = create_dircache(repo);

    int status = cmd_diff_repo_index(repo, dircache);

    free_dircache(dircache);
    free((void *)repo);

    (void)arglist;
    return status;
}