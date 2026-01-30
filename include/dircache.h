#ifndef GIT_CACHE_H
#define GIT_CACHE_H

#include "filesystem.h"
#include "repo.h"
#include "objects.h"

/*
Credits to git index format specification:
https://github.com/git/git/blob/master/Documentation/gitformat-index.adoc
*/

typedef struct {
    struct fs_fileinfo *info; // can be NULL
    obj_hash *hash; // can be NULL
    int unix_perm;
    int stage_num;
    int git_mode;
    int namelen; 
    char *name; // path relative to repo root
} git_index_entry;

typedef struct {
    int num_entries;
    git_index_entry **entries; // sorted by name, entries with same name are sorted by stage_num
} git_dircache;

// parse contents of repo's index into struct
git_dircache *create_dircache(const git_repo *);

// adds file to repo's index.
// @param filepath ASSUME it is a unix path relative to repo root
// if file is aready in index, updates it only if stat info has changed
int add_file_to_index(const git_repo *, git_dircache *, char *filepath);

// adds entry to repo's index. entry must be of a blob!
int add_tree_entry_to_index(const git_repo *, git_dircache *, git_tree_entry *);

// removes file's entry from repo's index.
// @param filepath ASSUME it is a unix path relative to repo root
int remove_file_from_index(const git_repo *, git_dircache *, char *filepath);

void free_dircache(git_dircache *);

#endif