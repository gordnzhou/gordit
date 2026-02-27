#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "commands.h"

int main(int argc, char* argv[]) {
    arg_list args = {0};
    parse_args(&args, argc, argv);

    char *command = argv[1];
    int ret_code = 0;
    for (size_t i = 0; i < COMMANDS_SIZE; i++) {
        if (strcmp(cmd_func_table[i].name, command) == 0) {
            ret_code = cmd_func_table[i].handler(&args);
        }
    }

    return ret_code;
}