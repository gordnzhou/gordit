#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

void *smalloc(size_t size);
void *scalloc(size_t num_elements, size_t element_size);
void *srealloc(void *ptr, size_t new_size);

FILE *sfopen(const char *filepath, const char *mode);
char *sgetcwd(char *pathbuf, size_t pathsize);
DIR  *sopendir(const char *folderpath);

// reads all bytes from file or crash
// @param name name of file for logging.
void freadb_full(void *dest, size_t filesize, FILE *file, const char *name);

// write all bytes to file or crash
// @param name name of file for logging.
void fwriteb_full(void *src, size_t filesize, FILE *file, const char *name);

// @param name name of file for logging.
void sfputs(const char *str, FILE *file, const char *name);

// @param name name of file for logging.
// @param struct_bufsize if not exactly `buf_len` bytes were read and this is set, program will crash
// @return NULL if end of file is reached
char *sfgets(char *buf, size_t buf_len, FILE *file, const char *name, int strict_bufsize);

void sremove(const char *filepath);

char *sstrdup(const char *src);
char *sstrndup(const char *src, size_t size);

int is_path_in_folder(const char *abs_folder_path, const char *abs_path);

// compares two strings ignoring cases 
int strcicmp(const char *, const char *);

// Quick way to get dynamic array at the cost of complexity from macros
// and obscuring the type (int* is a array?)
// USE FOR SIMPLE CASES WHERE DATA IS NOT BEING PASSED AROUND FUNCTIONS
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

#define DA_COPY(dst, src) do {                                               \
    if (src != NULL) {                                                       \
    size_t _len = DA_LEN(src);                                               \
    da_header *_h = smalloc(sizeof(da_header) + _len * sizeof(*(src)));      \
    _h->len      = _len;                                                     \
    _h->capacity = _len;                                                     \
    memcpy(_h + 1, (src), _len * sizeof(*(src)));                            \
    (dst) = (void *)(_h + 1);                                                \
    }                                                                        \
} while (0)

typedef struct {
    size_t len;
    size_t capacity;
    char **data;
} strarr_t;

strarr_t *strarr_new();
void     strarr_push(strarr_t *arr, const char *str);
void     strarr_free(strarr_t *arr);

#define DEFINE_DARRAY(name, type)                                        \
typedef struct {                                                         \
    size_t len;                                                          \
    size_t capacity;                                                     \
    type *data;                                                          \
} name##_t;                                                              \
                                                                         \
static inline name##_t *name##_new() {                                   \
    name##_t *arr = smalloc(sizeof(*arr));                               \
    arr->len = 0;                                                        \
    arr->capacity = DA_CAP_INIT;                                         \
    arr->data = smalloc(arr->capacity * sizeof(type));                   \
    return arr;                                                          \
}                                                                        \
                                                                         \
static inline name##_t *name##_copy(const name##_t *src) {               \
    name##_t *dst = smalloc(sizeof(*dst));                               \
    dst->len      = src->len;                                            \
    dst->capacity = src->capacity;                                       \
    dst->data     = smalloc(src->capacity * sizeof(type));               \
    memcpy(dst->data, src->data, src->len * sizeof(type));               \
    return dst;                                                          \
}                                                                        \
                                                                         \
static inline void name##_push(name##_t *arr, type val) {                \
    if (arr->len >= arr->capacity) {                                     \
        arr->capacity *= DA_SCALE_FACTOR;                                \
        arr->data = srealloc(arr->data, arr->capacity * sizeof(type));   \
    }                                                                    \
    arr->data[arr->len++] = val;                                         \
}                                                                        \
                                                                         \
static inline void name##_free(name##_t *arr,                            \
    void (*element_free_func)(type)) {                                   \
    for (size_t i = 0; i < arr->len; i++) {                              \
        element_free_func(arr->data[i]);                                 \
    }                                                                    \
    free(arr->data);                                                     \
    free(arr);                                                           \
}

#endif
