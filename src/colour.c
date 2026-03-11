#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "colour.h"

int colour_print(const char colour[], const char *fmt, ...) {
    va_list args;
    int ret;
    int tty = isatty(STDOUT_FILENO);

    if (tty && colour)
        fputs(colour, stdout);

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    if (tty && colour)
        fputs(COLOUR_RESET, stdout);

    return ret;
}