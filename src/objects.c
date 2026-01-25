#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "global.h"
#include "filesystem.h"
#include "objects.h"

#define CRLF_LF_ON 1

void print_obj_raw(git_obj *obj) {
    #define GIT_OBJ_PRINT_LIMIT 500
    
    int header_size = strlen((char *)obj->data) + 1;
    int content_size = obj->size - header_size;
    int length = content_size < GIT_OBJ_PRINT_LIMIT ? content_size : GIT_OBJ_PRINT_LIMIT;
    
    printf("\nHASH: %s\n", obj->hash);
    printf("HEADER: %s\nCONTENT:\n", obj->data);
    printf("%.*s", length, obj->data + header_size);
    printf("%s", content_size > GIT_OBJ_PRINT_LIMIT ? "...\n" : "\n");
}

void print_tree(git_obj_tree *tree) {
    print_obj_raw(&tree->obj);

    for (int i = 0; i < tree->entries.size; i++) {
        git_tree_entry *entry = tree->entries.data[i];
        if (entry->type == TREE_OBJ) {
            print_tree(entry->u.tree);
        }
    }
}

void hash_data(unsigned char *data, size_t size, obj_hash *o_hash) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(data, size, hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        snprintf(*o_hash + i * 2, OBJ_HASH_SIZE, "%02x", hash[i]);
    } 
}

int is_like_binary(FILE *fptr) {
    char peek[8000];
    size_t n = fread(peek, 1, sizeof(peek), fptr);
    fseek(fptr, 0, SEEK_SET);

    for (size_t i = 0; i < n; i++)
        if (peek[i] == 0) return 1;

    return 0;
}

