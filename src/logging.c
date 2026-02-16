#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "logging.h"

int info(const char *fmt, ...) { 
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "%s: ", MY_NAME);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args); 

    return 0;
}

int warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "warning: ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args); 

    return 0;
}

int error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args); 
    
    return -1;
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "fatal: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args); 
    exit(128);
}
 