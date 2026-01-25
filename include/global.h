#ifndef GLOBAL_H
#define GLOBAL_H

// global struct storing 
// root path with .gordit as immediate subfolder,
// paths to index, HEAD
// paths to folders refs, objects, (logs)
#include <filesystem.h>
#include <objects.h>

typedef struct {
    char root_path[PATH_MAX];
    char ref_path[PATH_MAX];
    char objects_path[PATH_MAX];
    char index_path[PATH_MAX];
    char head_path[PATH_MAX];
} repo;

// finds cwd's repo and initalizes `REPO`.
// @return 0 if repo was found + `ROOT` was successfully initalized, 
// 1 if no repo found, and -1 if any errors.
int get_working_repo();

// @return 1 if path exists, 0 if path is new, otherwise -1 if error making directory
int hash_to_path(const obj_hash hash, char *out);

// nothing for now..
void free_repo();

#endif