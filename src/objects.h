#ifndef GIT_OBJECTS_H
#define GIT_OBJECTS_H

#include <time.h>

#include "repo.h"
#include "fileinfo.h"

#define OBJ_TYPE_LIST(X) \
    X(BLOB, "blob")      \
    X(TREE, "tree")      \
    X(COMMIT, "commit")  \
    X(TAG, "tag")

enum obj_type {
#define X(name, str) OBJ_TYPE_##name,
    OBJ_TYPE_LIST(X)
#undef X
    OBJ_TYPE_COUNT
};

typedef struct git_obj {
    obj_hash hash;
    enum obj_type type;
    size_t size; 
    unsigned char *data; 
} git_obj;

static const char *obj_type_list[OBJ_TYPE_COUNT] = {
#define X(name, str) [OBJ_TYPE_##name] = str,
    OBJ_TYPE_LIST(X)
#undef X
};

static inline const char *obj_type_string(enum obj_type type)
{
    if (type >= 0 && type < OBJ_TYPE_COUNT)
        return obj_type_list[type];
    return "";
}

void hash_from_bytes(const unsigned char *bytes, obj_hash *out_hash);

void hash_to_bytes(const obj_hash hash, unsigned char *out_bytes);

void copy_hash(obj_hash *out, const obj_hash* in);

void string_to_hash(obj_hash *out, const char *in);

void free_obj(git_obj *obj);

// allocates a null-terminated string version of data 
// without the header
char *obj_content_string(const git_obj *obj);

void create_git_obj(const unsigned char *file_contents, size_t size, enum obj_type type, git_obj *obj);

// @return 1 if object is already stored, 0 otherwise
int write_obj_to_disk(const git_repo *repo, const git_obj *obj);

int delete_obj_from_disk(obj_hash hash);

void create_obj_from_disk(git_obj *obj, const git_repo *repo, const obj_hash hash, enum obj_type type);

// Transform blob object to its original file and saves it to `filepath`.
// Assumes directory already exists
// @return 0 if successful, -1 if folder doesnt exist or other errors.
int create_file_from_blob(const char *filepath, const git_obj*);

// Inits blob struct representing `filepath`.
// @return pointer to blob or NULL on failure
git_obj *create_blob_from_file(const fileinfo *);
#endif