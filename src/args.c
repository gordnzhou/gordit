#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "args.h"
#include "utils.h"
#include "logging.h"
#include "colour.h"
#include "parse.h"

const char *cmd_optstring(const char *command) {
    #define OPT_TOG_GETOPT_TOK ""
    #define OPT_NUM_GETOPT_TOK ":"
    #define OPT_STR_GETOPT_TOK ":"

    #define Y(opt_type, char, name, desc, usage) \
        #char opt_type##_GETOPT_TOK

    #define X(name, options, has_pargs, desc, p) \
        if (strcmp(#name, command) == 0) {       \
            return ":" options "h";              \
        }
    GIT_ARGS
    #undef X
    #undef Y

    fatal("cannot get optstring of %s", command);
}

void print_helpstring(const char *command) {
    #define Y(o, char, n, desc, u) \
        printf("  -%-3s %-40s\n", #char , desc);

    #define X(name, options, has_ps, desc, parg_name)            \
        if (strcmp(#name, command) == 0) {                       \
            colour_print(COLOUR_BOLD, "%s options:\n", #name);   \
            options                                              \
            return;                                              \
        }
    GIT_ARGS
    #undef X
    #undef Y

    fatal("cannot get help string of %s", command);
}

const char *cmd_usagestring(const char *command) {
    #define PARGS_USAGE_0(name) ""
    #define PARGS_USAGE_1(name) name ""

    #define Y(o, c, n, d, usage) usage " "
    #define X(name, options, has_pargs, desc, parg_name)          \
        if (strcmp(#name, command) == 0) {                        \
            return options PARGS_USAGE_##has_pargs (parg_name);   \
        }
    GIT_ARGS
    #undef X
    #undef Y

    fatal("cannot get usage string of %s", command);
}

void cmd_usage_print(const char *program, const char *command) {
    printf("usage: %s %s %s\n\n", program, command, cmd_usagestring(command));
    print_helpstring(command);
}

void default_help_print(const char *program) {
    printf("usage: %s <command> [-h] [<args>]\n\nAll Commands:\n", program);
    #define X(name, options, has_pargs, desc, p) printf("  %-15s %-40s\n", #name , desc);    
    GIT_ARGS
    #undef X
    printf("\n");
}

int command_main(int argc, char *const argv[]) {
    char *command = argc >= 2 ? argv[1] : "";
    char *program = MY_NAME;
    int opt;
    int status = 0;
    int cmd_found = 0;
    int show_help = 0;
    optind = 2;
    opterr = 0;

    #define OPT_TOG_ASSIGN(name) arglist.name = 1
    #define OPT_NUM_ASSIGN(name) arglist.name = atoi(optarg)
    #define OPT_STR_ASSIGN(name) arglist.name = optarg

    #define CHAR_a 'a'
    #define CHAR_b 'b'
    #define CHAR_c 'c'
    #define CHAR_d 'd'
    #define CHAR_e 'e'
    #define CHAR_f 'f'
    #define CHAR_g 'g'
    #define CHAR_h 'h'
    #define CHAR_i 'i'
    #define CHAR_j 'j'
    #define CHAR_k 'k'
    #define CHAR_l 'l'
    #define CHAR_m 'm'
    #define CHAR_n 'n'
    #define CHAR_o 'o'
    #define CHAR_p 'p'
    #define CHAR_q 'q'
    #define CHAR_r 'r'
    #define CHAR_s 's'
    #define CHAR_t 't'
    #define CHAR_u 'u'
    #define CHAR_v 'v'
    #define CHAR_w 'w'
    #define CHAR_x 'x'
    #define CHAR_y 'y'
    #define CHAR_z 'z'

    #define PARGS_PARSE_0
    #define PARGS_PARSE_1                            \
        for (int i = optind; i < argc; i++) {        \
            if (arglist.size_pargs >= 100) {         \
                fatal("too many arguments");         \
            }                                        \
            arglist.size_pargs++;                    \
            arglist.pargs[i-optind] = argv[i];       \
        }

    #define Y(opt_type, char, name, desc, usage) \
        case CHAR_##char: opt_type##_ASSIGN(name); break;

    #define X(name, options, has_pargs, desc, parg_name)                        \
    if (strcmp(#name, command) == 0) {                                          \
        assert(cmd_found == 0); cmd_found = 1;                                  \
        ARGLIST_STRUCT(name) arglist = { 0 };                                   \
        const char *optstring = cmd_optstring(#name);                           \
        while ((opt = getopt(argc, argv, optstring)) != -1) {                   \
            switch(opt) {                                                       \
                options                                                         \
                case ':':                                                       \
                    status = error("option '-%c' requires a value\n", optopt);  \
                    show_help = 1;                                              \
                    break;                                                      \
                case '?':                                                       \
                    status = error("unknown option '-%c'.\n", optopt);          \
                    show_help = 1;                                              \
                    break;                                                      \
                case 'h':                                                       \
                    show_help = 1;                                              \
                    break;                                                      \
                default:                                                        \
                    abort();                                                    \
            }                                                                   \
        }                                                                       \
        PARGS_PARSE_ ##has_pargs                                                \
        if (show_help) {                                                        \
            cmd_usage_print(program, command);                                  \
        } else {                                                                \
            run_ ##name (&arglist);                                             \
        }                                                                       \
    }
    GIT_ARGS
    #undef X 
    #undef Y

    if (cmd_found) {
        return status;
    }

    if (strcmp(command, "") == 0) {
        status = 1;
        default_help_print(program);
    }  else if (strcmp(command, "-h") == 0) {
        default_help_print(program);
    } else {
        status = 1;
        info("'%s' is not a valid command. See '%s -h' for a list of commands.", command, program);
    }

    return status;
}