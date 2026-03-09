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
    const char *name;
    const obj_hash *new_hash;
    const obj_hash *old_hash;
} diff_entry;

typedef struct {
    int size;
    diff_entry *entries;
} git_diff;

// @return 1 if index will produce the same tree as the tree parameter
int is_tree_and_dc_same(const git_dircache *index, const git_obj_tree *tree);

git_diff *diff_new(size_t max_size);

void diff_free(git_diff *diff);

void diff_trees(git_diff *out, const git_obj_tree *new, int new_size, const git_obj_tree *old, int old_size);

// index is NEW and tree is OLD
void diff_index_tree(git_diff *out, const git_dircache *dircache, const git_obj_tree *tree, int tree_size);

// modified and added entries do not have their new hashes saved
// repo is NEW and dircache is OLD
void diff_repo_index(git_diff *out, const git_repo *repo, const strarr_t *repo_files, const git_dircache *dircache);

// @return 0 if no change, 1 if modified, -1 if deleted
int diff_file_repo_index(const git_repo *repo, const git_index_entry *entry);

int is_file_changed_unstaged(const git_index_entry *entry, const fileinfo *info);

// checks index entries in repo
// repo is NEW and dircache is OLD
void diff_repo_index_tracked(git_diff *out, const git_repo *repo, const git_dircache *dircache);

// checks for new files (files in working tree but not in index).
// repo is NEW and dircache is OLD
void diff_repo_index_untracked(git_diff *out, const strarr_t *repo_files, const git_dircache *dircache);

// TODO: diff repo and a tree
#endif