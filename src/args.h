#ifndef ARGS_H
#define ARGS_H

typedef struct {
    int count;
    int cmd_args_count;
    char *const *all;
    char *const *cmd_args;
    char *message;
} arg_list;

int parse_args(arg_list *args, int argc, char *const argv[]);

#endif