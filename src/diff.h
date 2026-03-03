#ifndef DIFF_H
#define DIFF_H

#include "tree.h"
#include "dircache.h"
#include "utils.h"

enum diff_type {
    MODIFIED,
    ADDED,
    REMOVED,
};

typedef struct {
    enum diff_type type;
    char *name;
} diff_entry;

typedef struct {
    int size;
    diff_entry *entries;
} git_diff;

// @return 1 if index will produce the same tree as the tree parameter
int is_tree_and_dc_same(const git_dircache *index, const git_obj_tree *tree);

// index is NEW and tree is OLD
void diff_index_tree(git_diff *out, const git_dircache *dircache, const git_obj_tree *tree, int tree_size);

// repo is NEW and dircache is OLD
void diff_repo_index(git_diff *out, const git_repo *repo, const strarr_t *repo_files, const git_dircache *dircache);

// checks index entries in repo
// repo is NEW and dircache is OLD
void diff_repo_index_tracked(git_diff *out, const git_repo *repo, const git_dircache *dircache);

// checks for new files (files in working tree but not in index).
// repo is NEW and dircache is OLD
void diff_repo_index_untracked(git_diff *out, const strarr_t *repo_files, const git_dircache *dircache);
#endif