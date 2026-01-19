#ifndef GIT_OBJECTS_H
#define GIT_OBJECTS_H

#include <time.h>

#define HASH_SIZE 40

#define O_TYPE_BLOB "blob"
#define O_TYPE_TREE "tree"
#define O_TYPE_COMMIT "commit"
#define O_TYPE_TAG "tag"

typedef char obj_hash[HASH_SIZE + 1];

typedef struct git_obj_blob {
    obj_hash hash;
    char *type;
    unsigned int mode;
    char *name;
    size_t size; // size of data, NOT including header
    void *data; 
} git_obj_blob;

// Inits blob struct representing `filepath`.
// @return pointer to blob or NULL on failure
struct git_obj_blob *create_blob(const char *filepath);

// Creates blob file in objects folder, if it does not already exist.
// @return 0 if successful, -1 otherwise.
int write_blob_to_disk(const struct git_obj_blob *);

// Creates blob struct from blob file. 
// @return pointer to blob or NULL if could not read file.
struct git_obj_blob *read_blob_from_disk(obj_hash hash);

// Transform blob object to its original file and saves it as a file.
// @return 0 if successful, -1 otherwise.
int create_file_from_blob(const char *path, const struct git_obj_blob *);

void free_blob(struct git_obj_blob *);

typedef struct git_obj_tree {
    obj_hash hash;
    char *type;
    unsigned int mode;
    char *name;
    int num_entries;
    struct git_obj_tree_entry **entries; // sorted alphabetically by name
} git_obj_tree;

typedef struct git_obj_tree_entry {
    struct git_obj_tree *tree;
    struct git_obj_blob *blob;
    char *type;
} git_obj_tree_entry;

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
int tree_find(const struct git_obj_tree *, obj_hash hash, struct git_obj_tree_entry *obj, char *path);

// Get flattened representation of tree with regex filter on blob name. `size` points to the list size.
// @return list of blobs in tree of length `*size`
struct git_obj_blob **tree_get_blobs(struct git_obj_tree *, char *regex_filter, int *size);

void free_tree(struct git_obj_tree *);

// maybe implement tree walk for tree diffing
// tree diffing = walk through two flattened trees and produce list of blobs that differ

typedef struct git_obj_commit {
    obj_hash hash;
    char *type;
    struct git_obj_tree *tree;
    time_t timestamp;
    int p_count; // number of parent commits
    struct git_obj_commit **parents;
    char *author;
    char *commiter;
    char *msg;
} git_obj_commit;


typedef struct git_obj {
    obj_hash hash;
    char *type;
} git_obj;

// Deletes object in objects folder. 
// @warning Only delete an object if nothing else (trees, commits, refs, HEAD) points to it!
// @return 0 on success, -1 otherwise.
int delete_obj_from_disk(obj_hash hash);

// Reads object's contents as text in objects folder, filling `buf` with its contents.
// @return 0 on success, -1 otherwise
int get_obj_contents(obj_hash hash, char *buf, int buf_size);

#endif