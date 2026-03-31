#ifndef REPO_H
#define REPO_H

#include "filesystem.h"
#include "hash.h"

#define GIT_MODE_DIR 0040000
#define GIT_MODE_FILE_X 0100755
#define GIT_MODE_FILE_R 0100644

#define GIT_IGNORE_NAME ".gorditignore"
#define GLOBAL_CONFIG_NAME ".gorditconfig"

#define GIT_FOLDER  ".gordit"
#define REFS_NAME   "refs"
#define OBJS_NAME   "objects"
#define HEAD_NAME   "HEAD"
#define INDEX_NAME  "index"
#define CONFIG_NAME "config"
#define BACKUP_HEAD_NAME "ORIG_HEAD"
#define MERGE_HEAD_NAME  "MERGE_HEAD"
#define LOCAL_REFS_NAME "heads"
#define REMOTE_REFS_NAME "remotes"
#define TAG_REFS_NAME "tags"

#define GIT_PATH_SEP "/"

#define GIT_FOLDER_PATH GIT_FOLDER
#define INDEX_PATH      GIT_FOLDER GIT_PATH_SEP INDEX_NAME
#define HEAD_PATH       GIT_FOLDER GIT_PATH_SEP HEAD_NAME
#define CONFIG_PATH     GIT_FOLDER GIT_PATH_SEP CONFIG_NAME
#define BAC_HEAD_PATH   GIT_FOLDER GIT_PATH_SEP BACKUP_HEAD_NAME
#define MERGE_HEAD_PATH GIT_FOLDER GIT_PATH_SEP MERGE_HEAD_NAME
#define OBJECTS_PATH    GIT_FOLDER GIT_PATH_SEP OBJS_NAME
#define REFS_PATH       GIT_FOLDER GIT_PATH_SEP REFS_NAME
#define LOC_REFS_PATH   REFS_PATH GIT_PATH_SEP "heads"
#define TAG_REFS_PATH   REFS_PATH GIT_PATH_SEP "tags"
#define REM_REFS_PATH   REFS_PATH GIT_PATH_SEP "remotes"

#define REPO_PATHS                      \
    X(git_path, GIT_FOLDER_PATH)        \
    X(index_path, INDEX_PATH)           \
    X(head_path, HEAD_PATH)             \
    X(objects_path, OBJECTS_PATH)       \
    X(refs_path, REFS_PATH)             \
    X(local_refs_path, LOC_REFS_PATH)   \
    X(remote_refs_path, REM_REFS_PATH)  \
    X(tag_refs_path, TAG_REFS_PATH)     \
    X(backup_head_path, BAC_HEAD_PATH)  \
    X(merge_head_path, MERGE_HEAD_PATH) \
    X(config_path, CONFIG_PATH)

typedef struct {
    char root_path[PATH_MAX];

#define X(name, val) char name[PATH_MAX];
    REPO_PATHS
#undef X

} git_repo;

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int stat_mode_to_git(unsigned int st_mode);

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int git_mode_to_stat(unsigned int git_mode);
 
// gets repo of current working directory
const git_repo *git_repo_init();

// Initialize git folder in root, including subfolders, index and HEAD.
// @param start_branch NULL to use default starting branch name
// @returns 0 on success, 1 if git folder already in root, -1 otherwise. 
int repo_folder_initialize(char *repo_root, char *start_branch, int bare);

// gets path of object in repo's objects folder
// @return 1 if path already exists, 0 otherwise
int obj_store_path(const git_repo *, const obj_hash, char *out);

// used for name in index and trees
// @returns '/' seperated path relative to repo root.
void repo_rel_path(const git_repo *, const char *, char *out);

void repo_full_path(const git_repo *, const char *, char *out);

const char *git_global_config_path();

// cleans slashes
void git_path_clean(char *path);

#endif