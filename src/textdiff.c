#include <string.h>

#include "textdiff.h"
#include "utils.h"
#include "logging.h"

void split_text(git_textfile *text, char *buf, size_t buf_len) {
    strarr_t *lines = strarr_new();
    char *prev_end = buf;
    for (char *cur = buf; cur < buf + buf_len; cur++) {
        if (*cur == '\n') {
            char *line_start = prev_end; 
            size_t line_len = strlen(line_start);
            *cur = '\0';
            if (line_len > 0 && line_len == (size_t)(cur - line_start)) {
                strarr_push(lines, line_start);
            }
            *cur = '\n';
            prev_end = cur + 1;
        }
    }
    if (prev_end < buf + buf_len) {
        size_t left = (buf + buf_len) - prev_end; 
        char *temp = smalloc(left + 1);
        memcpy(temp, prev_end, left);
        temp[left] = '\0';
        size_t line_len = strlen(temp);
        if (line_len > 0 && line_len == left) {
            strarr_push(lines, temp);
        }
        free(temp);
    }

    text->lcount = lines->len;
    text->lines = smalloc(text->lcount*sizeof(char *));
    for (size_t i = 0; i < text->lcount; i++) {
        text->lines[i] = sstrdup(lines->data[i]);
    }

    strarr_free(lines);
}

git_textfile *textfile_new(fileinfo *finfo) {
    git_textfile *text = smalloc(sizeof(*text));

    (void)finfo;
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

void textfile_free(git_textfile *text) {
    for (size_t i = 0; i < text->lcount; i++) {
        free(text->lines[i]);
    }
    free(text->lines);
    free(text);
}