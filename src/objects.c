#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "global.h"
#include "filesystem.h"
#include "objects.h"

#define CRLF_LF_ON 1

size_t read_bytes_norm(char *buf, size_t buf_size, FILE *fptr, size_t *read) {
    char c, next_c;
    size_t i = 0, read_and_used = 0; 
    while ((c = fgetc(fptr)) != EOF && read_and_used < buf_size) {
        int skip = 0;
        if (CRLF_LF_ON && c == '\r') {
            next_c = fgetc(fptr);
            if (next_c == EOF) {
                *read = i + 1;
                return read_and_used;
            }
            i += 2;
            if (next_c == '\n') {
                buf[read_and_used++] = '\n'; 
            } else {
                buf[read_and_used++] = '\r';
                buf[read_and_used++] = '\n';
            }
        } else {
            i++;
            buf[read_and_used++] = c;
        }
    }

    *read = i;
    return read_and_used;
}

// ASSUME fptr points to an empty file
size_t write_norm_bytes(unsigned char *buf, size_t buf_size, FILE *fptr) {
    size_t i = 0;
    char c;
    while (i < buf_size) {
        c = buf[i++];
        if (CRLF_LF_ON && c == '\n') {
            fputc('\r', fptr);
        }
        fputc(c, fptr);
    }

    return i;
}

git_obj_blob *create_blob(const char *filepath) {
    fs_fileinfo fileinfo;
    FILE *fptr;

    if (fs_getinfo(filepath, &fileinfo) == -1) {
        return NULL;
    }

    if ((fptr = fs_fopen(filepath, "rb")) == NULL) {
        return NULL;
    }

    size_t file_size = fileinfo.fi_size;
    size_t read;
    char *buf = malloc(file_size);
    size_t norm_size = read_bytes_norm(buf, file_size, fptr, &read);
    if (read != file_size) {
        free(buf);
        fclose(fptr);
        return NULL;
    }
    fclose(fptr);

    char header[256];
    snprintf(header, 256, "%s %llu", O_TYPE_BLOB, norm_size);
    size_t header_size = strlen(header) + 1; 

    git_obj_blob *blob = malloc(sizeof(*blob));
    blob->type = O_TYPE_BLOB;
    blob->size = header_size + norm_size;
    blob->data = malloc(header_size + norm_size);

    memcpy(blob->data, header, header_size);
    memcpy(blob->data + header_size, buf, norm_size);
    free(buf);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(blob->data, blob->size, hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(blob->hash + i * 2, "%02x", hash[i]);
    } 
    blob->hash[SHA_DIGEST_LENGTH*2] = '\0';

    return blob;
}

int write_blob_to_disk(const git_obj_blob *blob) {
    char path[PATH_MAX], parent_path[PATH_MAX];
    FILE *f_ptr;
    hash_to_path(blob->hash, path);
    fs_path_dirname(path, parent_path);

    if (fs_file_exists(path)) {
        return 0;
    }

    if (fs_mkdir(parent_path, 0700) == -1 && errno != EEXIST) {
        perror("Could not make directory");
        return -1;
    }

    if ((f_ptr = fs_fopen(path, "wb")) == NULL) {
        perror("Error opening file");
        return -1;
    }
    
    uLong buf_len = compressBound(blob->size);
    Bytef *buf = malloc(buf_len);

    if (compress(buf, &buf_len, blob->data, blob->size) != Z_OK) {
        free(buf);
        return -1;
    }

    size_t written = fs_writebytes(buf, 1, buf_len, f_ptr);
    fs_fclose(f_ptr);
    free(buf);

    return (written == buf_len) ? 0 : -1;
}

