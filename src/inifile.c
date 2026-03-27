#include <string.h>

#include "filesystem.h"
#include "inifile.h"
#include "utils.h"

#define BUF_SIZE 4096

static char buf[BUF_SIZE];

int inifile_write(const char *path, const char *section, const char *key, const char *value) {
    FILE *fptr = sfopen(path, "r");
    sfgets(buf, BUF_SIZE, fptr, path, 0);

    return 0;
}

int inifile_read_str(char *value, const char *path, const char *section, const char *key) {
    return 0;
}

int inifile_read_int(int *value, const char *path, const char *section, const char *key) {
    return 0;
}

