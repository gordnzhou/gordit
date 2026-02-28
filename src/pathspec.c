#include <string.h>

#include "pathspec.h"
#include "filesystem.h"
#include "logging.h"
#include "utils.h"

char *normalize_path(const git_repo *repo, const char *filepath) {
    int size = strlen(filepath);
    char *norm_path = smalloc(size + 1);
    repo_rel_path(repo, filepath, norm_path);

    for (int i = 0; i < size; i++) {
        if (norm_path[i] == '\\') {
            norm_path[i] = '/';
        }
    }

    return norm_path;
}

pathspec_result *init_pathspec_result() {
    pathspec_result *result = smalloc(sizeof(*result));
    result->size = 0;
    result->capacity = 8;
    result->norm_paths = smalloc(result->capacity * sizeof(char *));
    return result;
}

void free_pathspec_result(pathspec_result *result) {
    for (int i = 0; i < result->size; i++) {
        free(result->norm_paths[i]);
    }
    free(result);
}

void add_pathspec_result(pathspec_result *result, char *norm_path) {
    if (result->size >= result->capacity) {
        result->capacity *= 2;
        result->norm_paths = srealloc(result->norm_paths, result->capacity * sizeof(char *));
    } 
    result->norm_paths[result->size++] = norm_path;
}

void expand_arg(pathspec_result *result, const git_repo *repo, const char *arg) {
    if (fs_file_exists(arg)) {
        char abs_path[PATH_MAX];
        if (fs_path_abs(arg, abs_path) == 0) {
            if (is_path_in_repo(repo, abs_path)) {
                add_pathspec_result(result, normalize_path(repo, abs_path));
                return;
            }

            fatal("'%s' is outside of current repository (%s)", repo->root_path);
        }

        if (strlen(arg) + 1 > PATH_MAX) {
            fatal("path '%s' is too long (> %d)", arg, PATH_MAX);
        } 
        fatal("unable to get absolute path of '%s'", arg);
    } 

    fatal("unable to parse '%s' as path", arg);
}

char *clean_str(const char *str) {
    int len = strlen(str);
    char *out = smalloc(len + 1);
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] != ' ' && str[i] != '\0' && str[i] != '\n') {
            out[j++] = str[i];
        }
    }
    out[j] = '\0';
    return out;
}

// TODO: expand spec syntax beyond just filename
// regex: ".", "*.txt", "src/*.html", 
// path base: "src/"
int path_matches_pathspec(const char *filepath, const char *spec) {
    assert(strlen(spec) < PATH_MAX);

    char *spec_clean = clean_str(spec);
    char *filepath_clean = clean_str(filepath);
    int res = strstr(filepath_clean, spec_clean) != NULL;

    free(spec_clean);
    free(filepath_clean);
    return res;
}

int check_ignores(const char *folder, const char *fullpath, const git_repo *repo) {
    DIR *dir;
    if ((dir = fs_opendir(folder)) == NULL) {
        fatal("could not check ignores as could not open directory %s", folder);
    }

    fs_dirent *ent;
    char line[PATH_MAX];
    while ((ent = fs_readdir(dir, folder)) != NULL) {
        if (strcmp(ent->de_name, GIT_IGNORE_NAME) == 0) {
            FILE *fptr = fopen(ent->de_path, "r");
            if (fptr == NULL) {
                continue;
            }

            while (fgets(line, PATH_MAX, fptr) != NULL) {
                if (path_matches_pathspec(fullpath, line)) {
                    fclose(fptr);
                    return 1;
                }
            }

            fclose(fptr);
        }
    }

    fs_closedir(dir);
    if (strcmp(repo->root_path, folder) == 0) {
        return 0;
    }

    char parent[PATH_MAX];
    fs_path_dirname(folder, parent);
    return check_ignores(parent, fullpath, repo);
}

int is_file_ignored(const git_repo *repo, const char *path) {
    char full_path[PATH_MAX], imm_parent[PATH_MAX];
    fs_path_join(repo->root_path, path, full_path);
    fs_path_dirname(full_path, imm_parent);
    return check_ignores(imm_parent, full_path, repo);
}

void filter_ignores(pathspec_result *result, const git_repo *repo) {
    int orig_size = result->size;
    for (int i = 0; i < orig_size; i++) {
        if (!is_file_ignored(repo, result->norm_paths[i])) {
            add_pathspec_result(result, result->norm_paths[i]);
        } else {
            free(result->norm_paths[i]);
        }
    }

    int new_size = result->size - orig_size;
    memmove(result->norm_paths, result->norm_paths + orig_size, sizeof(char *) * new_size);
    result->size = new_size;
}

void folder_add_files(pathspec_result *result, const git_repo *repo, const char *folder, int tracked_only, char **ignore_arr) {
    DIR *dir;
    if ((dir = fs_opendir(folder)) == NULL) {
        fatal("could not open directory %s", folder);
    }

    char **ignore_arr_cur = NULL;
    if (tracked_only) {
        char ignore_list_path[PATH_MAX];
        fs_path_join(folder, GIT_IGNORE_NAME, ignore_list_path);

        if (fs_file_exists(ignore_list_path)) {
            FILE *fptr = sfopen(ignore_list_path, "r");
            char line[PATH_MAX];
            while (fgets(line, PATH_MAX, fptr) != NULL) {
                DA_PUSH(ignore_arr_cur, sstrdup(line));
            }
            fclose(fptr);
        }
    }

    fs_dirent *ent;
    while ((ent = fs_readdir(dir, folder)) != NULL) {
        if (strcmp(ent->de_name, ".") == 0 || 
            strcmp(ent->de_name, "..") == 0 ||
            strcmp(ent->de_name, ".git") == 0 || 
            strcmp(ent->de_name, GIT_FOLDER) == 0) {
            continue;
        }

        char *norm_path = normalize_path(repo, ent->de_path);
        int ignore = 0;
        if (tracked_only) {
            for (size_t i = 0; i < DA_LEN(ignore_arr_cur); i++) {
                if (path_matches_pathspec(norm_path, ignore_arr_cur[i])) {
                    ignore = 1;
                    break;
                } 
            }
            for (size_t i = 0; i < DA_LEN(ignore_arr); i++) {
                if (path_matches_pathspec(norm_path, ignore_arr[i])) {
                    ignore = 1;
                    break;
                } 
            }
  
        }

        if (ignore) {
            continue;
        }

        if (ent->de_type == FS_ISFILE) {
            add_pathspec_result(result, norm_path);
        } else if (ent->de_type == FS_ISDIR) {
            char *subfolder = sstrdup(ent->de_path);
            folder_add_files(result, repo, subfolder, tracked_only, ignore_arr_cur);
            free(subfolder);
            free(norm_path);
        }
    }

    fs_closedir(dir);
    DA_FREE_DEEP(ignore_arr_cur, free);
}

pathspec_result *repo_all_files(const git_repo *repo, int tracked_only) {
    pathspec_result *result = init_pathspec_result();
    folder_add_files(result, repo, repo->root_path, tracked_only, NULL);
    return result;
}