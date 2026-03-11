#ifndef COMMANDS_H
#define COMMANDS_H

#include "args.h"
#include "dircache.h"
#include "pathspec.h"
#include "refs.h"

int cmd_commit_index(const git_repo *repo, git_dircache *dircache, 
    const char *author_name, 
    const char *author_email, 
    const char *msg
);

int cmd_add(const git_repo *repo, git_dircache *dircache, const pathspec_result *result);

int cmd_rm(const git_repo *repo, git_dircache *dircache, const git_obj_tree *head_tree, const pathspec_result *result);

int cmd_diff_repo_index(const git_repo *repo, git_dircache *dircache);

int cmd_restore_files(const git_repo *repo, const git_obj_tree *target_tree, const pathspec_result *result);

int cmd_checkout_branch(const git_repo *repo, const git_obj_tree *target_tree, git_ref *target_ref);

int cmd_branch_new(const git_repo *repo, git_ref *head_ref, const char *branch_name);

int cmd_branch_delete(const git_repo *repo, const git_ref *head_ref, const char *branch_name);

int cmd_branch_list(const git_repo *repo, const git_ref *head_ref);

#endif