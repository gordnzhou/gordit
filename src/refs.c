#include <stdlib.h>

#include "refs.h"
#include "logging.h"
#include "utils.h"
#include "filesystem.h"

void ref_full_path(const git_repo *repo, const char *ref_path, char *full_path) {
    assert(strstr(ref_path, REFS_NAME) == ref_path);
    fs_path_join(repo->git_folder_path, ref_path, full_path);
}

void write_ref(const git_repo *repo, const char *ref_path, const obj_hash *commit) {
    char full_path[PATH_MAX];
    ref_full_path(repo, ref_path, full_path);
    
    FILE *fptr = sfopen(full_path, "w");
    sfputs(*commit, fptr, full_path);
    fclose(fptr);
}

void del_ref(const git_repo *repo, const char *ref_path) {
    char full_path[PATH_MAX];
    ref_full_path(repo, ref_path, full_path);

    sremove(full_path);
}

int read_ref(const git_repo *repo, const char *ref_path, obj_hash *out_hash) {
    char full_path[PATH_MAX];
    ref_full_path(repo, ref_path, full_path);

    if (!fs_file_exists(full_path)) {
        return -1;
    }

    FILE *fptr = sfopen(full_path, "r");
    sfgets(*out_hash, OBJ_HASH_SIZE, fptr, full_path, 1);
    fclose(fptr);
    return 0;
}

char *local_ref_path(const char *local_branch_name) {
    char *ret = smalloc(strlen(LOCAL_REFS_NAME) + strlen(local_branch_name) + 1);
    fs_path_join(LOCAL_REFS_NAME, local_branch_name, ret);
    return ret;
}

void write_branch_local(const git_repo *repo, char *name, const obj_hash *commit) {
    char *ref_path = local_ref_path(name);
    write_ref(repo, ref_path, commit);
    free(ref_path);
}

void delete_branch_local(const git_repo *repo, char *name) {
    char *ref_path = local_ref_path(name);
    del_ref(repo, ref_path);
    free(ref_path);
}

int read_branch_local(const git_repo *repo, char *name, obj_hash *out_hash) {
    char *ref_path = local_ref_path(name);
    int ret = read_ref(repo, ref_path, out_hash);
    free(ref_path);
    return ret;
}

char *indirect_ref_str(char *ref_path) {
    size_t len = strlen(INDIRECT_REF_HEADER) + strlen(ref_path) + 1;
    char *ret = smalloc(len);
    snprintf(ret, len, "%s%s", INDIRECT_REF_HEADER, ref_path);
    return ret;
}

void move_head(const git_repo *repo, char *branch_name) {
    FILE *fptr = sfopen(repo->head_path, "w");
    char *ref_path = local_ref_path(branch_name);
    char *contents = indirect_ref_str(ref_path);

    sfputs(contents, fptr, repo->head_path);
    fclose(fptr);
    free(ref_path);
    free(contents);
}

void detach_head(const git_repo *repo, const obj_hash *commit) {
    FILE *fptr = sfopen(repo->head_path, "w");
    sfputs(*commit, fptr, repo->head_path);
    fclose(fptr);
}

char *read_head(const git_repo *repo, int *is_detached) {
    #define BUF_SIZE 256

    char *buf = smalloc(BUF_SIZE);

    FILE *fptr = sfopen(repo->head_path, "r");
    sfgets(buf, BUF_SIZE, fptr, repo->head_path, 0);
    fclose(fptr);

    if (strstr(buf, INDIRECT_REF_HEADER) == buf) {
        *is_detached = 0;
        return buf + strlen(INDIRECT_REF_HEADER);
    }

    if (strlen(buf) != OBJ_HASH_SIZE - 1) {
        fatal("head is corrupted: it neither points to a ref nor a hash");
    }

    *is_detached = 1;
    return buf;
}

