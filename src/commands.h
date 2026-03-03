#ifndef COMMANDS_H
#define COMMANDS_H

#include "args.h"

#define COMMANDS_LIST(X)      \
    X(init, run_init)         \
    X(add, run_add)           \
    X(rm, run_rm)             \
    X(commit, run_commit)     \
    X(status, run_status)     \
    X(log, run_log)           \
    X(branch, run_branch)     \
    X(checkout, run_checkout) \

#define CMD_PROTOTYPE(name, func) int func(arg_list *);
    COMMANDS_LIST(CMD_PROTOTYPE)

int run_no_command(arg_list *);

typedef struct {
    const char *name;
    int (*handler)(arg_list *);
} command_entry;

#define TABLE_ENTRY(name, func) { #name, func },
command_entry cmd_func_table[] = {
    COMMANDS_LIST(TABLE_ENTRY)
};

#define COMMANDS_SIZE ((size_t)(sizeof(cmd_func_table) / sizeof(cmd_func_table[0])))

#endif