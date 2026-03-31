#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pathspec.h"
#include "filesystem.h"
#include "logging.h"
#include "dircache.h"
#include "tree.h"
#include "utils.h"

#define MAX_PATHSPEC_PARTS 100

char *path_git_name(const git_repo *repo, const char *path) {
    if (strlen(path) + 1 > PATH_MAX) {
        fatal("path '%s' is too long (> %d)", path, PATH_MAX);
    } 

    static char abs_path[PATH_MAX];
    if (fs_path_abs(path, abs_path)) {
        fatal("unable to get absolute path of '%s'", path);
    }

    git_path_clean(abs_path);

    if (!is_path_in_folder(repo->root_path, abs_path)) {
        fatal("'%s' is outside of current repository (%s)", abs_path, repo->root_path);
    }
    if (is_path_in_folder(repo->git_path, abs_path)) {
        fatal("'%s' is inside repo's internal git folder", abs_path);
    }

    int root_len = strlen(repo->root_path);
    int path_len = strlen(abs_path);
    if (root_len == path_len) {
        return sstrdup("");
    }
    
    return sstrdup(abs_path + root_len + 1);
}

pathspec_item *pathspec_parse(const git_repo *repo, const char *relative_dir, const char *arg) {
    pathspec_item *ps = smalloc(sizeof(*ps));
    ps->orig = arg;

    char arg_path[PATH_MAX];
    char *cwd = relative_dir ? NULL : sgetcwd(NULL, PATH_MAX);
    fs_path_join(relative_dir ? relative_dir : cwd, arg, arg_path);

    static struct stat st;
    if (stat(arg, &st) == 0) {
        ps->fullname = path_git_name(repo, arg_path);
    } else {
        ps->fullname = smalloc(PATH_MAX);
        repo_rel_path(repo, arg_path, ps->fullname);
    }
    git_path_clean(ps->fullname);
    if (!cwd) free(cwd);

    ps->parts = smalloc(MAX_PATHSPEC_PARTS*sizeof(pathspec_slice));
    ps->nparts = 0;

    char *fullname_copy = sstrdup(ps->fullname);
    char *token = strtok(fullname_copy, "/\\");
    while (token != NULL) {
        if (ps->nparts == MAX_PATHSPEC_PARTS) {
            fatal("pathspec '%s' has too many parts", arg);
        }
        pathspec_slice slice = { 0 };
        slice.str = ps->fullname + (token - fullname_copy);
        slice.len = strlen(token);
        slice.type = STR;
        if (strcmp(WILDCARD_RECUR_TOKEN, token) == 0) {
            slice.type = WILD_RECUR;
        } else if (strpbrk(token, "?*[]")) {
            slice.type = GLOB_PATT;
        }

        ps->parts[ps->nparts++] = slice;
        token = strtok(NULL, "/\\");
    }

    return ps;
}

void pathspec_free(pathspec_item *pathspec) {
    free(pathspec->parts);
    free(pathspec->fullname);
    free(pathspec);
}

git_file_list *git_fl_init() {
    git_file_list *list = smalloc(sizeof(*list));
    list->size = 0;
    list->capacity = 16;
    list->items = smalloc(list->capacity*sizeof(git_file_item *));
    return list;
}

// TODO: could convert to hash map
int git_fl_find(const git_file_list *list, const char *name) {
    for (int i = 0; i < list->size; i++) {
        if (strcmp(name, list->items[i]->name) == 0) {
           return 1;
        }
    }

    return 0;
}

void git_fl_add(git_file_list *list, const char *name) {
    list->changed = 1; 

    if (git_fl_find(list, name)) {
        return;
    }

    git_file_item *item = smalloc(sizeof(*item));
    snprintf(item->name, PATH_MAX, "%s", name);
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, list->capacity*sizeof(git_file_item *));
    }
    list->items[list->size++] = item;
}

void git_fl_free(git_file_list *list) {
    for (int i = 0; i < list->size; i++) {
        free(list->items[i]);
    }
    free(list->items);
    free(list);
}

