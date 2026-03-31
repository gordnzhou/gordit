#ifndef ARGS_H
#define ARGS_H

#define PARGS_MAX 100

#define ARGLIST_STRUCT(name) arglist_##name##_t

#define OPT_TOG OPT_TOG
#define OPT_NUM OPT_TOG
#define OPT_STR OPT_STR

/*
define commands in macro GIT_ARGS:

X(name, options, has_pargs, description, pargs_name)
- name: literal of command's name
- options: <CMD-NAME>_OPTIONS (see below)
- has_pargs: 1 if command accepts positional args else 0
- description: command's help string
- pargs_name: name of positional args in help strng

define an options for a command in macro <CMD-NAME>_OPTIONS:

Y(type, command, name, description, usage)
- type: OPT_TOG, OPT_STR, OPT_NUM
- command: option's character [A-Za-z]
- name: name of option's struct field 
- description: option description string shown in help
- usage: option's string in command usage
*/

#define INIT_OPTIONS \
    Y(OPT_TOG, b, bare, "Create a bare repository", "[-b]") \
    Y(OPT_STR, s, start_branch, "Specify name of starting branch", "[-s <branch-name>]")

#define ADD_OPTIONS \
    Y(OPT_TOG, f, force, "Ignored files can also be added", "[-f]")

#define RM_OPTIONS                                                       \
    Y(OPT_TOG, s, staged_only, "Remove files from staging only", "[-s]") \
    Y(OPT_TOG, f, force, "Skip check for uncommitted changes", "[-f]")

#define COMMIT_OPTIONS                                                      \
    Y(OPT_STR, m, message, "Commit message", "[-m <message>]")              \
    Y(OPT_STR, n, name, "Name of commit's author", "[-a <author-name>]")    \
    Y(OPT_STR, e, email, "Email of commit's author", "[-e <author-email>]")

#define LOG_OPTIONS

#define STATUS_OPTIONS

#define BRANCH_OPTIONS \
    Y(OPT_TOG, l, list, "List all local branches", "[-l]") \
    Y(OPT_STR, n, new_branch, "Create a local branch", "[-n <branch-name>]") \
    Y(OPT_STR, d, del_branch, "Delete a local branch", "[-d <branch-name>]") 

#define CHECKOUT_OPTIONS \
    Y(OPT_STR, b, branch, "Branch to checkout", "[-b <branch-name>]")

#define DIFF_OPTIONS

#define CONFIG_OPTIONS \
    Y(OPT_TOG, g, global, "Use global config (local is used by default)", "[-g]") \
    Y(OPT_TOG, a, show_all, "Show all entries in config", "[-a]") \
    Y(OPT_TOG, d, delete, "Delete an entry", "[-d]")

#define GIT_ARGS                                                                              \
    X(init,   INIT_OPTIONS, 0, "Creates a new repository", "")                                \
    X(add,    ADD_OPTIONS, 1, "Add files to the staging index", "<pathspec>...")              \
    X(rm,     RM_OPTIONS, 1, "Remove files from the staging index", "<pathspec>...")          \
    X(commit, COMMIT_OPTIONS, 0, "Snapshot your repo's working tree", "")                     \
    X(log, LOG_OPTIONS, 1, "Show repo's commit history", "pathspec")                          \
    X(status, STATUS_OPTIONS, 1, "Show the working tree's current status", "<pathspec>...")   \
    X(branch, BRANCH_OPTIONS, 0, "Manage your branches", "")                                  \
    X(checkout, CHECKOUT_OPTIONS, 1, "Checkout previous commits and files", "<pathspec>...")  \
    X(diff, DIFF_OPTIONS, 1, "Show diff between files, commits, and more", "<pathspec>...")   \
    X(config, CONFIG_OPTIONS, 1, "Read and write repository and global configurations ", "<name> [<value>] (include value to WRITE)") \

#define OPT_TOG_TYPE int
#define OPT_NUM_TYPE int
#define OPT_STR_TYPE char *
#define Y(opt_type, char, name, desc, usage) opt_type##_TYPE name;

#define PARGS_FIELDS_0
#define PARGS_FIELDS_1 int size_pargs; char *pargs[PARGS_MAX];

#define X(name, options, has_pargs, desc, parg_name) \
typedef struct {                          \
    PARGS_FIELDS_ ##has_pargs             \
    options                               \
} ARGLIST_STRUCT(name);

GIT_ARGS

#undef Y
#undef X

int command_main(int argc, char *const argv[]);

#endif