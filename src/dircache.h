#ifndef GIT_CACHE_H
#define GIT_CACHE_H

#include "filesystem.h"
#include "repo.h"
#include "tree.h"
#include "fileinfo.h"

/*
Credits to git index format specification:
https://github.com/git/git/blob/master/Documentation/gitformat-index.adoc
*/

typedef struct {
    fs_statinfo info; 
    obj_hash hash; 
    int unix_perm;
    int stage_num;
    int git_mode;
    int namelen; 
    char name[PATH_MAX]; // path relative to repo root
} git_index_entry;

typedef struct {
    int num_entries;
    int capacity;
    git_index_entry **entries; // sorted by name in memcmp() order, entries with same name are sorted by stage_num
} git_dircache;

void free_dircache(git_dircache *);

void print_dircache(git_dircache *);

// parse contents of repo's index into struct
git_dircache *create_dircache(const git_repo *);

// adds file to dircache.
// @param blob if file is new, creates blob from file and sets it to this
// @return 0 on success, -1 if unable to create blob
int add_file_to_dc(git_dircache *, const fileinfo *, git_obj **blob);

// removes file's entries from repo's index.
// @param filepath ASSUME it is a unix path relative to repo root
// @return 0 if removed, -1 if not in index
int remove_file_from_dc(git_dircache *, const fileinfo *);

void write_index(const git_repo *, git_dircache *);

git_obj_tree *build_tree_from_index(git_dircache *);

int dircache_has_conflicts(git_dircache *);

git_index_entry **dircache_find_file(const git_dircache *dircache, const char *name);

int is_stat_same(const fs_statinfo *s1, const fs_statinfo *s2);

#endif