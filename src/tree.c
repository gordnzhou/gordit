#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
    
#include "filesystem.h"
#include "tree.h"
#include "utils.h"
#include "logging.h"
#include "repo.h"

void print_tree_recur(git_obj_tree *tree, const char *prefix) {
    #define INDENT "----"
    
    for (int i = 0; i < tree->size; i++) {
        git_tree_entry *entry = tree->entries[i];
        printf("%s%s: %s ", prefix, obj_type_string(entry->type), fs_path_pbasename(entry->name));

        if (entry->type == OBJ_TYPE_BLOB) {
            printf("(%s)\n", entry->u.blob_hash);
        } else if (entry->type == OBJ_TYPE_TREE) {
            printf("(%s)\n", entry->u.tree->obj.hash);
            int len = strlen(prefix) + strlen(INDENT) + 1;
            char *new_prefix = smalloc(len);
            snprintf(new_prefix, len, "%s%s", prefix, INDENT);
            print_tree_recur(entry->u.tree, new_prefix);
            free(new_prefix);
        }
    }
}

void print_tree(git_obj_tree *tree) {
    printf("TREE: %s\n", tree->obj.hash);
    print_tree_recur(tree, "");
}


int cmp_tree_entries(const void *p1, const void *p2) {
    const git_tree_entry *te1 = *(const git_tree_entry * const *)p1;
    const git_tree_entry *te2 = *(const git_tree_entry * const *)p2;
    return strcmp(te1->name, te2->name);
}

// 6 (mode) + 4 (type) + 40 (hash) + PATH_MAX (name) + 4 (seperators)
#define MAX_TREE_ENTRY_LINE 54 + PATH_MAX

void hash_tree_full(git_obj_tree *tree) {
    unsigned char *buf = smalloc(tree->size * MAX_TREE_ENTRY_LINE);
    size_t content_size = 0;

    for (int i = 0; i < tree->size; i++) {
        git_tree_entry *entry = tree->entries[i];

        obj_hash hash;
        const char *type;
        if (entry->type == OBJ_TYPE_TREE) {
            if (entry->u.tree->obj.data == NULL) {
                hash_tree_full(entry->u.tree);
            }

            copy_hash(&hash, &(entry->u.tree->obj.hash));
            type = obj_type_string(entry->u.tree->obj.type);
        } else {
            copy_hash(&hash, &(entry->u.blob_hash));
            type = obj_type_string(OBJ_TYPE_BLOB);
        }

        size_t line_size = snprintf((char *)buf + content_size, MAX_TREE_ENTRY_LINE, 
            "%06o %s %s %s\n", entry->git_mode, type, hash, fs_path_pbasename(entry->name));
        assert(line_size <= MAX_TREE_ENTRY_LINE);

        content_size += line_size;
    }

    create_git_obj(buf, content_size, OBJ_TYPE_TREE, &(tree->obj));
    free(buf);
}

void free_tree(git_obj_tree *tree) {
    for (int i = 0; i < tree->size; i++) {
        git_tree_entry *entry = tree->entries[i];
        if (entry->type == OBJ_TYPE_TREE) {
            free_tree(entry->u.tree);
        }
        free(entry);
    }
    free(tree->entries);
    free(tree->obj.data);
    free(tree);
}

git_obj_tree *init_tree() {
    git_obj_tree *tree = scalloc(1, sizeof(*tree));
    tree->capacity = 1;
    tree->entries = scalloc(1, sizeof(git_tree_entry *));
    tree->obj.type = OBJ_TYPE_TREE;
    return tree;
}

void add_tree_entry(git_tree_entry *entry, git_obj_tree *tree) {
    if (tree->size >= tree->capacity) {
        tree->capacity *= 2;
        tree->entries = srealloc(tree->entries, tree->capacity * sizeof(git_tree_entry *));
    }

    tree->entries[tree->size++] = entry;
}

int write_tree_to_disk(const git_repo *repo, const git_obj_tree *tree, int check_blobs) {
    int exists = 1;
    for (int i = 0; i < tree->size; i++) {
        git_tree_entry *entry = tree->entries[i];
        if (entry->type == OBJ_TYPE_TREE) {
            exists &= write_tree_to_disk(repo, entry->u.tree, check_blobs);
        } else if (entry->type == OBJ_TYPE_BLOB) {
            if (!check_blobs) {
                continue;
            }
            char path[PATH_MAX];
            obj_store_path(repo, entry->u.blob_hash, path);
            if (!fs_file_exists(path)) {
                DEBUG_PRINT("Writing tree %s but it contains a blob that has not already been written: %s", 
                    tree->obj.hash, entry->u.blob_hash);
            }
        }
    }
    exists &= write_obj_to_disk(repo, &(tree->obj));
    return exists;
}