size_t read_bytes_norm(unsigned char *buf, size_t buf_size, FILE *fptr, size_t *read) {
    char c, next_c;
    int is_binary = is_like_binary(fptr);
    size_t i = 0;
    size_t read_and_used = 0; 

    if (is_binary || !CRLF_LF_ON) {
        *read = fs_readbytes(buf, 1, buf_size, fptr);
        return *read;
    }

    while ((c = fgetc(fptr)) != EOF && i < buf_size) {
        if (c == '\r') {
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

void create_git_obj(const unsigned char *file_contents, size_t size, const char *type, git_obj *obj) {
    char header[256];
    snprintf(header, 256, "%s %llu", type, (unsigned long long)size);
    size_t header_size = strlen(header) + 1; 

    obj->type = type;
    obj->size = header_size + size;
    obj->data = malloc(header_size + size);

    memcpy(obj->data, header, header_size);
    memcpy(obj->data + header_size, file_contents, size);

    hash_data(obj->data, obj->size, &(obj->hash));
}

git_obj_blob *create_blob(size_t filesize, FILE *fptr) {
    size_t read;
    unsigned char *buf = malloc(filesize);

    size_t norm_size = read_bytes_norm(buf, filesize, fptr, &read);
    if (read != filesize) {
        perror("read less bytes than expected");
        free(buf);
        return NULL;
    }

    git_obj_blob *blob = malloc(sizeof(*blob));
    create_git_obj(buf, norm_size, O_TYPE_BLOB, &(blob->obj));

    free(buf);
    return blob;
}

int write_compressed_data(const char *path, const unsigned char *data, size_t size) {
    FILE *fptr;
    if ((fptr = fs_fopen(path, "wb")) == NULL) {
        return -1;
    }

    uLong buf_len = compressBound(size);
    Bytef *buf = malloc(buf_len);
    if (compress(buf, &buf_len, data, size) != Z_OK) {
        fs_fclose(fptr);
        free(buf);
        return -1;
    }

    size_t written = fs_writebytes(buf, 1, buf_len, fptr);
    fs_fclose(fptr);
    free(buf);

    return (written == buf_len) ? 0 : -1;
}

// @return 0 if obj was successfully stored, -1 if unable to
int write_obj_to_disk(const obj_hash hash, const unsigned char *data, size_t size) {
    char path[PATH_MAX];
    int status = hash_to_path(hash, path);

    if (status == 1) {
        return 0;
    }
    if (status == -1 || write_compressed_data(path, data, size) != 0) {
        printf("ERROR: could not save hash: %s\n", hash);
        return -1;
    } 
    
    return 0;
}

int write_blob_to_disk(const git_obj_blob *blob) {
    return write_obj_to_disk(blob->obj.hash, blob->obj.data, blob->obj.size);
}

size_t obj_uncompressed_size(const unsigned char *raw_bytes, size_t raw_size) {
    size_t buf_size = raw_size < 256 ? raw_size : 256;
    char header_buf[buf_size];

    size_t header_size;
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (Bytef *)raw_bytes;
    strm.avail_in = raw_size;
    strm.next_out = (Bytef *)header_buf;
    strm.avail_out = buf_size;
    if (inflateInit(&strm) != Z_OK) {
        return 0;
    }

    int ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        inflateEnd(&strm);
        return 0;
    }
    inflateEnd(&strm);

    int ok = 0;
    for (header_size = 0; header_size < buf_size - strm.avail_out; header_size++) {
        if (header_buf[header_size] == '\0') {
            ok = 1;
            break;
        }
    }
    if (!ok) {
        return 0;
    }

    char *sep;
    if ((sep = strchr(header_buf, ' ')) == NULL) {
        return 0;
    }
    *sep = '\0'; sep++;

    size_t size;
    if (sscanf(sep, "%zu", &size) != 1) {
        return 0;
    }
    size += header_size + 1;

    return size;
}

unsigned char *read_raw_data(const char *path, size_t *raw_size) {
    size_t size;
    FILE *fptr;
    fs_fileinfo fileinfo;

    if (fs_getinfo(path, &fileinfo) == -1) {
        return NULL;
    }

    size = fileinfo.fi_size;

    if ((fptr = fs_fopen(path, "rb")) == NULL) {
        return NULL;
    }

    unsigned char *raw_buf = malloc(size);
    size_t read = fread(raw_buf, 1, size, fptr);
    if (read != size) {
        fs_fclose(fptr);
        free(raw_buf);
        return NULL;
    }
    fs_fclose(fptr);

    *raw_size = size;
    return raw_buf;
}

unsigned char *read_obj_from_disk(const obj_hash hash, size_t *size) {
    char path[PATH_MAX];
    int status = hash_to_path(hash, path);
    
    size_t full_size, raw_size;
    unsigned char *data, *raw_buf;

    if (status != 1 || (raw_buf = read_raw_data(path, &raw_size)) == NULL) {
        printf("ERROR: could not get hash's file: %s\n", hash);
        return NULL;
    }

    if ((full_size = obj_uncompressed_size(raw_buf, raw_size)) == 0) {
        free(raw_buf);
        printf("ERROR: could not get uncompressed size of hash: %s\n", hash);
        return NULL;
    }
    
    data = malloc(full_size);
    if (uncompress(data, (uLongf *)(&full_size), (Bytef *)raw_buf, raw_size) != Z_OK) {
        free(data);
        free(raw_buf);
        printf("ERROR: could not uncompress hash: %s\n", hash);
        return NULL;
    }
    
    *size = full_size;
    return data;
}

int is_header_type_matches(const unsigned char *data, const char *type) {
    char *end;
    if ((end = strchr((char *)data, ' ')) == NULL) {
        return 0;
    }

    *end = '\0';
    int matches = strcmp((char *)data, type) == 0;
    *end = ' ';

    return matches;
}

git_obj_blob *read_blob_from_disk(obj_hash hash) {
    git_obj_blob *blob = malloc(sizeof(*blob));
    blob->obj.type = O_TYPE_BLOB;
    snprintf(blob->obj.hash, PATH_MAX, "%s", hash);

    if ((blob->obj.data = read_obj_from_disk(hash, &(blob->obj.size))) == NULL) {
        free(blob);
        return NULL;
    } 

    if (!is_header_type_matches(blob->obj.data, O_TYPE_BLOB)) {
        printf("ERROR: cannot create blob, %s not a blob\n", hash);
        free(blob->obj.data);
        free(blob);
        return NULL;
    }

    return blob;
}

int create_file_from_blob(const char *filepath, const git_obj_blob *blob) {
    FILE *f_ptr;
    if ((f_ptr = fs_fopen(filepath, "wb")) == NULL) {
        perror("Error opening file");
        return -1;
    }
    
    unsigned char *start;
    if ((start = (unsigned char *)strchr((char *)blob->obj.data, '\0')) == NULL) {
        fs_fclose(f_ptr);
        return 1;
    }
    start++;

    size_t size = blob->obj.size - (start - blob->obj.data);
    size_t written = write_norm_bytes(start, size, f_ptr);
    fs_fclose(f_ptr);
     
    return written >= size ? 0 : 1;
}

void free_blob(git_obj_blob *blob) {
    free(blob->obj.data);
    free(blob);
}

int cmp_tree_entries(const void *p1, const void *p2) {
    const git_tree_entry *te1 = *(const git_tree_entry * const *)p1;
    const git_tree_entry *te2 = *(const git_tree_entry * const *)p2;
    return strcmp(te1->name, te2->name);
}

// 6 (mode) + 4 (type) + 40 (hash) + PATH_MAX (name) + 4 (seperators)
#define MAX_TREE_ENTRY_LINE 54 + PATH_MAX

size_t tree_entry_line(unsigned char *buf, git_tree_entry *entry) {
    git_obj obj = entry->u.tree->obj;
    int size = snprintf((char *)buf, MAX_TREE_ENTRY_LINE, "%06o %s %s %s\n", entry->git_mode, obj.type, obj.hash, entry->name);
    assert(size <= MAX_TREE_ENTRY_LINE);

    return size;
}

// NOTE: only supports files and folders. symlinks and gitlinks just return 0.
unsigned int git_mode(unsigned int st_mode) {
    if (S_ISDIR(st_mode)) {
        return 0040000;
    } 
    if (st_mode & S_IXUSR) {
        return 0100755;
    } 
    return 0100644;
}

unsigned int stat_mode(unsigned int git_mode) {
    if (git_mode == 0100644) {
        return 0644;
    }
    if (git_mode == 0100755) {
        return 0755;
    }
    return 0;
}

void init_entries_data(git_tree_list_entries *entries) {
    entries->size = 0;
    entries->data = malloc(sizeof(git_tree_entry *));
    entries->capacity = 1;
}

void free_tree_entry_and_obj(git_tree_entry *entry) {
    switch (entry->type) {
        case BLOB_OBJ:
            free_blob(entry->u.blob);
            break;
        case TREE_OBJ:
            free_tree(entry->u.tree);
    }

    free(entry);
}

void free_tree_entries_data(git_tree_list_entries *entries) {
    for (int i = 0; i < entries->size; i++) {
        free_tree_entry_and_obj(entries->data[i]);
    }
    free(entries->data);
}

int add_tree_entry(git_tree_entry *entry, git_tree_list_entries *entries) {
    if (entries->size >= entries->capacity) {
        entries->capacity *= 2;
        git_tree_entry **tmp;
        if ((tmp = realloc(entries->data, entries->capacity * sizeof(git_tree_entry *))) == NULL) {
            return -1;
        }
        entries->data = tmp;
    }

    entries->data[entries->size++] = entry;
    return 0;
}

// technically not needed; trees are made from entries in index
git_obj_tree *create_tree(const char *folderpath) {
    DIR *dir;
    fs_dirent *ent;

    if ((dir = fs_opendir(folderpath)) == NULL) {
        perror("could not open directory");
        return NULL;
    }

    int ok = 1;
    git_tree_list_entries entries;
    init_entries_data(&entries);

    while ((ent = fs_readdir(dir, folderpath)) != NULL) {
        if (strcmp(ent->de_name, ".") == 0 || strcmp(ent->de_name, "..") == 0) {
            continue;
        }

        git_tree_entry *tree_ent = malloc(sizeof(*tree_ent));
        snprintf(tree_ent->name, PATH_MAX, "%s", ent->de_name);
        tree_ent->git_mode = git_mode(ent->de_mode);

        if (ent->de_type == FS_ISFILE) {
            int file_error = 0;
            FILE *fptr;
            git_obj_blob *blob;

            if ((fptr = fs_fopen(ent->de_path, "rb")) != NULL) {
                if ((blob = create_blob(ent->de_size, fptr)) == NULL) {
                    file_error = 1;
                }
            } else {
                file_error = 1;
            }

            if (file_error) {
                fs_fclose(fptr);
                free(tree_ent);
                ok = 0;
                break;
            }
            fs_fclose(fptr);

            tree_ent->u.blob = blob;
            tree_ent->type = BLOB_OBJ;
        } else if (ent->de_type == FS_ISDIR) {
            // NOTE: CANNOT just pass ent->de_path. 
            // ent is a static struct which means ent->path will be overwritten by recursive call
            char subpath[PATH_MAX];
            snprintf(subpath, PATH_MAX, "%s", ent->de_path);
            git_obj_tree *subtree = create_tree(subpath);

            if (subtree == NULL) {
                free(tree_ent);
                continue;
            }

            tree_ent->u.tree = subtree;
            tree_ent->type = TREE_OBJ;
        }

        if (add_tree_entry(tree_ent, &entries) != 0) {
            free_tree_entry_and_obj(tree_ent);
            ok = 0;
            break;
        }
    }
    fs_closedir(dir);

    if (entries.size == 0) {
        return NULL;
    }

    if (!ok) {
        free_tree_entries_data(&entries);
        return NULL;
    }

    qsort(entries.data, entries.size, sizeof(git_tree_entry *), cmp_tree_entries);
    
    unsigned char *buf = malloc(entries.size * MAX_TREE_ENTRY_LINE);
    size_t content_size = 0;
    for (int i = 0; i < entries.size; i++) {
        size_t line_size = tree_entry_line(buf + content_size, entries.data[i]);
        content_size += line_size;
    }

    git_obj_tree *tree = malloc(sizeof(*tree));
    create_git_obj(buf, content_size, O_TYPE_TREE, &(tree->obj));
    tree->entries = entries;

    return tree;
}

int write_tree_to_disk(const git_obj_tree *tree) {
    for (int i = 0; i < tree->entries.size; i++) {
        int ok = -1;
        git_tree_entry *entry = tree->entries.data[i];
        switch (entry->type) {
            case BLOB_OBJ:
                ok = write_blob_to_disk(entry->u.blob);
                break;
            case TREE_OBJ:
                ok = write_tree_to_disk(entry->u.tree);
                break;
        }

        if (ok == -1) {
            return -1;
        }
    }

    return write_obj_to_disk(tree->obj.hash, tree->obj.data, tree->obj.size);
}

int tree_cmp(struct git_obj_tree *old, struct git_obj_tree *new, const char *path) {
    (void)old;
    (void)new;
    (void)path;
    return 0;
}

int parse_tree_entries(const unsigned char *entries_buf, size_t entries_buf_length, git_tree_list_entries *entries) {
    char *entries_copy = malloc(entries_buf_length);
    memcpy(entries_copy, entries_buf, entries_buf_length);

    char *saveptr_lines;
    char *saveptr_fields;

    init_entries_data(entries);

    int ok = 1;
    char *line = strtok_r(entries_copy, "\n", &saveptr_lines);
    while (line != NULL) {
        git_tree_entry *entry = malloc(sizeof(*entry));
        entry->git_mode = atoi(strtok_r(line, " ", &saveptr_fields));
        char *type = strtok_r(NULL, " ", &saveptr_fields);
        char *hash = strtok_r(NULL, " ", &saveptr_fields);
        snprintf(entry->name, PATH_MAX, "%s", strtok_r(NULL, " ", &saveptr_fields));

        if (strcmp(type, O_TYPE_BLOB) == 0) {
            entry->type = BLOB_OBJ;
            if ((entry->u.blob = read_blob_from_disk(hash)) == NULL) {
                free(entry);
                ok = 0;
            }

        } else if (strcmp(type, O_TYPE_TREE) == 0) {
            entry->type = TREE_OBJ;
            if ((entry->u.tree = read_tree_from_disk(hash)) == NULL) {
                free(entry);
                ok = 0;
            }

        } else {
            ok = 0; // should not ever reach this
        }
        if (!ok) {
            break;
        }

        if (add_tree_entry(entry, entries) != 0) {
            free_tree_entry_and_obj(entry);
            ok = 0;
            break;
        }

        line = strtok_r(NULL, "\n", &saveptr_lines);
    }

    if (!ok) {
        free_tree_entries_data(entries);
        free(entries_copy);
        return -1;
    }

    free(entries_copy);

    return 0;
}

git_obj_tree *read_tree_from_disk(obj_hash hash) {
    git_obj_tree *tree = malloc(sizeof(*tree));
    tree->obj.type = O_TYPE_TREE;
    snprintf(tree->obj.hash, OBJ_HASH_SIZE, "%s", hash);

    if ((tree->obj.data = read_obj_from_disk(hash, &(tree->obj.size))) == NULL) {
        free(tree);
        return NULL;
    }

    if (!is_header_type_matches(tree->obj.data, O_TYPE_TREE)) {
        printf("ERROR: cannot create tree, %s is not a tree\n", hash);
        free(tree->obj.data);
        free(tree);
        return NULL;
    }
    
    int header_size = strlen((char *)tree->obj.data) + 1;
    int entries_buf_size = tree->obj.size - header_size;

    unsigned char *entries_start = tree->obj.data + header_size;
    
    git_tree_list_entries entries;
    if (parse_tree_entries(entries_start, entries_buf_size, &entries) != 0) {
        free(tree->obj.data);
        free(tree);
        return NULL;
    }

    tree->entries = entries;
    return tree;
}

int tree_find(const git_obj_tree *tree, obj_hash hash, git_tree_entry *obj, char *path) {
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
    free_tree_entries_data(&(tree->entries));
    free(tree->obj.data);
    free(tree);
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