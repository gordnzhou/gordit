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
            if (!is_path_in_folder(repo->root_path, abs_path)) {
                fatal("'%s' is outside of current repository (%s)", abs_path, repo->root_path);
            }
            if (is_path_in_folder(repo->git_path, abs_path)) {
                fatal("'%s' is inside repo's internal git folder", abs_path);
            }

            add_pathspec_result(result, normalize_path(repo, abs_path));
            return;
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
    char ignore_path[PATH_MAX];
    fs_path_join(folder, GIT_IGNORE_NAME, ignore_path);
    if (fs_file_exists(ignore_path)) {
        FILE *fptr = sfopen(ignore_path, "r");
        char line[PATH_MAX];
        while (fgets(line, PATH_MAX, fptr) != NULL) {
            if (path_matches_pathspec(fullpath, line)) {
                fclose(fptr);
                return 1;
            }
        }
        fclose(fptr);
    }

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

void folder_add_files(strarr_t *result, const git_repo *repo, const char *folder, int tracked_only, strarr_t *ignore_arr) {
    DIR *dir = sopendir(folder);

    strarr_t *ignore_arr_cur = strarr_new();
    if (tracked_only) {
        char ignore_list_path[PATH_MAX];
        fs_path_join(folder, GIT_IGNORE_NAME, ignore_list_path);

        if (fs_file_exists(ignore_list_path)) {
            FILE *fptr = sfopen(ignore_list_path, "r");
            char line[PATH_MAX];
            while (fgets(line, PATH_MAX, fptr) != NULL) {
                strarr_push(ignore_arr_cur, line);
            }
            fclose(fptr);
        }
    }

    fs_dirent ent = { 0 };
    int ret = 0;
    while ((ret = fs_readdir(dir, &ent, folder)) != 0) {
        if (ret == -1) {
            continue;
        }

        if (strcmp(ent.de_name, ".git") == 0 || strcmp(ent.de_name, GIT_FOLDER) == 0) {
            continue;
        }

        char *norm_path = normalize_path(repo, ent.de_path);
        int ignore = 0;
        if (tracked_only) {
            for (size_t i = 0; i < ignore_arr->len + ignore_arr_cur->len; i++) {
                const char *spec = (i < ignore_arr->len) ? 
                    ignore_arr->data[i] : ignore_arr_cur->data[i - ignore_arr->len];

                if (path_matches_pathspec(norm_path, spec)) {
                    ignore = 1;
                    break;
                } 
            }
        }

        if (ignore) {
            continue;
        }

        if (ent.de_type == FS_ISFILE) {
            strarr_push(result, norm_path);
        } else if (ent.de_type == FS_ISDIR) {
            char *subfolder = sstrdup(ent.de_path);
            folder_add_files(result, repo, subfolder, tracked_only, ignore_arr_cur);
            free(subfolder);
            free(norm_path);
        }
    }

    closedir(dir);
    strarr_free(ignore_arr_cur);
}

strarr_t *repo_all_files(const git_repo *repo, int tracked_only) {
    strarr_t *result = strarr_new();
    strarr_t *ignore_arr = strarr_new();
    folder_add_files(result, repo, repo->root_path, tracked_only, ignore_arr);
    strarr_free(ignore_arr);
    return result;
}