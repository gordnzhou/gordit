#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>

void *smalloc(size_t size);
void *scalloc(size_t num_elements, size_t element_size);
void *srealloc(void *ptr, size_t new_size);

FILE *sfopen(const char *filepath, const char *mode);
char *sgetcwd(char *pathbuf, size_t pathsize);

// reads all bytes from file or crash
void freadb_full(void *dest, size_t filesize, FILE *file, const char *name);

// write all bytes to file or crash
void fwriteb_full(void *src, size_t filesize, FILE *file, const char *name);

void sfputs(const char *str, FILE *file, const char *name);
void sfgets(char *buf, size_t buf_len, FILE *file, const char *name, int strict_bufsize);
void sremove(const char *filepath);

char *sstrdup(const char *src);
char *sstrndup(const char *src, size_t size);
#endif