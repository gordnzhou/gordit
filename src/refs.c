#include <stdlib.h>
#include <string.h>

#include "refs.h"
#include "logging.h"
#include "utils.h"
#include "filesystem.h"

int is_valid_branch_name(const char *branch_name) {
    char f = branch_name[0];
    if (f == '.' || f == '/' || f == '\\') {
        return 0;
    }
    return strpbrk(branch_name, "*~^:?[") == NULL;
}

void free_ref(git_ref *ref) {
    free(ref->name);
    free(ref);
}

char *ref_full_path(const git_repo *repo, enum ref_type type, const char *ref_name) {
    char *out = smalloc(PATH_MAX);
    switch (type) {
        case REF_LOCAL:
            fs_path_join(repo->local_refs_path, ref_name, out);
            break;
        case REF_TAG:
            fs_path_join(repo->tag_refs_path, ref_name, out);
            break;
        case REF_REMOTE:
            fs_path_join(repo->remote_refs_path, ref_name, out);
            break;
        case DIRECT:
            DEBUG_PRINT("tried to read direct hash as a ref");
            assert(0);
    }

    return out;
}

void write_ref(const git_repo *repo, enum ref_type type, const char *ref_name, const obj_hash *commit) {
    char *path = ref_full_path(repo, type, ref_name);
    FILE *fptr = sfopen(path, "w");
    sfputs(*commit, fptr, path);
    fclose(fptr);
    free(path);
}

int del_ref(const git_repo *repo, enum ref_type type, const char *name) {
    char *path = ref_full_path(repo, type, name);
    if (!fs_file_exists(path)) {
        free(path);
        return 0;
    }
    
    sremove(path);
    free(path);

    return 1;
}

git_ref *read_ref(const git_repo *repo, enum ref_type type, const char *name, int fail_if_empty) {
    git_ref *ref = smalloc(sizeof(*ref));
    ref->name = sstrdup(name);
    ref->type = type;
    ref->empty_hash = 0;

    char *path = ref_full_path(repo, ref->type, ref->name);
    if (!fs_file_exists(path)) {

        if (fail_if_empty) {
            fatal("ref not found at '%s'", path);
        }

        ref->empty_hash = 1;
        return ref;
    }

    FILE *fptr = sfopen(path, "r");
    sfgets(ref->hash, sizeof(obj_hash), fptr, path, 1);
    fclose(fptr);

    return ref;
}

char *indirect_ref_str(const char *ref_path) {
    size_t len = strlen(INDIRECT_REF_HEADER) + strlen(ref_path) + 1;
    char *ret = smalloc(len);
    snprintf(ret, len, "%s%s", INDIRECT_REF_HEADER, ref_path);
    return ret;
}

void move_head(const git_repo *repo, git_ref *ref) {
    char *contents = NULL;

    if (ref->type == DIRECT) {
        contents = ref->hash;
    } else { 
        char *ref_path = ref_full_path(repo, ref->type, ref->name);

        // parses into: "ref: refs/<type>/<name>""
        int git_folder_len = strlen(repo->git_path);
        assert(memcmp(ref_path, repo->git_path, git_folder_len) == 0);
        contents = indirect_ref_str(ref_path + git_folder_len + 1);
        free(ref_path);
    }

    FILE *fptr = sfopen(repo->head_path, "w");
    sfputs(contents, fptr, repo->head_path);
    fclose(fptr);
    free(contents);
}

#define HEAD_BUF_SIZE 256

git_ref *read_head(const git_repo *repo) {
    char buf[HEAD_BUF_SIZE];
    FILE *fptr = sfopen(repo->head_path, "r");
    sfgets(buf, HEAD_BUF_SIZE, fptr, repo->head_path, 0);
    fclose(fptr);

    int is_symbolic = strstr(buf, INDIRECT_REF_HEADER) == buf; 
    if (!is_symbolic && strlen(buf) + 1 != sizeof(obj_hash)) {
        fatal("head is corrupted: it is neither symbolic nor contains a hash");
    }

    if (is_symbolic) {
        char  *ref_path = buf + strlen(INDIRECT_REF_HEADER);

        enum ref_type type;
        char temp[PATH_MAX];
        fs_path_dirname(ref_path, temp);
        const char *type_folder = fs_path_pbasename(temp);
        if (strcmp(type_folder, LOCAL_REFS_NAME) == 0) {
            type = REF_LOCAL;
        } else if (strcmp(type_folder, TAG_REFS_NAME) == 0) {
            type = REF_TAG;
        } else if (strcmp(type_folder, REMOTE_REFS_NAME) == 0) {
            type = REF_REMOTE;
        } else {
            fatal("invalid ref path '%s'", ref_path);
        }

        // only case where returned ref could have empty hash
        return read_ref(repo, type, fs_path_pbasename(ref_path), 0);
    }

    git_ref *ref = smalloc(sizeof(*ref));
    ref->type = DIRECT;
    ref->name = NULL;
    snprintf(ref->hash, OBJ_HASH_SIZE, "%s", buf);
    return ref;
}

int is_head_detached(git_ref *head_ref) {
    return head_ref->type != REF_LOCAL;
}

int backup_head(const git_repo *repo) {
    git_ref *head_ref = read_head(repo);
    if (head_ref->empty_hash) {
        return 0;
    }

    FILE *fptr = sfopen(repo->backup_head_path, "w");
    sfputs(head_ref->hash, fptr, repo->backup_head_path);
    fclose(fptr);
    free_ref(head_ref);

    return 1;
}

strarr_t *refs_all_names(const git_repo *repo, enum ref_type type) {
    const char *folder;
    if (type == REF_LOCAL) {
        folder = repo->local_refs_path;
    } else if (type == REF_TAG) {
        folder = repo->tag_refs_path;
    } else if (type == REF_REMOTE) {
        folder = repo->remote_refs_path;
    } else {
        assert(0);
    }

    strarr_t *ret = strarr_new();

    DIR *dir = sopendir(folder);
    fs_dirent dirent = { 0 };
    int status;
    while ((status = fs_readdir(dir, &dirent, folder)) != 0) {
        if (status == -1) {
            fatal("could not read ref folder '%s'", folder);
        }

        strarr_push(ret, dirent.de_name);
    }

    closedir(dir);

    return ret;
}