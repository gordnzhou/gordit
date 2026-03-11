#ifndef TEXTDIFF_H
#define TEXTDIFF_H

#include "objects.h"

enum linediff_op {
    KEEP = 0,
    DELETE,
    INSERT
};

typedef struct {
    int lcount;
    char **lines;
} git_textfile;

typedef struct {
    git_textfile *text;
    size_t line;
    enum linediff_op op;
    int show;
} git_linediff;

typedef struct {
    int count;
    git_linediff *diff_lines;
} git_textdiff;

git_textfile *textfile_from_file(fileinfo *finfo);
git_textfile *textfile_from_blob(git_obj *blob);
void textfile_free(git_textfile *text);

void textdiff_myers_print(git_textfile *new, const char *new_name, git_textfile *old, const char *old_name);

#endif