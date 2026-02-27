#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "repo.h"
#include "filesystem.h"
#include "objects.h"
#include "fileinfo.h"
#include "logging.h"
#include "utils.h"

// switch remove carriage return from text files during object creation
#ifdef _WIN32 
    #define CRLF_LF_ON 1
#else
    #define CRLF_LF_ON 0
#endif

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
        *read = fread(buf, 1, buf_size, fptr);
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

void copy_hash(obj_hash *out, const obj_hash *in) {
    snprintf(*out, OBJ_HASH_SIZE, "%s", *in);
}

void string_to_hash(obj_hash *out, const char *in) {
    assert(strlen(in) < sizeof(obj_hash));
    copy_hash(out, (obj_hash *)in);
}

void hash_from_bytes(const unsigned char *bytes, obj_hash *out_hash) {
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        snprintf(*out_hash + (i << 1), OBJ_HASH_SIZE, "%02x", bytes[i]);
    } 
}

void hash_to_bytes(const obj_hash hash, unsigned char *out_bytes) {
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        char hex[3] = {hash[i << 1], hash[(i << 1) + 1], '\0'};
        sscanf(hex, "%2hhx", &out_bytes[i]);
    }
}

void hash_data(unsigned char *data, size_t size, obj_hash *o_hash) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(data, size, hash);
    hash_from_bytes(hash, o_hash);
}

void create_git_obj(const unsigned char *file_contents, size_t size, enum obj_type type, git_obj *obj) {
    char header[256];
    snprintf(header, 256, "%s %llu", obj_type_string(type), (unsigned long long)size);
    size_t header_size = strlen(header) + 1; 

    obj->type = type;
    obj->size = header_size + size;
    obj->data = smalloc(header_size + size);

    memcpy(obj->data, header, header_size);
    memcpy(obj->data + header_size, file_contents, size);

    hash_data(obj->data, obj->size, &(obj->hash));
}

git_obj *create_blob_from_file(const fileinfo *finfo) {
    size_t read;
    size_t filesize = finfo->stat.fi_size;
    unsigned char *buf = smalloc(filesize);

    size_t norm_size = read_bytes_norm(buf, filesize, finfo->fptr, &read);
    if (read != filesize) {
        error("could not create blob from '%s': unable to fully read file", finfo->abs_path);
        free(buf);
        return NULL;
    }

    git_obj *blob = malloc(sizeof(*blob));
    create_git_obj(buf, norm_size, OBJ_TYPE_BLOB, blob);
    free(buf);
    return blob;
}

int write_obj_to_disk(const git_repo *repo, const git_obj *obj) {
    char path[PATH_MAX];
    if (obj_store_path(repo, obj->hash, path) == 1) {
        return 1;
    }
    FILE *fptr = sfopen(path, "wb");

    uLong buf_len = compressBound(obj->size);
    Bytef *buf = smalloc(buf_len);
    if (compress(buf, &buf_len, obj->data, obj->size) != Z_OK) {
        fatal("could not compress object: %s\n", path);
    }

    fwriteb_full(buf, buf_len, fptr, path);
    fclose(fptr);
    free(buf);
    
    return 0;
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
    fs_statinfo statinfo;

    if (fs_getinfo(path, &statinfo) == -1) {
        return NULL;
    }

    size = statinfo.fi_size;

    unsigned char *raw_buf = smalloc(size);
    
    fptr = sfopen(path, "rb");
    freadb_full(raw_buf, size, fptr, path);
    fclose(fptr);

    *raw_size = size;
    return raw_buf;
}

int check_obj_header(git_obj *obj) {
    char *end;
    if ((end = strchr((char *)obj->data, ' ')) == NULL) {
        DEBUG_PRINT("object %s is not an object!", obj->hash);
        return -1;
    }

    *end = '\0';
    if (strcmp((char *)obj->data, obj_type_string(obj->type)) != 0) {
        DEBUG_PRINT("read header of object %s to be: %s\n", obj->hash, obj->data);
        return -1;
    }
    *end = ' ';

    return 0;
}

void create_obj_from_disk(git_obj *obj, const git_repo *repo, const obj_hash hash, enum obj_type type) { 
    snprintf(obj->hash, OBJ_HASH_SIZE, "%s", hash);
    obj->type = type;

    size_t raw_size;
    unsigned char *raw_buf;

    char path[PATH_MAX];
    if (obj_store_path(repo, hash, path) != 1) {
        fatal("%s object at '%s' was not found", obj_type_string(type), path);
    }
    
    if ((raw_buf = read_raw_data(path, &raw_size)) == NULL) {
        fatal("could not read %s object at '%s'", obj_type_string(type), hash);
    }

    if ((obj->size = obj_uncompressed_size(raw_buf, raw_size)) == 0) {
        fatal("could not get uncompressed size of %s object at '%s'", obj_type_string(type), hash);
    }
    
    obj->data = smalloc(obj->size);

    if (uncompress(obj->data, (uLongf *)(&obj->size), (Bytef *)raw_buf, raw_size) != Z_OK) {
        fatal("could not uncompress %s object at '%s'", obj_type_string(type), hash);
    }

    if (check_obj_header(obj) < 0) {
        fatal("could not object '%s' as a %s\n", obj->hash, obj->type);
    }
}

char *obj_content_string(const git_obj *obj) {
    int header_size = strlen((char *)obj->data) + 1;
    int length = obj->size - header_size;

    char *content = smalloc(length + 1);
    memcpy(content, obj->data + header_size, length);
    content[length] = '\0';

    return content;
}

int create_file_from_blob(const char *filepath, const git_obj *blob) {
    FILE *fptr = sfopen(filepath, "wb");
    
    unsigned char *start;
    if ((start = (unsigned char *)strchr((char *)blob->data, '\0')) == NULL) {
        fclose(fptr);
        return -1;
    }
    start++;

    size_t size = blob->size - (start - blob->data);
    size_t written = write_norm_bytes(start, size, fptr);
    fclose(fptr);
     
    return written >= size ? 0 : -1;
}

void free_obj(git_obj *obj) {
    free(obj->data);
    free(obj);
}