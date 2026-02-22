#ifndef REFS_H
#define REFS_H

#include "repo.h"

#define START_BRANCH "main"
#define INDIRECT_REF_HEADER "ref: "

void write_ref(const git_repo *repo, const char *ref_path, const obj_hash *commit);
void del_ref(const git_repo *repo, const char *ref_path);
int read_ref(const git_repo *repo, const char *ref_path, obj_hash *out_hash);

void write_branch_local(const git_repo *repo, char *name, const obj_hash *commit);
void delete_branch_local(const git_repo *repo, char *name);
int read_branch_local(const git_repo *repo, char *name, obj_hash *out_hash);

// hash should point to commit object (lightweight tag) OR tag object (annotated tag)
void write_tag(const git_repo *repo, char *name, const obj_hash *hash);
void delete_tag(const git_repo *repo, char *name);
int read_tag(const git_repo *repo, char *name, obj_hash *out_hash);

void detach_head(const git_repo *repo, const obj_hash *commit);
void move_head(const git_repo *repo, char *branch_name);

int is_head_detached(const git_repo *repo);

// @return 1 if head is detached (no branch ref), else 0.
int read_head(const git_repo *repo, char *out, size_t out_size);
#endif