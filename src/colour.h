#ifndef COLOUR_H
#define COLOUR_H

#define COLOUR_RESET   "\033[0m"
#define COLOUR_RED     "\033[31m"
#define COLOUR_GREEN   "\033[32m"
#define COLOUR_YELLOW  "\033[33m"
#define COLOUR_BLUE    "\033[34m"
#define COLOUR_MAGENTA "\033[35m"
#define COLOUR_CYAN    "\033[36m"
#define COLOUR_WHITE   "\033[37m"
#define COLOUR_BOLD    "\033[1m"

int colour_print(const char *colour, const char *fmt, ...);

#endif