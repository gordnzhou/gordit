#include <string.h>

#include "utils.h"
#include "diff.h"
#include "pathspec.h"
#include "logging.h"

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

void diff_index_tree(git_diff *out, const git_dircache *dircache, const git_obj_tree *tree, int tree_size) {
    git_tree_entry **tree_flat = NULL;

    if (tree != NULL) {
        tree_flat = smalloc(tree_size * sizeof(git_tree_entry *));
        tree_blobs_flat(tree_flat, tree_size, tree);
    }

    int tree_i = 0;
    int index_i = 0;
    while (tree_i < tree_size || index_i < dircache->num_entries) {
        git_tree_entry *tree_entry = NULL;
        git_index_entry *index_entry = NULL;

        int cmp = 0;
        if (tree_i == tree_size) {
            index_entry = dircache->entries[index_i];
            cmp = -1;
        } else if (index_i == dircache->num_entries) {
            tree_entry = tree_flat[tree_i];
            cmp = 1;
        } else {
            tree_entry = tree_flat[tree_i];
            index_entry = dircache->entries[index_i];
            cmp = strcmp(index_entry->name, tree_entry->name);
        }

        if (cmp == 0) {
            if (strcmp(tree_entry->u.blob_hash, index_entry->hash) != 0) {
                out->entries[out->size++] = (diff_entry){ .type = MODIFIED, .name = index_entry->name };
            }
            tree_i++;
            index_i++;
        } else if (cmp < 0) {
            out->entries[out->size++] = (diff_entry){ .type = ADDED, .name = index_entry->name };
            index_i++;
        } else {
            out->entries[out->size++] = (diff_entry){ .type = REMOVED, .name = index_entry->name };
            tree_i++;
        }
    }

    if (tree_flat) free(tree_flat);
}

void diff_repo_index(git_diff *out, const git_repo *repo, const strarr_t *repo_files, const git_dircache *dircache) {
    diff_repo_index_tracked(out, repo, dircache);
    diff_repo_index_untracked(out, repo_files, dircache);
}

void diff_repo_index_tracked(git_diff *out, const git_repo *repo, const git_dircache *dircache) {
    char path[PATH_MAX];
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        fs_path_join(repo->root_path, entry->name, path);

        if (!fs_file_exists(path)) {
            out->entries[out->size++] = (diff_entry){ .type = REMOVED, .name = entry->name };
            continue;
        }

        fileinfo *info = start_fileinfo(repo, entry->name, "rb");
        if (info == NULL) {
            fatal("could get stat info of '%s'\n", entry->name);
        }
        
        if (!is_stat_same(&(info->stat), &(entry->info))) {
            git_obj *blob = create_blob_from_file(info);
            if (!blob) {
                continue;
            }

            if (strcmp(blob->hash, entry->hash) != 0) {
                out->entries[out->size++] = (diff_entry){ .type = MODIFIED, .name = entry->name };
            }

            free_obj(blob);
        }
        end_fileinfo(info);
    }
}

void diff_repo_index_untracked(git_diff *out, const strarr_t *repo_files, const git_dircache *dircache) {
    for (size_t i = 0; i < repo_files->len; i++) {
        char *name = repo_files->data[i];
        if (dircache_find_file(dircache, name) == NULL) {
            out->entries[out->size++] = (diff_entry){ .type = ADDED, .name = name };
        }
    }
}