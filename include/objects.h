#ifndef GIT_OBJECTS_H
#define GIT_OBJECTS_H

#include <time.h>

#define OBJ_HASH_SIZE 41

#define O_TYPE_BLOB "blob"
#define O_TYPE_TREE "tree"
#define O_TYPE_COMMIT "commit"
#define O_TYPE_TAG "tag"

typedef char obj_hash[OBJ_HASH_SIZE];

typedef struct git_obj {
    obj_hash hash;
    const char *type;
    size_t size; // size of data, including header
    unsigned char *data; 
} git_obj;

typedef struct git_obj_blob {
    git_obj obj;
} git_obj_blob;

typedef struct git_tree_list_entries {
    struct git_tree_entry **data;
    int size;
    int capacity;
} git_tree_list_entries;

typedef struct git_obj_tree {
    git_obj obj;
    struct git_tree_list_entries entries;
} git_obj_tree;

enum obj_type { BLOB_OBJ, TREE_OBJ };

typedef struct git_tree_entry {
    char name[PATH_MAX];
    unsigned int git_mode;
    enum obj_type type;
    union {
        struct git_obj_tree *tree;
        struct git_obj_blob *blob;
    } u;
} git_tree_entry;


// prints tree and children head recursively
void print_tree(git_obj_tree *);

// Inits blob struct representing `filepath`.
// @return pointer to blob or NULL on failure
struct git_obj_blob *create_blob(size_t filesize, FILE *);

// Creates blob file in objects folder, if it does not already exist.
// @return 0 if successful, -1 otherwise.
int write_blob_to_disk(const struct git_obj_blob *);

// Creates blob struct from blob file. 
// @return pointer to blob or NULL if could not read file.
struct git_obj_blob *read_blob_from_disk(obj_hash hash);

// Transform blob object to its original file and saves it to `filepath`.
// Assumes directory already exists
// @return 0 if successful, -1 if folder doesnt exist or other errors.
int create_file_from_blob(const char *filepath, const struct git_obj_blob *);

void free_blob(struct git_obj_blob *);

// Inits tree struct representing `folderpath`. Recursively creates tree for subfolders and blobs for files.
// @return pointer to tree or NULL if failure
struct git_obj_tree *create_tree(const char *folderpath);

// Creates tree file and files for all of its sub-trees and blobs in objects folder.
// Skips trees and blobs that already exist in objects folder.
// @return 0 if successful, -1 otherwise.
int write_tree_to_disk(const struct git_obj_tree *);

// Creates tree struct from tree file.
// @return pointer to tree or NULL if could not read file.
struct git_obj_tree *read_tree_from_disk(obj_hash hash);

// Walks through tree, looking for object with a matching hash.
// If successful, `path` contains path to object from tree and `obj` contains its struct.
// @return 1 if successful, 0 otherwise. 
int tree_find(const struct git_obj_tree *, obj_hash hash, struct git_tree_entry *obj, char *path);

// Get flattened representation of tree with regex filter on blob name. `size` points to the list size.
// @return list of blobs in tree of length `*size`
struct git_obj_blob **tree_get_blobs(struct git_obj_tree *, char *regex_filter, int *size);

void free_tree(struct git_obj_tree *);

// Checks if two trees are identical
// TODO: return dynamically-sized list of blob names that were:
// 1. added (not in 1st, in 2nd)
// 2. removed (in 1st, not in 2nd)
// 3. change (in both, but different hashes);
// @return 0 if equal, -1 otherwise
int tree_cmp(struct git_obj_tree *, struct git_obj_tree *, const char *);

// maybe implement tree walk for tree diffing
// tree diffing = walk through two flattened trees and produce list of blobs that differ

typedef struct git_obj_commit {
    git_obj obj;
    git_obj_tree *tree;
    time_t timestamp;
    int p_count; // number of parent commits
    struct git_obj_commit **parents;
    char *author;
    char *commiter;
    char *msg;
} git_obj_commit;

// Deletes object in objects folder. 
// @warning Only delete an object if nothing else (trees, commits, refs, HEAD) points to it!
// @return 0 on success, -1 otherwise.
int delete_obj_from_disk(obj_hash hash);

// Reads object's contents as text in objects folder, filling `buf` with its contents.
// @return 0 on success, -1 otherwise
int get_obj_contents(obj_hash hash, char *buf, int buf_size);

#endif