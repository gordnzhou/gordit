#ifndef PARSE_H
#define PARSE_H

#include "args.h"

int run_init(const arglist_init_t   *);
int run_add(const arglist_add_t    *);
int run_rm(const arglist_rm_t     *);
int run_commit(const arglist_commit_t *);
int run_log(const arglist_log_t *);
int run_status(const arglist_status_t *);
int run_branch(const arglist_branch_t *);
int run_checkout(const arglist_checkout_t *);
int run_diff(const arglist_diff_t *);

#endif