git_obj_blob *read_blob_from_disk(obj_hash hash) {
    char path[PATH_MAX], *raw_buf;
    size_t raw_size;
    FILE *fptr;
    fs_fileinfo fileinfo;

    hash_to_path(hash, path);
    if (fs_getinfo(path, &fileinfo) == -1) {
        return NULL;
    }
    raw_size = fileinfo.fi_size;

    if ((fptr = fs_fopen(path, "rb")) == NULL) {
        printf("Error opening file");
        return NULL;
    }

    raw_buf = malloc(raw_size);
    size_t read = fread(raw_buf, 1, raw_size, fptr);
    if (read != raw_size) {
        perror("read failed");
        fs_fclose(fptr);
        free(raw_buf);
        return NULL;
    }
    fs_fclose(fptr);

    char header_buf[256];
    size_t header_size;
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (Bytef *)raw_buf;
    strm.avail_in = raw_size;
    strm.next_out = (Bytef *)header_buf;
    strm.avail_out = 256;
    if (inflateInit(&strm) != Z_OK) {
        free(raw_buf);
        return NULL;
    }

    int ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        inflateEnd(&strm);
        free(raw_buf);
        return NULL;
    }
    inflateEnd(&strm);

    int ok = 0;
    for (header_size = 0;header_size < 256 - strm.avail_out; header_size++) {
        if (header_buf[header_size] == '\0') {
            ok = 1;
            break;
        }
    }
    if (!ok) {
        free(raw_buf);
        return NULL;
    }

    char *sep;
    if ((sep = strchr(header_buf, ' ')) == NULL) {
        free(raw_buf);
        return NULL;
    }
    *sep = '\0'; sep++;

    size_t size;
    if (sscanf(sep, "%zu", &size) != 1 || strcmp(header_buf, O_TYPE_BLOB) != 0) {
        free(raw_buf);
        return NULL;
    }
    size += header_size + 1;

    git_obj_blob *blob = malloc(sizeof(*blob));
    blob->data = malloc(size);
    blob->size = size;
    blob->type = O_TYPE_BLOB;
    strcpy(blob->hash, hash);

    if (uncompress(blob->data, (uLongf *)(&size), (Bytef *)raw_buf, raw_size) != Z_OK) {
        free(raw_buf);
        free(blob->data);
        free(blob);
        return NULL;
    }

    free(raw_buf);
    return blob;
}

int create_file_from_blob(const char *filepath, const git_obj_blob *blob) {
    FILE *f_ptr;
    if ((f_ptr = fs_fopen(filepath, "wb")) == NULL) {
        perror("Error opening file");
        return -1;
    }
    
    char *start;
    if ((start = strchr((char *)blob->data, '\0')) == NULL) {
        fs_fclose(f_ptr);
        return 1;
    }
    start++;

    size_t size = blob->size - (start - (char *)blob->data);
    size_t written = write_norm_bytes(start, size, f_ptr);
    fs_fclose(f_ptr);
     
    return written >= size ? 0 : 1;
}

void free_blob(git_obj_blob *blob) {
    free(blob->data);
    free(blob);
}

int cmp_tree_entries(const void *p1, const void *p2) {
    git_obj_tree_entry *te1 = (git_obj_tree_entry *)p1;
    git_obj_tree_entry *te2 = (git_obj_tree_entry *)p2;
    return strcmp(te1->name, te2->name);
}

git_obj_tree *create_tree(const char *folderpath) {
    return NULL;

    DIR *dir;
    fs_dirent *ent;

    if ((dir = fs_opendir(folderpath)) == NULL) {
        return NULL;
    }

    git_obj_tree *tree = malloc(sizeof(*tree));
    tree->type = O_TYPE_TREE;
    tree->num_entries = 0;

    int capacity = 1;
    tree->entries = malloc(sizeof(git_obj_tree_entry *));

    while ((ent = fs_readdir(dir)) != NULL) {
        if (strcmp(ent->de_name, ".") == 0 || strcmp(ent->de_name, "..") == 0) {
            continue;
        }

        git_obj_tree_entry *tree_ent = malloc(sizeof(*tree_ent));
        strcpy(tree_ent->name, ent->de_name);
        tree_ent->mode = 0;
        if (ent->de_type == FS_ISFILE) {
            git_obj_blob *blob = create_blob(ent->de_path);
            tree_ent->blob = blob;

        } else if (ent->de_type == FS_ISDIR) {
            printf("Dir: %s", ent->de_name);
            git_obj_tree *subtree = create_tree(ent->de_path);

            if (subtree != NULL) {
                tree_ent->tree = subtree;
            } else {
                free(tree_ent);
                continue;
            }
        }

        if (tree->num_entries + 1 > capacity) {
            capacity *= 2;
            if ((realloc(tree->entries, capacity * sizeof(git_obj_tree_entry *))) == NULL) {
                free(tree->entries);
                free(tree);
                fs_closedir(dir);
                return NULL;
            }
        }

        tree->entries[tree->num_entries] = tree_ent;
        tree->num_entries++;
    }
    closedir(dir);

    if (tree->num_entries == 0) {
        free(tree->entries);
        free(tree);
        return NULL;
    }

    if ((tree->entries = realloc(tree->entries, tree->num_entries * sizeof(git_obj_tree_entry *))) == NULL) {
        free(tree->entries);
        free(tree);
        return NULL;
    }

    qsort(tree->entries, tree->num_entries, sizeof(git_obj_tree_entry *), cmp_tree_entries);

    size_t file_size = 0;
    for (int i = 0; i < tree->num_entries; i++) {
        git_obj_tree_entry *entry = tree->entries[i];
    }
    
    char header[256];
    snprintf(header, 256, "%s %llu", O_TYPE_COMMIT, file_size);
    size_t header_size = strlen(header) + 1; 


    // create actual file "commit <size>\0<entry 1><entry 2>..." and set to tree->data
    // generate hash from tree->data

    return tree;
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