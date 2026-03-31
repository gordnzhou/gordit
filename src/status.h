#ifndef STATUS_H
#define STATUS_H

#include "repo.h"
#include "pathspec.h"

void print_repo_status(const git_repo *repo, git_file_list *show_files);

void print_commit_tree(const git_repo *repo, const obj_hash commit_hash, const char *colour);

#endif