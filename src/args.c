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

    #define X(name, options, has_ps, desc) \
        if (strcmp(#name, command) == 0) { \
            return ":" options "h";        \
        }
    GIT_ARGS
    #undef X
    #undef Y

    fatal("cannot get optstring of %s", command);
}

void print_helpstring(const char *command) {
    #define Y(o, char, n, desc, u) \
        printf("  -%-3s %-40s\n", #char , desc);

    #define X(name, options, has_ps, desc)                       \
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
    #define PATHSPEC_USAGE_0 
    #define PATHSPEC_USAGE_1 "<pathspec>... "

    #define Y(o, c, n, d, usage) usage " "
    #define X(name, options, has_ps, desc)               \
        if (strcmp(#name, command) == 0) {               \
            return options PATHSPEC_USAGE_##has_ps "";   \
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
    #define X(name, options, has_ps, desc) printf("  %-15s %-40s\n", #name , desc);    
    GIT_ARGS
    #undef X
    printf("\n");
}

int command_main(int argc, char *const argv[]) {
    char *command = argc >= 2 ? argv[1] : "";
    char *program = argv[0];
    int opt;
    int status = 0;
    int cmd_found = 0;
    int show_help = 0;
    optind = 2;
    opterr = 0;

    #define OPT_TOG_ASSIGN(name) arglist.name = 1
    #define OPT_NUM_ASSIGN(name) arglist.name = atoi(optarg)
    #define OPT_STR_ASSIGN(name) arglist.name = optarg

    #define PATHSPEC_PARSE_0
    #define PATHSPEC_PARSE_1                         \
        for (int i = optind-1; i < argc; i++) {      \
            if (arglist.ps_size >= 100) {            \
                fatal("too many arguments");         \
            }                                        \
            arglist.ps_size++;                       \
            arglist.pathspecs[i-optind+1] = argv[i]; \
        }

    #define Y(opt_type, char, name, desc, usage) \
        case ((#char)[0]): opt_type##_ASSIGN(name); break;

    #define X(name, options, has_ps, desc)                      \
    if (strcmp(#name, command) == 0) {                          \
        assert(cmd_found == 0); cmd_found = 1;                  \
        ARGLIST_STRUCT(name) arglist = { 0 };                   \
        const char *optstring = cmd_optstring(#name);           \
        while ((opt = getopt(argc, argv, optstring)) != -1) {   \
            switch(opt) {                                       \
                options                                         \
                case ':':                                       \
                    status = error("missing arg for '-%c'\n", optopt);      \
                    show_help = 1;                              \
                    break;                                      \
                case '?':                                       \
                    status = error("unknown option '-%c'.\n", optopt);   \
                    show_help = 1;                              \
                    break;                                      \
                case 'h':                                       \
                    show_help = 1;                              \
                    break;                                      \
                default:                                        \
                    abort();                                    \
            }                                                   \
        }                                                       \
        PATHSPEC_PARSE_ ##has_ps                                \
        if (show_help) {                                        \
            cmd_usage_print(program, command);                  \
        } else {                                                \
            run_ ##name (&arglist);                             \
        }                                                       \
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