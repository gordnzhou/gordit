#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"
#include "logging.h"

void *smalloc(size_t size) {
    void *ret = malloc(size);
    if (!ret && !size) {
        ret = malloc(1);
    } 
    if (!ret) {
        fatal("Out of memory, malloc could not allocate %lu bytes", (unsigned long)size);
    }
    return ret;
}

void *scalloc(size_t num_elements, size_t element_size) {
    void *ret = calloc(num_elements, element_size);
    if (!ret && !num_elements) {
        ret = calloc(1, element_size);
    }
    if (!ret) {
        fatal("Out of memory, calloc could not allocate %lu bytes", (unsigned long)element_size);
    }
    return ret;
}

void *srealloc(void *ptr, size_t new_size) {
    void *ret = realloc(ptr, new_size);
    if (!ret && !new_size) {
        ret = realloc(ptr, 1);
    } 
    if (!ret) {
        fatal("Out of memory, realloc could not allocate %lu bytes", (unsigned long)new_size);
    }
    return ret;
}

FILE *sfopen(const char *filepath, const char *mode) {
    FILE *ret = fopen(filepath, mode);
    if (!ret) {
        fatal("could not open '%s': %s", filepath, strerror(errno));
    }
    return ret;
}

char *sgetcwd(char *pathbuf, size_t pathsize) {
    char *ret = getcwd(pathbuf, pathsize);
    if (!ret) {
        fatal("could not get cwd: %s", strerror(errno));
    }
    return ret;
}

void freadb_full(void *dest, size_t filesize, FILE *file, const char *name) {
    size_t read = fread(dest, 1, filesize, file);
    if (read != filesize) {
        fatal("could not fully read to: %s", name);
    }
}

void fwriteb_full(void *src, size_t filesize, FILE *file, const char *name) {
    size_t written = fwrite(src, 1, filesize, file);
    if (written != filesize) {
        fatal("could not fully write to: %s", name);
    }
}

void sfputs(const char *str, FILE *file, const char *name) {
    if (fputs(str, file) != 0) {
        fatal("could not write string to %s: %s", name, strerror(errno));
    }
}

void sfgets(char *buf, size_t buf_len, FILE *file, const char *name, int strict_bufsize) {
    if (fgets(buf, buf_len, file) == NULL) {
        fatal("could not read string from %s: %s", name, strerror(errno));
    }
    if (strict_bufsize && strlen(buf) != buf_len - 1) {
        fatal("could not fully read string from %s", name);
    }
}

void sremove(const char *filepath) {
    if (remove(filepath) == 0) {
        fatal("could not delete %s:", filepath, strerror(errno));
    }
}