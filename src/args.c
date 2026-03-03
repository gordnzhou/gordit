#include <stdio.h>
#include <stdlib.h>

#include "args.h"

int parse_args(arg_list *args, int argc, char *const argv[]) {
    // TODO: use getopt to properly parse args
    // - each command (including just "gordit") has own set of options
    // - help message: list commands and options for "gordit"
    // - args after "--" are pathspec

    args->count = argc;
    args->all = argv;

    args->cmd_args_count = argc > 2 ? argc - 2 : 0;
    args->cmd_args = argc > 2 ? argv + 2 : 0;

    args->message = argc > 2 ? argv[2] : NULL;
    
    return 0;
}