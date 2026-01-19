#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>

#include "global.h"
#include "filesystem.h"
#include "objects.h"

git_obj_blob *create_blob(const char *filepath) {
    fs_fileinfo fileinfo;
    FILE *fptr;

    if (fs_getinfo(filepath, &fileinfo) == -1) {
        return NULL;
    }

    if ((fptr = fopen(filepath, "rb")) == NULL) {
        return NULL;
    }

    char header[256];
    snprintf(header, 256, "%s %llu", O_TYPE_BLOB, fileinfo.fi_size);
    size_t header_len = strlen(header);

    git_obj_blob *blob = malloc(sizeof(*blob));
    blob->type = O_TYPE_BLOB;
    blob->mode = fileinfo.fi_mode;
    blob->size = fileinfo.fi_size;
    blob->name = malloc(PATH_MAX );
    blob->data = malloc(header_len + fileinfo.fi_size + 1);

    fs_path_basename(filepath, blob->name);

    strcpy(blob->data, header);

    // start reading data at next byte AFTER null terminator of header 
    size_t read = fs_readbytes(blob->data + header_len + 1, 1, blob->size, fptr);
    if (read != blob->size) {
        free(blob->data);
        free(blob->name);
        free(blob);
        fclose(fptr);
        return NULL;
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(blob->data, header_len + blob->size + 1, hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(blob->hash + i * 2, "%02x", hash[i]);
    } 

    printf("HASH: %s\n", blob->hash);

    return blob;
}

int write_blob_to_disk(const git_obj_blob *blob) {
    (void)blob;

    return 0;
}

git_obj_blob *read_blob_from_disk(obj_hash hash) {
    (void)hash;
    return NULL;
}

int create_file_from_blob(const char *path, const git_obj_blob *blob) {
    (void)path;
    (void)blob;
    return 0;
}

void free_blob(git_obj_blob *blob) {
    free(blob->data);
    free(blob->name);
    free(blob);
}

git_obj_tree *create_tree(const char *folderpath) {
    (void)folderpath;
    return NULL;
}

int write_tree_to_disk(const git_obj_tree *tree) {
    (void)tree;
    return 0;
}

git_obj_tree *read_tree_from_disk(obj_hash hash) {
    (void)hash;
    return NULL;
}

int tree_find(const git_obj_tree *tree, obj_hash hash, git_obj_tree_entry *obj, char *path) {
    (void)tree;
    (void)hash;
    (void)obj;
    (void)path;
    return 0;
}

git_obj_blob **tree_get_blobs(git_obj_tree *tree, char *regex_filter, int *size) {
    (void)tree;
    (void)regex_filter;
    (void)size;
    return NULL;
}

void free_tree(git_obj_tree *tree) {
    (void)tree;
}

int delete_obj_from_disk(obj_hash hash) {
    (void)hash;
    return 0;
}

int get_obj_contents(obj_hash hash, char *buf, int buf_size) {
    (void)hash;
    (void)buf;
    (void)buf_size;
    return 0;
}