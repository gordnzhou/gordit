#ifndef STATUS_H
#define STATUS_H

#include "repo.h"

void print_repo_status(const git_repo *repo);

void print_commit_tree(const git_repo *repo);

#endif