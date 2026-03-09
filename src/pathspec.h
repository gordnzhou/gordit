#ifndef PATHSPEC_H
#define PATHSPEC_H

#include "repo.h"
#include "utils.h"

typedef struct {
    char name[PATH_MAX];
    char abs_path[PATH_MAX];
    int  exists;
    int  ignored;
} git_file_arg;

typedef struct {
    int size;
    int capacity;
    git_file_arg **args;
} pathspec_result;

pathspec_result *init_pathspec_result();

// expands arg to file path(s) and adds them to result
void expand_arg(pathspec_result *result, const git_repo *repo, const char *arg);

strarr_t *repo_all_files(const git_repo *repo, int tracked_only);

void free_pathspec_result(pathspec_result *result);

#endif