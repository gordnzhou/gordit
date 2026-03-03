#ifndef REFS_H
#define REFS_H

#include "repo.h"
#include "utils.h"

#define START_BRANCH "main"
#define INDIRECT_REF_HEADER "ref: "

enum ref_type {
    REF_LOCAL,
    REF_TAG,
    REF_REMOTE,
    DIRECT,
};

typedef struct {
    enum ref_type type;
    char *name; // NULL if type is DIRECT
    int empty_hash;
    obj_hash hash;
} git_ref;

int              del_ref(const git_repo *repo, enum ref_type type, const char *name);
git_ref        *read_ref(const git_repo *repo, enum ref_type type, const char *name, int fail_if_empty);
void           write_ref(const git_repo *repo, enum ref_type type, const char *ref_name, const obj_hash *commit);
strarr_t *refs_all_names(const git_repo *repo, enum ref_type type);

int is_valid_branch_name(const char *branch_name);

// @return 1 if head_ref is not a local branch
int is_head_detached(git_ref *head_ref);

// updates contents of HEAD file to point to ref. If type is DIRECT (no name), 
// HEAD's content is replaced with the ref hash, leaving HEAD in detached state
void move_head(const git_repo *repo, git_ref *ref);

// reads contents of HEAD file.
// @return HEAD's ref with hash resolved. if HEAD is just a hash, type is DIRECT and so name field is empty
git_ref *read_head(const git_repo *repo);

// Creates backup file of HEAD's resolved hash
// @return 1 on backup, 0 if nothing to backup
int backup_head(const git_repo *repo);

void free_ref(git_ref *ref);

#endif