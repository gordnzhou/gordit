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

void free_commit(git_obj_commit *commit);

void print_commit(const git_obj_commit *commit);

git_obj_commit *create_commit(git_obj_tree *tree, 
    int num_parents, 
    obj_hash *parents, 
    char *author_name, char *author_email, 
    char *msg);

// serializes commit struct to object text
git_obj *create_commit_obj(const git_obj_commit *commit);

// deserialize commit struct from object text
git_obj_commit *create_commit_from_disk(const git_repo *repo, obj_hash hash);
#endif