int pathspec_slice_matches(const pathspec_slice *part, const char *name) {
    if (part->type == WILD_RECUR) {
        return 1;
    }
    if (part->type == STR) {
        size_t namelen = strlen(name);
        return namelen == part->len && memcmp(part->str, name, part->len) == 0;
    }
    if (part->type == GLOB_PATT) {
        const char *pend = part->str + part->len;
        const char *p = part->str, *s = name;
        const char *star_p = NULL, *star_s = NULL;

        while (*s) {
            if (p < pend && (*p == '?' || *p == *s)) {
                p++; s++;
            } else if (p < pend && *p == '*') {
                star_p = p++;
                star_s = s;
            } else if (p < pend && *p == '[') {
                ++p;
                int negate = (p < pend && *p == '!'), matched = 0;
                if (negate) {
                    ++p;
                }

                while (p < pend && *p != ']') {
                    if ((p+1) < pend && *(p+1) == '-' && 
                        (p+2) < pend && *(p+2) != ']') {
                        if (*s >= *p && *s <= *(p+2)) matched = 1;
                        p += 3;
                    } else {
                        if (*s == *p) matched = 1;
                        p++;
                    }
                }
                if (p < pend && *p == ']') p++;

                if (matched == negate) {
                    if (!star_p) return 0;
                    p = star_p + 1;
                    s = ++star_s;
                } else {
                    s++;
                }
            } else if (star_p) {
                p = star_p + 1;
                s = ++star_s;
            } else {
                return 0;
            }
        }

        while (p < pend && *p == '*') p++;
        return p == pend;
    }

    fatal("unimplemented");
}

int pathspec_full_matches(const pathspec_item *pathspec, const char *name) {
    char *name_copy = sstrdup(name);
    const char *name_part = strtok(name_copy, "/");
    int match_part = 0;
    int wild_recurse = 0;

    while (name_part != NULL && match_part < pathspec->nparts) {
        pathspec_slice *part = pathspec->parts + match_part;
        if (part->type == WILD_RECUR) {
            wild_recurse = 1;
            match_part++;
        } else if (wild_recurse) {
            if (pathspec_slice_matches(part, name_part)) {
                match_part++;
                wild_recurse = 0;
            }
            name_part = strtok(NULL, "/");
        } else {
            if (!pathspec_slice_matches(part, name_part)) {
                return 0;
            }
            match_part++;
            name_part = strtok(NULL, "/");
        }
    }

    return match_part == pathspec->nparts;
}

int is_ignored(const git_repo *repo, const char *name) {
    static char folder[PATH_MAX], filepath[PATH_MAX], buf[PATH_MAX];
    fs_path_join(repo->root_path, name, folder);
    int len = strlen(folder);

    for (int i = strlen(repo->root_path); i < len; i++) {
        if (folder[i] != '/') { continue; }

        folder[i] = '\0';
        fs_path_join(folder, GIT_IGNORE_NAME, filepath);
        FILE *file = fopen(filepath, "r");
        if (file) {
            while (sfgets(buf, PATH_MAX, file, filepath, 0)) {
                buf[strcspn(buf, "\n")] = '\0';
                pathspec_item *ps = pathspec_parse(repo, folder, buf);
                if (ps == NULL) {
                    continue;
                }
                if (pathspec_full_matches(ps, name)) {
                    fclose(file);
                    return 1;
                }
                pathspec_free(ps);
            }
            fclose(file);
        }
        folder[i] = '/';
    }

    return 0;
}

void git_fl_add_wt(const git_repo *repo, git_file_list *list, const char *path, int check_ignores) {
    char *name = path_git_name(repo, path);  
    if (check_ignores && is_ignored(repo, name)) {
        free(name);
        return;
    }

    git_fl_add(list, name);
    free(name);
}

void working_tree_walk(git_file_list *out, const git_repo *repo, const char *folder,  
    const pathspec_item *pathspec, int cur_depth, int wild_recurse,
    int check_ignores
) { 
    int found = cur_depth == pathspec->nparts;

    if (!found && !wild_recurse) {
        pathspec_slice *part = pathspec->parts + cur_depth;
        if (part->type == WILD_RECUR) {
            wild_recurse = 1;
            while (cur_depth < pathspec->nparts && pathspec->parts[cur_depth].type == WILD_RECUR) {
                cur_depth++;
            }
            found = cur_depth == pathspec->nparts;
        } else if (part->type == STR) {
            char target[PATH_MAX];
            fs_path_join(folder, part->str, target);
            target[strlen(target) - (strlen(part->str) - part->len)] = '\0';
            struct stat st;
            if (stat(target, &st)) {
                return;
            }
            if (S_ISREG(st.st_mode)) {
                if (cur_depth == pathspec->nparts - 1) {
                    git_fl_add_wt(repo, out, target, check_ignores);
                }
            } else if (S_ISDIR(st.st_mode)) {
                working_tree_walk(out, repo, target, pathspec, cur_depth + 1, 0, check_ignores);
            }
            return;
        }
    }

    DIR *dir = sopendir(folder);
    fs_dirent ent = { 0 };
    int ret = 0;
    while ((ret = fs_readdir(dir, &ent, folder)) != 0) {
        if (ret == -1) {
            continue;
        }
        if (strcmp(ent.de_name, ".git") == 0 || strcmp(ent.de_name, GIT_FOLDER) == 0) {
            continue;
        }

        char *subfolder = sstrdup(ent.de_path);
        if (found) {
            if (ent.de_type == FS_ISFILE) {
                git_fl_add_wt(repo, out, ent.de_path, check_ignores);
            } else if (ent.de_type == FS_ISDIR) {
                working_tree_walk(out, repo, subfolder, pathspec, cur_depth, 0, check_ignores);
            }
            continue;
        } else {
            if (pathspec_slice_matches(pathspec->parts + cur_depth, ent.de_name)) {
                if (ent.de_type == FS_ISFILE) {
                    if (cur_depth == pathspec->nparts - 1) {
                        git_fl_add_wt(repo, out, ent.de_path, check_ignores);
                    }
                } else if (ent.de_type == FS_ISDIR) {
                    working_tree_walk(out, repo, subfolder, pathspec, cur_depth + 1, 0, check_ignores);
                }
            } else if (wild_recurse && ent.de_type == FS_ISDIR) {
                working_tree_walk(out, repo, subfolder, pathspec, cur_depth, 1, check_ignores);
            }
        }
       free(subfolder);
    }
    closedir(dir);
}

