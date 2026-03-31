#ifndef COMMIT_H
#define COMMIT_H

#include "tree.h"

typedef struct git_obj_commit {
    obj_hash tree_hash;
    time_t timestamp;
    int num_parents; 
    obj_hash *parents;
    char *author_name;
    char *author_email;
    char *msg;
} git_obj_commit;

void commit_free(git_obj_commit *commit);

void commit_print(const git_obj_commit *commit);

git_obj_commit *commit_new(git_obj_tree *tree, 
    int num_parents, 
    obj_hash *parents, 
    const char *author_name, 
    const char *author_email, 
    const char *msg);

// serializes commit struct to object text
git_obj *create_commit_obj(const git_obj_commit *commit);

// deserialize commit struct from object text
git_obj_commit *commit_read(const git_repo *repo, const obj_hash hash);

// walks up parents of commit and searches for a commit with matching hash
// @return bool for if commit_hash is a parent of commit
int commit_find_parent(const git_repo *repo, const obj_hash root_commit, const obj_hash target_commit);

#endif