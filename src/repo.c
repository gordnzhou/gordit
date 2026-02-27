#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "filesystem.h"
#include "objects.h"
#include "repo.h"
#include "utils.h"
#include "logging.h"
#include "refs.h"

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int stat_mode_to_git(unsigned int st_mode) {
    if (S_ISDIR(st_mode)) {
        return GIT_MODE_DIR;
    } 
    if (st_mode & S_IXUSR) {
        return GIT_MODE_FILE_X;
    } 
    return GIT_MODE_FILE_R;
}

unsigned int git_mode_to_stat(unsigned int git_mode) {
    return git_mode & 0x1FF;
}


// Walks up `path` until it finds a directory with git folder. Fills `repo_root` with that directory path. 
// @returns 1 on successful find and 0 if unsuccessful.
int git_find_root(const char *path, char *repo_root) {
    DIR *dir;
    if ((dir = fs_opendir(path)) == NULL) {
        return 0;
    }

    fs_dirent *ent;
    while ((ent = fs_readdir(dir, path)) != NULL) {
        if (strcmp(ent->de_name, GIT_FOLDER) == 0) {
            snprintf(repo_root, PATH_MAX, "%s", path);
            fs_closedir(dir);
            return 1;
        }
    }
    fs_closedir(dir);

    char parent[PATH_MAX];
    fs_path_dirname(path, parent);
    if (strcmp(path, parent) == 0) {
        return 0;
    }

    return git_find_root(parent, repo_root);
}

void init_repo_context(git_repo *repo, const char *repo_root) {
    snprintf(repo->root_path, PATH_MAX, "%s", repo_root);
    fs_path_join(repo_root, GIT_FOLDER, repo->git_folder_path);
    fs_path_join(repo->git_folder_path, REFS_NAME, repo->refs_path);
    fs_path_join(repo->git_folder_path, OBJS_NAME, repo->objects_path);
    fs_path_join(repo->git_folder_path, INDEX_NAME, repo->index_path);
    fs_path_join(repo->git_folder_path, HEAD_NAME, repo->head_path);
}

const git_repo *get_working_repo(const char *cwd) {
    char repo_root[PATH_MAX];
    if (!git_find_root(cwd, repo_root)) {
        fatal("not in a repository");
    }

    git_repo *repo = smalloc(sizeof(*repo));
    init_repo_context(repo, repo_root);
    return repo;
}

int create_repo_folder(const char *cwd) {
    git_repo *repo = smalloc(sizeof *repo);
    init_repo_context(repo, cwd);

    int exists = fs_file_exists(repo->head_path);

    char local_refs_folder[PATH_MAX], tag_refs_folder[PATH_MAX];
    fs_path_join(repo->root_path, LOCAL_REFS_FOLDER, local_refs_folder);
    fs_path_join(repo->root_path, TAG_REFS_FOLDER, tag_refs_folder);

    mode_t mode = 0700;
    if (fs_mkdir(repo->git_folder_path, mode) == -1 ||
        fs_mkdir(repo->objects_path, mode) == -1 ||
        fs_mkdir(repo->refs_path, mode) == -1 ||
        fs_mkdir(local_refs_folder, mode) == -1 ||
        fs_mkdir(tag_refs_folder, mode) == -1) {
        fatal("could not initialize repository: %s", strerror(errno));
    }

    if (!exists) {
        move_head(repo, START_BRANCH);
    }

    free(repo);
   
    return exists;
}


int obj_store_path(const git_repo *repo, const obj_hash hash, char *out) {
    char path2[sizeof(obj_hash) + 1];

    for (size_t i = sizeof(obj_hash); i > 2; --i) {
        path2[i] = hash[i - 1];
    }
    path2[0] = hash[0];
    path2[1] = hash[1];
    path2[2] = '/';
    fs_path_join(repo->objects_path, path2, out);
    
    if (fs_file_exists(out)) {
        return 1;
    }
    
    char parent_path[PATH_MAX];
    fs_path_dirname(out, parent_path);
    if (fs_mkdir(parent_path, 0700) == -1) {
        fatal("could not create %s: %s", parent_path, strerror(errno));
    }

    return 0;
}

int is_path_in_repo(const git_repo *repo, const char *abs_path) {
    int root_len = strlen(repo->root_path);
    int path_len = strlen(abs_path);
    int rel_len = path_len - root_len;

    return rel_len > 0 && 
        memcmp(abs_path, repo->root_path, root_len) == 0 &&
        strstr(abs_path, GIT_FOLDER) == NULL;
}

void repo_rel_path(const git_repo *repo, const char *abs_path, char *out) {
    int root_len = strlen(repo->root_path);
    int path_len = strlen(abs_path);
    int rel_len = path_len - root_len;

    assert(rel_len > 0 && memcmp(repo->root_path, abs_path, root_len) == 0);

    snprintf(out, PATH_MAX, "%s", abs_path + root_len + 1);
     
    char *ptr = out;
    while (*ptr != '\0') {
        if (*ptr == '\\') {
            *ptr = '/';
        }
        ptr++;
    }
}