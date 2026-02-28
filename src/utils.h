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

typedef struct {
    size_t len;
    size_t capacity;
} da_header;

#define DA_CAP_INIT 16
#define DA_SCALE_FACTOR 2

#define DA_PUSH(arr, x) \
    do {                                                                      \
        if (arr == NULL) {                                                    \
            da_header *_h = smalloc(sizeof(*_h) + DA_CAP_INIT * sizeof(*arr));\
            _h->len = 0;                                                      \
            _h->capacity = DA_CAP_INIT;                                       \
            arr = (void *)(_h + 1);                                           \
        }                                                                     \
        da_header *_h = (da_header *)(arr) - 1;                               \
        if (_h->len >= _h->capacity) {                                        \
            _h->capacity *= DA_SCALE_FACTOR;                                  \
            _h = srealloc(_h, sizeof(*_h) + _h->capacity * sizeof(*arr));     \
            arr = (void *)(_h + 1);                                           \
        }                                                                     \
        (arr)[_h->len++] = (x);                                               \
    } while (0)

#define DA_LEN(arr) ((arr) ? ((da_header *)(arr) - 1)->len : 0)

#define DA_FREE(arr) do {                                   \
    if (arr) free((da_header *)(arr) - 1);                  \
    (arr) = NULL;                                           \
} while (0)

#define DA_FREE_DEEP(arr, free_func) do {        \
    for (size_t _i = 0; _i < DA_LEN(arr); _i++)  \
        (free_func)((arr)[_i]);                  \
    DA_FREE(arr);                                \
} while (0)

// TODO: handle NULL case
#define DA_COPY(dst, src) do {                                               \
    size_t _len = DA_LEN(src);                                               \
    da_header *_h = smalloc(sizeof(da_header) + _len * sizeof(*(src)));      \
    _h->len      = _len;                                                     \
    _h->capacity = _len;                                                     \
    memcpy(_h + 1, (src), _len * sizeof(*(src)));                            \
    (dst) = (void *)(_h + 1);                                                \
} while (0)

#endif