#include <stdio.h>
#include <stdlib.h>

#include "args.h"

int parse_args(arg_list *args, int argc, char *const argv[]) {
    if (argc <= 1) {
        printf("usage: gordit <command> [<args>]\n");
        return 0;
    }

    // TODO: use getopt to properly parse args
    args->count = argc;
    args->cmd_args_count = argc >= 2 ? argc - 2 : 0;
    args->all = argv;
    args->cmd_args = argv + 2;
    args->message = argc > 2 ? argv[2] : NULL;
    
    return 0;
}