git_obj_tree *deserialize_tree_recur(const git_repo *repo, obj_hash hash, const char *path) {
    git_obj_tree *tree = init_tree();
  
    create_obj_from_disk(&(tree->obj), repo, hash, OBJ_TYPE_TREE);
    
    char *entries_copy = obj_content_string(&(tree->obj));

    char *saveptr_lines = NULL;
    char *line = strtok_r(entries_copy, "\n", &saveptr_lines);
    while (line != NULL) {
        char *saveptr_fields = NULL;

        git_tree_entry *entry = smalloc(sizeof(*entry));
        entry->git_mode = atoi(strtok_r(line, " ", &saveptr_fields));
        char *type = strtok_r(NULL, " ", &saveptr_fields);
        char *hash = strtok_r(NULL, " ", &saveptr_fields);
        char *name = strtok_r(NULL, " ", &saveptr_fields);
        
        if (strcmp(type, obj_type_string(OBJ_TYPE_BLOB)) == 0) {
            entry->type = OBJ_TYPE_BLOB;
            snprintf(entry->name, PATH_MAX, "%s%s", path, name);

            string_to_hash(&(entry->u.blob_hash), hash);
        } else if (strcmp(type, obj_type_string(OBJ_TYPE_TREE)) == 0) {
            entry->type = OBJ_TYPE_TREE;
            snprintf(entry->name, PATH_MAX, "%s", name);
            
            char subpath[PATH_MAX];
            size_t pathsize = snprintf(subpath, PATH_MAX, "%s%s/", path, entry->name);
            if (pathsize > PATH_MAX) {
                fatal("could not read tree '%s': '%s''s name is too long", tree->obj.hash, hash);
            }
            entry->u.tree = deserialize_tree_recur(repo, hash, subpath);
        } else {
            fatal("tree %s is corrupted: could not get object type of entries", tree->obj.hash);
        }

        add_tree_entry(entry, tree);

        line = strtok_r(NULL, "\n", &saveptr_lines);
    }

    free(entries_copy);
    return tree;
}

git_obj_tree *read_tree_from_disk(const git_repo *repo, obj_hash hash) {
    return deserialize_tree_recur(repo, hash, "");
}

int tree_num_blobs(const git_obj_tree *root) {
    int ret = 0;
    for (int i = 0; i < root->size; i++) {
        git_tree_entry *entry = root->entries[i];
        if (entry->type == OBJ_TYPE_BLOB) {
            ret++;
        } else if (entry->type == OBJ_TYPE_TREE) {
            ret += tree_num_blobs(entry->u.tree);
        }
    }
    return ret;
}

void flatten_tree_recur(
    git_tree_entry **out_blob_list, 
    size_t max_size, size_t *idx, 
    const git_obj_tree *tree) 
{
    for (int i = 0; i < tree->size && *idx < max_size; i++) {
        git_tree_entry *entry = tree->entries[i];
        if (entry->type == OBJ_TYPE_BLOB) {
            out_blob_list[(*idx)++] = entry;
        } else if (entry->type == OBJ_TYPE_TREE) {
            flatten_tree_recur(out_blob_list, max_size, idx, entry->u.tree);
        }
    }
}

void tree_blobs_flat(git_tree_entry **out_blob_list, size_t out_size, const git_obj_tree *root) {
    size_t idx = 0;
    flatten_tree_recur(out_blob_list, out_size, &idx, root);
    assert(idx <= out_size);
}

git_obj_tree *create_tree_recur(const git_repo *repo, const char *folderpath);

// technically not needed; trees are made from entries in index
git_obj_tree *create_tree_from_path(const git_repo *repo, const char *folderpath) {
    git_obj_tree *tree = create_tree_recur(repo, folderpath);
    hash_tree_full(tree);
    return tree;
}

void create_tree_entries(const git_repo *repo, DIR *dir, const char *folderpath, git_obj_tree *tree) {
    char *path_copy = sstrdup(folderpath);
    fs_dirent *ent;

    while ((ent = fs_readdir(dir, folderpath)) != NULL) {
        if (strcmp(ent->de_name, ".") == 0 || strcmp(ent->de_name, "..") == 0) {
            continue;
        }

        git_tree_entry *tree_ent = smalloc(sizeof(*tree_ent));
        snprintf(tree_ent->name, PATH_MAX, "%s", ent->de_name);
        tree_ent->git_mode = stat_mode_to_git(ent->de_mode);

        if (ent->de_type == FS_ISFILE) {
            fileinfo *finfo;
            if ((finfo = start_fileinfo(repo, ent->de_path, "rb")) == NULL) {
                fatal("could not get file info for %s: %s", ent->de_path, strerror(errno));
            }

            git_obj *blob = create_blob_from_file(finfo);
            if (blob == NULL) {
                fatal("could not create blob from file %s: %s", ent->de_path, strerror(errno));
            }

            end_fileinfo(finfo);
            copy_hash(&(tree_ent->u.blob_hash), &(blob->hash));
            tree_ent->type = OBJ_TYPE_BLOB;
        } else if (ent->de_type == FS_ISDIR) {
            tree_ent->u.tree = create_tree_recur(repo, ent->de_path);
            tree_ent->type = OBJ_TYPE_TREE;
        }

        add_tree_entry(tree_ent, tree);
    }

    free(path_copy);
}

git_obj_tree *create_tree_recur(const git_repo *repo, const char *folderpath) {
    DIR *dir;
    if ((dir = fs_opendir(folderpath)) == NULL) {
        fatal("could not open directory: %s", folderpath);
    }

    git_obj_tree *tree = init_tree();
    create_tree_entries(repo, dir, folderpath, tree);
    fs_closedir(dir);

    qsort(tree->entries, tree->size, sizeof(git_tree_entry *), cmp_tree_entries);

    return tree;
}