int git_fl_working_tree_files(git_file_list *out, const git_repo *repo, const pathspec_item *pathspec, int check_ignores) {
    out->changed = 0;
    working_tree_walk(out, repo, repo->root_path, pathspec, 0, 0, check_ignores);
    return out->changed; 
}

void tree_obj_walk(git_file_list *out, const git_obj_tree *tree,  
    const pathspec_item *pathspec, 
    const char *path, int cur_depth, int wild_recurse
) {
    int found = cur_depth == pathspec->nparts;

    if (!found && !wild_recurse) {
        pathspec_slice *part = pathspec->parts + cur_depth;
        if (part->type == WILD_RECUR) {
            wild_recurse = 1;
            while (cur_depth < pathspec->nparts && pathspec->parts[cur_depth].type == WILD_RECUR) {
                cur_depth++;
            }
            found = cur_depth == pathspec->nparts;
        }
    }

    for (int i = 0; i < tree->size; i++) {
        git_tree_entry *entry = tree->entries[i];
        char *subpath = smalloc(PATH_MAX);
        snprintf(subpath, PATH_MAX, "%s%s%s", path, strcmp(path, "") == 0 ? "" : "/", entry->name);

        if (found) {
            if (entry->type == OBJ_TYPE_BLOB) {
                git_fl_add(out, subpath);
            } else if (entry->type == OBJ_TYPE_TREE) {
                tree_obj_walk(out, entry->u.tree, pathspec, subpath, cur_depth, 0);
            }
            continue;
        } else {
            if (pathspec_slice_matches(pathspec->parts + cur_depth, entry->name)) {
                if (entry->type == OBJ_TYPE_BLOB) {
                    if (cur_depth == pathspec->nparts - 1) {
                        git_fl_add(out, subpath);
                    }
                } else if (entry->type == OBJ_TYPE_TREE) {
                    tree_obj_walk(out, entry->u.tree, pathspec, subpath, cur_depth + 1, 0);
                }
            } else if (wild_recurse && entry->type == OBJ_TYPE_TREE) {
                tree_obj_walk(out, entry->u.tree, pathspec, subpath, cur_depth , 1);
            }
        }
       free(subpath);
    }
}

int git_fl_tree_obj_files(git_file_list *out, const git_obj_tree *tree, const pathspec_item *pathspec) {
    out->changed = 0;
    tree_obj_walk(out, tree, pathspec, "", 0, 0);
    return out->changed;
}

int git_fl_dircache_files(git_file_list *out, const git_dircache *dircache, const pathspec_item *pathspec) {
    out->changed = 0;
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        if (pathspec_full_matches(pathspec, entry->name)) {
            git_fl_add(out, entry->name);
        }
    }
    return out->changed;
}


strarr_t *working_tree_all_files(const git_repo *repo, int check_ignores) {
    strarr_t *out = strarr_new();
    git_file_list *files = git_fl_init();
    pathspec_item *ps = pathspec_parse(repo, repo->root_path, ".");
    git_fl_working_tree_files(files, repo, ps, check_ignores);
    pathspec_free(ps);
    for (int i = 0 ; i < files->size; i++) {
        strarr_push(out, files->items[i]->name);
    }
    git_fl_free(files);
    return out;
}