#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#include "filesystem.h"
#include "inifile.h"
#include "utils.h"
#include "logging.h"

#define LINE_BUF_SIZE 4096

char *dup_clean_str(const char *src, size_t max_size, int make_lower) {

    size_t size = strlen(src);
    size = max_size < size ? max_size : size;

    const char *start = src, *end = src + size - 1;

    while (start < end && isspace(*start)) { start++; }
    while (end > start && isspace(*end)) { end--; }

    if (start > end) {
        fatal("ini file is invalid");
    }

    size_t new_size = end - start + 1;
    char *out = smalloc(new_size + 1);
    memcpy(out, start, new_size);
    out[new_size] = '\0';
    if (make_lower) {
        for (size_t i = 0; i < new_size; i++) {
            out[i] = tolower(out[i]);
        }
    }

    return out;
}

inifile *inifile_read(const char *path) {
    inifile *inif = smalloc(sizeof(*inif));
    inif->sections = inif_sections_arr_new();
    inif->items_size = 0;

    FILE *fptr = fopen(path, "r");
    if (fptr == NULL) {
        return inif;
    }
    
    char line[LINE_BUF_SIZE];
    while (sfgets(line, LINE_BUF_SIZE, fptr, path, 0)) {
        int line_size = strlen(line);
        if (!line_size) {
            continue;
        }
        
        if (line[0] == '[') {
            const char *end = strchr(line + 1, ']');
            if (!end) {
                fatal("Invalid ini file");
            }
            
            inif_sections_arr_push(inif->sections, (inifile_section){
                .name = dup_clean_str(line + 1, end-line-1, 1),
                .items = inif_items_arr_new()
            });
        }
        else if (line[0] != ';' && line[0] != '#' && line[0] != '[') {
            const char *sep = strchr(line + 1, '=');
            if (!sep || !inif->sections->len) {
                continue;
            }

            inifile_section *section = inif->sections->data + inif->sections->len - 1;
            inif_items_arr_push(section->items, (inifile_item){
                .key = dup_clean_str(line, sep - line - 1, 1),
                .value = dup_clean_str(sep + 1, line_size, 0)
            });
            inif->items_size++;
        }

    }

    fclose(fptr);

    return inif;
}

// check if section exists
// if section DNE: checks if size is max and appends section to inif->sections
// if key DNE: checksi if size is max and appends to inif->items
int inifile_update(inifile *inif, const char *section, const char *key, const char *value) {
    inifile_section *sec = NULL;
    inifile_item *item = NULL;

    for (size_t i = 0; i < inif->sections->len; i++) {
        inifile_section *sec_cur = inif->sections->data + i;
        if (strcicmp(section, sec_cur->name) == 0) {
            sec = sec_cur;
            break;
        }
    }

    if (!sec) {
        inif_sections_arr_push(inif->sections, (inifile_section){
            .name = dup_clean_str(section, LINE_BUF_SIZE, 1),
            .items = inif_items_arr_new()
        });
        sec = inif->sections->data + inif->sections->len - 1;
    }

    for (size_t j = 0; j < sec->items->len; j++) {
        inifile_item *item_cur = sec->items->data + j;
        if (strcicmp(key, item_cur->key) == 0) {
            item = item_cur;
            break;
        }
    }

    if (!item) {
        inif_items_arr_push(sec->items, (inifile_item){
            .key = dup_clean_str(key, LINE_BUF_SIZE, 1),
            .value = dup_clean_str(value, LINE_BUF_SIZE, 0)
        });
    } else {
        free(item->value);
        item->value = dup_clean_str(value, LINE_BUF_SIZE, 0);
    }

    return 0;
}

int inifile_delete_item(inifile *inif, const char *section, const char *key) {
    inifile_section *sec = NULL;
    inifile_item *item = NULL;

    for (size_t i = 0; i < inif->sections->len; i++) {
        inifile_section *sec_cur = inif->sections->data + i;
        if (strcicmp(section, sec_cur->name) == 0) {
            sec = sec_cur;
            break;
        }
    }
    if (!sec) {
        return 0;
    }

    for (size_t j = 0; j < sec->items->len; j++) {
        inifile_item *item_cur = sec->items->data + j;
        if (strcicmp(key, item_cur->key) == 0) {
            item = item_cur;
            break;
        }
    }
    if (!item) {
        return 0;
    }

    item->value[0] = '\0';
    return 1;
}

const char *inifile_get_str(inifile *inif, const char *section, const char *key) {
    for (size_t i = 0; i < inif->sections->len; i++) {
        inifile_section *sec = inif->sections->data + i;
        if (strcicmp(sec->name, section)) {
            continue;
        }

        for (size_t j = 0; j < sec->items->len; j++) {
            inifile_item *item = sec->items->data + j;
            if (strcicmp(item->key, key)) {
                continue;
            }

            return item->value;  
        }
    }

    return NULL;
}

int inifile_get_int(inifile *inif, int *value, const char *section, const char *key) {
    const char *val = inifile_get_str(inif, section, key);
    if (val == NULL) {
        return -1;
    }

    *value = atoi(val);

    return 0;
}

int inifile_get_bool(inifile *inif, int *value, const char *section, const char *key) {
    const char *val = inifile_get_str(inif, section, key);
    if (val == NULL) {
        return -1;
    }

    *value = strcmp(val, "true") == 0 || 
             strcmp(val, "1")    == 0 || 
             strcmp(val, "yes")  == 0 || 
             strcmp(val, "on")   == 0;

    return 0;
}

void inifile_print(inifile *inif) {
    for (size_t i = 0; i < inif->sections->len; i++) {
        inifile_section *section = inif->sections->data + i;
        if (!section->items->len) {
            continue;
        }

        printf("[%s]\n", section->name);
        for (size_t j = 0; j < section->items->len; j++) {
            inifile_item *item = section->items->data + j;
            printf("    %s = %s\n", item->key, item->value);
        }
    }
}

void inifile_write(inifile *inif, const char *path) {
    FILE *fptr = sfopen(path, "w");

    char line_buf[LINE_BUF_SIZE];
    for (size_t i = 0; i < inif->sections->len; i++) {
        inifile_section *section = inif->sections->data + i;
        if (!section->items->len) {
            continue;
        }

        snprintf(line_buf, LINE_BUF_SIZE, "[%s]\n", section->name);
        sfputs(line_buf, fptr, path);

        for (size_t j = 0; j < section->items->len; j++) {
            inifile_item *item = section->items->data + j;
            if (!strlen(item->value)) {
                continue;
            }

            snprintf(line_buf, LINE_BUF_SIZE, "    %s = %s\n", item->key, item->value);
            sfputs(line_buf, fptr, path);
        }
    }

    fclose(fptr);

}

void free_item(inifile_item item) {
    free(item.key);
    free(item.value);
}

void free_section(inifile_section section) {
    inif_items_arr_free(section.items, free_item);
    free(section.name);
}

void inifile_free(inifile *inif) {
    inif_sections_arr_free(inif->sections, free_section);
    free(inif);
}