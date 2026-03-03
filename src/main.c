#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "commands.h"
#include "logging.h"

int main(int argc, char* argv[]) {
    arg_list args = {0};
    parse_args(&args, argc, argv);

    int cmd = 0;
    int ret_code = 0;

    if (argc > 1) {
        for (size_t i = 0; i < COMMANDS_SIZE; i++) {
            if (strcmp(cmd_func_table[i].name, argv[1]) == 0) {
                ret_code = cmd_func_table[i].handler(&args);
                cmd = 1;
                break;
            }
        }
    }

    if (!cmd) {
        ret_code = run_no_command(&args);
    }

    return ret_code;
}