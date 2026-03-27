#ifndef PATHSPEC_H
#define PATHSPEC_H

#include "repo.h"
#include "dircache.h"
#include "commit.h"
#include "utils.h"

#define WILDCARD_RECUR_TOKEN "**"

typedef struct {
    char name[PATH_MAX];
} git_file_item;

typedef struct {
    git_file_item **items;
    int size;
    int capacity;
    int changed;
} git_file_list;

typedef struct {
    enum {
        WILD_RECUR,
        GLOB_PATT,
        STR,
    } type;
    const char *str;
    size_t len;
} pathspec_slice;

typedef struct {
    pathspec_slice *parts;
    const char *orig;
    char *fullname;
    int nparts;
} pathspec_item;

/*
options
- working tree only (add, status)
- index only (rm)
- commit/tree obj (checkout, diff)

. = 
*/

void git_fl_free(git_file_list *list);
git_file_list *git_fl_init();

int git_fl_working_tree_files(git_file_list *out, const git_repo *repo, const pathspec_item *pathspec, int tracked_only);
int git_fl_dircache_files(git_file_list *out, const git_dircache *dircache, const pathspec_item *pathspec);
int git_fl_tree_obj_files(git_file_list *out, const git_obj_tree *tree, const pathspec_item *pathspec);

void pathspec_free(pathspec_item *pathspec);

// @param relative_dir abs path of pathspec's relative directory. NULL to use current working directory
pathspec_item *pathspec_parse(const git_repo *repo, const char *relative_dir, const char *arg);

int pathspec_full_matches(const pathspec_item *pathspec, const char *name);

strarr_t *repo_all_files(const git_repo *repo, int tracked_only);

#endif