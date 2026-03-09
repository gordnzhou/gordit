#include <string.h>

#include "textdiff.h"
#include "utils.h"
#include "logging.h"

void textfile_free(git_textfile *text) {
    for (int i = 0; i < text->lcount; i++) {
        free(text->lines[i]);
    }
    free(text->lines);
    free(text);
}

void textdiff_free(git_textdiff *diff) {
    free(diff->diff_lines);
    free(diff);
}

void split_text(git_textfile *text, char *file, size_t file_len) {
    strarr_t *lines = strarr_new();
    char *buf = smalloc(file_len + 1);
    memcpy(buf, file, file_len);
    buf[file_len] = '\0';
    
    char *token = strtok(buf, "\n");
    while (token != NULL) {
        strarr_push(lines, token);
        token = strtok(NULL, "\n");
    }

    text->lcount = lines->len;
    text->lines = smalloc(text->lcount*sizeof(char *));
    for (int i = 0; i < text->lcount; i++) {
        text->lines[i] = sstrdup(lines->data[i]);
    }

    strarr_free(lines);
    free(buf);
}

git_textfile *textfile_from_file(fileinfo *finfo) {
    git_obj *blob = create_blob_from_file(finfo);
    git_textfile *text = textfile_from_blob(blob);
    free_obj(blob);
    return text;
}

git_textfile *textfile_from_blob(git_obj *blob) {
    git_textfile *text = smalloc(sizeof(*text));

    unsigned char *text_start;
    if ((text_start = (unsigned char *)strchr((char *)blob->data, '\0')) == NULL) {
        fatal("invalid blob data");
    }
    text_start++;
    size_t size = blob->size - (text_start - blob->data);

    split_text(text, (char *)text_start, size);

    return text;
}

git_textdiff *text_diff_myers(git_textfile *new, git_textfile *old) {
    int max = new->lcount + old->lcount;
    int v_len = 2 * max + 2;
    int *v = scalloc(v_len, sizeof(*v));

    // move_x[d * v_len + k + max] = x at start of snake on diagonal k in round d
    int *move_x = smalloc((max + 1)*v_len*sizeof(*move_x));

    int found_d = 0;
    for (int d = 0; d < max; d++) {
        int *row = move_x + d * v_len;

        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && v[k - 1 + max] < v[k + 1 + max])) {
                x = v[k + 1 + max];
            } else {
                x = v[k - 1 + max] + 1;
            }
            
            row[k + max] = x;
            int y = x - k;
            while (x < new->lcount && y < old->lcount && strcmp(new->lines[x], old->lines[y]) == 0) {
                x++; y++;
            }

            v[k + max] = x;

            if (x >= new->lcount && y >= old->lcount) { 
                found_d = d;
                goto forward_done;
            }
        }
    }
forward_done:;
    free(v);

    int max_ops = new->lcount + old->lcount + (new->lcount < old->lcount ? new->lcount : old->lcount);
    git_textdiff *diff = smalloc(sizeof(*diff)); 
    diff->count = 0;
    diff->diff_lines = smalloc((size_t)max_ops * sizeof(git_linediff));

    int x = new->lcount, y = old->lcount;
    for (int d = found_d; d > 0; d--) {
        int k = x - y;
        int *row      = move_x + (size_t)d     * v_len;
        int *row_prev = move_x + (size_t)(d-1) * v_len;

        int xs = row[k + max];
        int ys = xs  - k;
        for (int i = x-1; i >= xs; i--) {
            diff->diff_lines[diff->count++] = (git_linediff){ .text = new, .line = i, .op = KEEP, .show = 0 };
        }

        if ((k == -d) || (k != d && row_prev[k + 1 + max] >= row_prev[k - 1 + max])) {
            diff->diff_lines[diff->count++] = (git_linediff){ .text = old, .line = ys-1, .op = INSERT, .show = 0 };
            x = xs, y = ys - 1;
        } else {
            diff->diff_lines[diff->count++] = (git_linediff){ .text = new, .line = xs-1, .op = DELETE, .show = 0 };
            x = xs - 1, y = ys;
        }
    }
    for (int i = x - 1; i >= 0; i--) {
        diff->diff_lines[diff->count++] = (git_linediff){ .text = new, .line = i, .op = KEEP, .show = 0 };
    }

    free(move_x);
    return diff;
}

void textdiff_myers_print(git_textfile *a, const char *a_name, git_textfile *b, const char *b_name) {
    git_textdiff *diff = text_diff_myers(a, b);

    for (int i = 0; i < diff->count; i++) {
        if (diff->diff_lines[i].op == KEEP) {
            continue;
        }

        for (int j = -3; j <= 3; j++) {
            int k = i + j;
            if (k < 0 || k >= diff->count) {
                continue;
            }
            diff->diff_lines[k].show = 1;
        }
    }

    printf("--- old/%s\n+++ new/%s\n\n", a_name, b_name);
    for (int i = diff->count - 1; i >= 0; i--) {
        git_linediff *linediff = diff->diff_lines + i;
        if (!linediff->show) {
            continue;
        }

        char op = linediff->op == KEEP ? ' ' : linediff->op == INSERT ? '+' : '-';
        const char *line = linediff->text->lines[linediff->line];
        printf("%c %s\n", op, line); 

        if (i > 0 && (diff->diff_lines[i - 1].show == 0)) {
            printf("\n\n");
        }
    }
    textdiff_free(diff);
}

