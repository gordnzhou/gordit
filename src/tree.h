#ifndef TREE_H
#define TREE_H

#include "objects.h"

typedef struct git_tree_entry {
    char name[PATH_MAX];
    unsigned int git_mode;
    enum obj_type type;
    union {
        obj_hash blob_hash;
        struct git_obj_tree *tree;
    } u;
} git_tree_entry;

typedef struct git_obj_tree {
    git_obj obj;
    int size;
    int capacity;
    struct git_tree_entry **entries;
} git_obj_tree;

void free_tree(git_obj_tree *);

// prints tree and children head recursively
void print_tree(git_obj_tree *);

git_obj_tree *read_tree_from_disk(const git_repo *repo, obj_hash hash);

git_obj_tree *init_tree();

void add_tree_entry(git_tree_entry *entry, git_obj_tree *tree);

// calculates object hash for tree struct and its subtree.
void hash_tree_full(git_obj_tree *tree);

// Writes all trees and its children (tree objs ONLY) to disk.
// @param check_blobs set to show warning if tree has a child blob that is not saved
// @return 1 if tree and all children are already stored, 0 otherwise
int write_tree_to_disk(const git_repo *repo, const git_obj_tree *tree, int check_blobs);

int tree_num_blobs(const git_obj_tree *root);

void tree_blobs_flat(git_tree_entry **out_blob_list, size_t out_size, const git_obj_tree *root);

git_tree_entry **tree_find_blob(const git_obj_tree *root, const char *name);

// Inits tree struct representing `folderpath`. Recursively creates tree for subfolders and blobs for files.
// @return pointer to tree or NULL if failure
git_obj_tree *create_tree_from_path(const git_repo *, const char *folderpath);
#endif