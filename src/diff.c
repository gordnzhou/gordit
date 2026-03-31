#include <string.h>

#include "utils.h"
#include "diff.h"
#include "pathspec.h"
#include "logging.h"

typedef struct {
    const char *name;
    obj_hash *hash;
} git_entry;

typedef struct {
    int size;
    git_entry *entries;
} git_entry_list;

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
        if (strcmp(t_entry->name, i_entry->name) != 0 || !hash_eq(t_entry->u.blob_hash, i_entry->hash)) {
            same = 0;
            break;
        }
    }

    free(tree_flat);

    return same;
}

git_entry_list *entry_list_from_tree(const git_obj_tree *tree, int tree_size, const git_file_list *filter) {  
    git_entry_list *list = smalloc(sizeof(*list));
    list->entries = NULL;
    list->size = tree_size;

    if (tree && tree_size) {
        git_tree_entry **flat = smalloc(tree_size*sizeof(git_tree_entry *));
        tree_blobs_flat(flat, tree_size, tree);
        list->entries = smalloc(list->size*sizeof(git_entry));
        for (int i = 0; i < tree_size; i++) {
            git_tree_entry *entry = flat[i];

            if (filter != NULL && filter->size && !git_fl_find(filter, entry->name)) {
                continue;
            }

            list->entries[i] = (git_entry){ .hash = &(entry->u.blob_hash), .name = entry->name };
        }

        free(flat);
    }

    return list;
}

git_entry_list *entry_list_from_dircache(const git_dircache *dircache, const git_file_list *filter) {
    git_entry_list *list = smalloc(sizeof(*list));
    list->size = dircache->num_entries;
    list->entries = list->size > 0 ? smalloc(list->size*sizeof(git_entry)) : NULL;
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];

        if (filter != NULL && filter->size && !git_fl_find(filter, entry->name)) {
            continue;
        }

        list->entries[i] = (git_entry){ .hash = &(entry->hash), .name = entry->name };
    }
    return list;
}

void free_entry_list(git_entry_list *list) {
    free(list->entries);
    free(list);
}

void diff_add(git_diff *out, enum diff_type type, const char *name, 
    const obj_hash *old, 
    const obj_hash *new
) {
    out->entries[out->size++] = (diff_entry){ 
        .type = type, 
        .name = name,
        .old_hash = old,
        .new_hash = new };
}

void entry_list_cmp(git_diff *out, const git_entry_list *new, const git_entry_list *old) {
    int old_i = 0;
    int new_i = 0;
    while (old_i < old->size || new_i < new->size) {
        git_entry *old_entry = NULL;
        git_entry *new_entry = NULL;

        int cmp = 0;
        if (old_i == old->size) {
            new_entry = &(new->entries[new_i]);
            cmp = -1;
        } else if (new_i == new->size) {
            old_entry = &(old->entries[old_i]);
            cmp = 1;
        } else {
            old_entry = &(old->entries[old_i]);
            new_entry = &(new->entries[new_i]);
            cmp = strcmp(new_entry->name, old_entry->name);
        }

        if (cmp == 0) {
            if (!hash_eq(*(old_entry->hash), *(new_entry->hash))) {
                diff_add(out, MODIFIED, new_entry->name, old_entry->hash, new_entry->hash);
            }
            old_i++;
            new_i++;
        } else if (cmp < 0) {
            diff_add(out, ADDED, new_entry->name, NULL, new_entry->hash);
            new_i++;
        } else {
            diff_add(out, REMOVED, old_entry->name, old_entry->hash, NULL);
            old_i++;
        }
    }
}

git_diff *diff_new(size_t max_size) {
    git_diff *ret = smalloc(sizeof(*ret));
    ret->size = 0;
    ret->entries = smalloc(max_size*sizeof(diff_entry));
    return ret;
}

void diff_free(git_diff *diff) {
    free(diff->entries);
    free(diff);
}

void diff_trees(git_diff *out, const git_obj_tree *new, int new_size, const git_obj_tree *old, int old_size) {
    git_entry_list *new_list = entry_list_from_tree(new, new_size, NULL);
    git_entry_list *old_list = entry_list_from_tree(old, old_size, NULL);

    entry_list_cmp(out, new_list, old_list);

    free_entry_list(new_list);
    free_entry_list(old_list);
}

void diff_index_tree(git_diff *out, const git_dircache *dircache, const git_obj_tree *tree, int tree_size, const git_file_list *filter) {
    git_entry_list *new_list = entry_list_from_dircache(dircache, filter);
    git_entry_list *old_list = entry_list_from_tree(tree, tree_size, filter);

    entry_list_cmp(out, new_list, old_list);

    free_entry_list(new_list);
    free_entry_list(old_list);
}

void diff_repo_index(git_diff *out, const git_repo *repo, 
    const strarr_t *repo_files, 
    const git_dircache *dircache, 
    const git_file_list *filter
) {
    diff_repo_index_tracked(out, repo, dircache, filter);
    diff_repo_index_untracked(out, repo_files, dircache, filter);
}

int is_file_changed_unstaged(const git_index_entry *entry, const fileinfo *info) {
    if (!is_stat_same(&(info->stat), &(entry->info))) {
        git_obj *blob = create_blob_from_file(info);
        if (!blob) {
            fatal("could not get blob of '%s'", info->norm_path);
        }

        if (!hash_eq(blob->hash, entry->hash)) {
            return 1;
        }

        free_obj(blob);
    }

    return 0;
}

int diff_file_repo_index(const git_repo *repo, const git_index_entry *entry) {
    char path[PATH_MAX];
    fs_path_join(repo->root_path, entry->name, path);

    if (!fs_file_exists(path)) {
        return -1;
    }

    fileinfo *info = start_fileinfo(repo, entry->name, "rb");
    if (info == NULL) {
        fatal("could not get stat info of '%s'", entry->name);
    }
    int res = is_file_changed_unstaged(entry, info);
    end_fileinfo(info);
    return res;
}

void diff_repo_index_tracked(git_diff *out, const git_repo *repo, const git_dircache *dircache, const git_file_list *filter) {
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];

        if (filter != NULL && filter->size && !git_fl_find(filter, entry->name)) {
            continue;
        }

        int changed = diff_file_repo_index(repo, entry);

        if (changed == 1) {
            diff_add(out, MODIFIED, entry->name, &(entry->hash), NULL);
        } else if (changed == -1) {
            diff_add(out, REMOVED, entry->name, &(entry->hash), NULL);
        }
    }
}

void diff_repo_index_untracked(git_diff *out, const strarr_t *repo_files, const git_dircache *dircache, const git_file_list *filter) {
    for (size_t i = 0; i < repo_files->len; i++) {
        char *name = repo_files->data[i];

        if (filter != NULL && filter->size && !git_fl_find(filter, name)) {
            continue;
        }

        if (dircache_find_file(dircache, name) == NULL) {
            diff_add(out, ADDED, name, NULL, NULL);
        }
    }
}