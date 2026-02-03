#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "filesystem.h"
#include "objects.h"
#include "repo.h"

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

int git_init_repo(const char *cwd) {
    char head_path[PATH_MAX];
    fs_path_join(cwd, HEAD_PATH, head_path);

    if (fs_file_exists(head_path) == 1) {
        return 1;
    }

    if (fs_mkdir(GIT_FOLDER, 0700) == -1 ||
        fs_mkdir(OBJS_FOLDER, 0700) == -1 ||
        fs_mkdir(REFS_FOLDER, 0700) == -1) {
        return -1;
    }

    FILE *fptr;
    if ((fptr = fs_fopen(HEAD_PATH, "wb")) == NULL) {
        return -1;
    }

    // TODO: make seperate HEAD and refs initialization function
    fprintf(fptr, "refs: refs/heads/main");
    fs_fclose(fptr);
   
    return 0;
}

const git_repo *get_working_repo(const char *cwd) {
    char repo_root[PATH_MAX];
    if (!git_find_root(cwd, repo_root)) {
        return NULL;
    }

    git_repo *repo = malloc(sizeof(*repo));

    char git_folder_path[PATH_MAX];
    fs_path_join(repo_root, GIT_FOLDER, git_folder_path);

    strncpy(repo->root_path, repo_root, PATH_MAX);
    fs_path_join(git_folder_path, REFS_NAME, repo->ref_path);
    fs_path_join(git_folder_path, OBJS_NAME, repo->objects_path);
    fs_path_join(git_folder_path, INDEX_NAME, repo->index_path);
    fs_path_join(git_folder_path, HEAD_NAME, repo->head_path);

    return repo;
}

int obj_store_path(const git_repo *repo, const obj_hash hash, char *out) {
    char path2[OBJ_HASH_SIZE + 1];

    for (size_t i = OBJ_HASH_SIZE; i > 2; --i) {
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
        perror("Could not make directory objects store");
        return -1;
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