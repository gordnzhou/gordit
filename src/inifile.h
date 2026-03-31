#ifndef INI_FILE_H
#define INI_FILE_H

#include <stdlib.h>
#include <stdio.h>
#include <utils.h>

typedef struct {
    char *key;
    char *value;
} inifile_item;

DEFINE_DARRAY(inif_items_arr, inifile_item)

typedef struct {
    inif_items_arr_t *items;
    char *name; 
} inifile_section;

DEFINE_DARRAY(inif_sections_arr, inifile_section)

typedef struct {
    inif_sections_arr_t *sections; 
    size_t items_size;
} inifile;

inifile *inifile_read(const char *path);

void inifile_print(inifile *inif);

int inifile_update(inifile *inif, const char *section, const char *key, const char *value);

// @return 1 if item existed, 0 otherwise
int inifile_delete_item(inifile *inif, const char *section, const char *key);

const char *inifile_get_str(inifile *inif, const char *section, const char *key);
int  inifile_get_int(inifile *inif,  int *value, const char *section, const char *key);
int inifile_get_bool(inifile *inif,  int *value, const char *section, const char *key);

void inifile_write(inifile *inif, const char *path);

void inifile_free(inifile *inif);
#endif