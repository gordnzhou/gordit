#ifndef LOGGING_H
#define LOGGING_H

#ifdef __GNUC__
#define NORETURN __attribute__((__noreturn__))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
#else
#define NORETURN
#endif

#ifdef _DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

#define MY_NAME "gordit"

int info(const char *fmt, ...);
int warn(const char *fmt, ...);
int error(const char *fmt, ...);
void fatal(const char *fmt, ...) NORETURN;

#endif