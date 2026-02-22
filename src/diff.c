#include <string.h>

#include "utils.h"
#include "diff.h"

int is_tree_and_dc_same(const git_dircache *index, const git_obj_tree *tree) {
    int tree_size = tree_num_blobs(tree);

    if (tree_size != index->num_entries) {
        return 0;
    }

    git_tree_entry **tree_flat = smalloc(tree_size * sizeof(git_tree_entry *));
    tree_blobs_flat(tree_flat, tree_size, tree);

    int same = 1;
    for (int i = 0; i < tree_size; i++) {
        git_tree_entry *t_entry = tree_flat[i];
        git_index_entry *i_entry = index->entries[i];
        assert(t_entry->type == OBJ_TYPE_BLOB);
        if (strcmp(t_entry->u.blob_hash, i_entry->hash) != 0 || strcmp(t_entry->name, i_entry->name) != 0) {
            same = 0;
            break;
        }
    }

    free(tree_flat);

    return same;
}