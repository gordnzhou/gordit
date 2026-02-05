#ifndef REPO_H
#define REPO_H

#include <filesystem.h>

#define OBJ_HASH_SIZE 41
typedef char obj_hash[OBJ_HASH_SIZE];

#define GIT_MODE_DIR 0040000
#define GIT_MODE_FILE_X 0100755
#define GIT_MODE_FILE_R 0100644

#define GIT_FOLDER  ".gordit"
#define REFS_NAME "refs"
#define OBJS_NAME "objects"
#define HEAD_NAME "HEAD"
#define INDEX_NAME "index"

#define REFS_FOLDER GIT_FOLDER "/" REFS_NAME
#define OBJS_FOLDER GIT_FOLDER "/" OBJS_NAME
#define HEAD_PATH GIT_FOLDER "/" HEAD_NAME
#define INDEX_PATH GIT_FOLDER "/" INDEX_NAME

#define GIT_IGNORE_NAME ".gorditignore"

typedef struct {
    char root_path[PATH_MAX];
    char ref_path[PATH_MAX];
    char objects_path[PATH_MAX];
    char index_path[PATH_MAX];
    char head_path[PATH_MAX];
} git_repo;

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int stat_mode_to_git(unsigned int st_mode);

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int git_mode_to_stat(unsigned int git_mode);
 
// gets repo context 
const git_repo *get_working_repo(const char *cwd);

// If git folder exists in cwd do nothing.
// Otherwise create git folder in cwd, including subfolders, index and HEAD.
// @returns 0 on success, 1 if git folder already in cwd, -1 otherwise. 
int git_init_repo(const char *cwd);

// gets path of object in repo's objects folder
// @return 1 if object already in git folder, 0 if new, -1 if could not create dir
int obj_store_path(const git_repo *, const obj_hash, char *out);

// @return 1 if path is inside of repo and not in git folder
int is_path_in_repo(const git_repo *, const char *);

// used for name in index and trees
// @returns '/' seperated path relative to repo root.
void repo_rel_path(const git_repo *, const char *, char *out);

#endif