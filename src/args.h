#ifndef ARGS_H
#define ARGS_H

#define MAX_PATHSPEC 100

#define ARGLIST_STRUCT(name) arglist_##name##_t

#define OPT_TOG OPT_TOG
#define OPT_NUM OPT_TOG
#define OPT_STR OPT_STR

// X(type, command, name, description, usage)
#define INIT_OPTIONS \
    Y(OPT_TOG, b, bare, "create a bare repository", "[-b]")

#define ADD_OPTIONS
#define RM_OPTIONS

#define COMMIT_OPTIONS                                                      \
    Y(OPT_STR, m, message, "commit message", "[-m <message>]")              \
    Y(OPT_STR, n, name, "name of commit's author", "[-a <author-name>]")    \
    Y(OPT_STR, e, email, "email of commit's author", "[-e <author-email>]")

#define LOG_OPTIONS
#define STATUS_OPTIONS

#define BRANCH_OPTIONS \
    Y(OPT_TOG, l, list, "list all local branches", "[-l]") \
    Y(OPT_STR, n, new_branch, "create a local branch", "[-n <branch-name>]") \
    Y(OPT_STR, d, del_branch, "delete a local branch", "[-d <branch-name>]") 

#define CHECKOUT_OPTIONS \
    Y(OPT_STR, b, branch, "branch to checkout", "[-b <branch-name>]")

#define DIFF_OPTIONS

// X(name, OPTIONS, has_pathspec, description)
#define GIT_ARGS                                                                  \
    X(init,   INIT_OPTIONS, 0, "Creates a new repository")                        \
    X(add,    ADD_OPTIONS, 1, "Add files to the staging index")                   \
    X(rm,     RM_OPTIONS, 1, "Remove files from the staging index")               \
    X(commit, COMMIT_OPTIONS, 0, "Snapshot your repo's working tree")             \
    X(log, LOG_OPTIONS, 1, "Show repo's commit history")                          \
    X(status, STATUS_OPTIONS, 1, "Show the working tree's current status")        \
    X(branch, BRANCH_OPTIONS, 0, "Manage your branches")                          \
    X(checkout, CHECKOUT_OPTIONS, 1, "Checkout previous commits and their files") \
    X(diff, DIFF_OPTIONS, 1, "Show diff between files, commits, and more")       \

#define OPT_TOG_TYPE int
#define OPT_NUM_TYPE int
#define OPT_STR_TYPE char *
#define Y(opt_type, char, name, desc, usage) opt_type##_TYPE name;

#define PATHSPEC_FIELDS_0
#define PATHSPEC_FIELDS_1 int ps_size; char *pathspecs[MAX_PATHSPEC];

#define X(name, options, has_ps, desc) \
typedef struct {                       \
    PATHSPEC_FIELDS_ ##has_ps          \
    options                            \
} ARGLIST_STRUCT(name);

GIT_ARGS

#undef Y
#undef X

int command_main(int argc, char *const argv[]);

#endif