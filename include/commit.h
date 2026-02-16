#ifndef COMMIT_H
#define COMMIT_H

#include "objects.h"

typedef struct git_obj_commit {
    git_obj_tree *tree;
    time_t timestamp;
    int num_parents; 
    obj_hash *parents;
    char *author_name;
    char *author_email;
    char *msg;
} git_obj_commit;

git_obj_commit *create_commit(git_obj_tree *tree, 
    int num_parents, 
    obj_hash *parents, 
    char *author_name, char *author_email, 
    char *msg);

git_obj *create_commit_obj(const git_obj_commit *commit);

#endif