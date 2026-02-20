#ifndef DIFF_H
#define DIFF_H

#include "tree.h"
#include "dircache.h"

// @return 1 if index will produce the same tree as the tree parameter
int is_tree_and_dc_same(const git_dircache *index, const git_obj_tree *tree);

#endif