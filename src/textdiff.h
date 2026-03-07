#ifndef TEXTDIFF_H
#define TEXTDIFF_H

#include "objects.h"

typedef struct {
    size_t lcount;
    char **lines;
} git_textfile;

typedef struct {
    git_textfile *text;
    int line;
    int op; // 0 for remove, 1 for add
} git_linediff;

typedef struct {
    int dlcount;
    git_linediff *diff_lines;
} git_textdiff;

git_textfile *textfile_new(fileinfo *finfo);
// git_textfile *textfile_from_blob(git_obj *blob);
void textfile_free(git_textfile *text);

git_textdiff *textfile_diff_myers(git_textfile *a, git_textfile *b);

void textdiff_print(git_textdiff *diff);

#endif