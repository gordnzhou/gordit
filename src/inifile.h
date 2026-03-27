#ifndef INI_FILE_H
#define INI_FILE_H

#include <stdlib.h>
#include <stdio.h>

#if 0
typedef struct {
    const char *section;
    const char *key;
    const char *value;
} inifile_kvpair;

typedef struct {
    FILE *fptr;
    inifile_kvpair *kvpairs;
    size_t lines;
} inifile;

inifile *inifile_open(const char *path);

int inifile_parse(inifile *inifile);

int inifile_write(, const char *key, const char *value);

int inifile_read_str(char *value, const char *path, const char *section, const char *key);

int inifile_read_int(int *value, const char *path, const char *section, const char *key);


void inifile_close(inifile *);
#endif

#endif