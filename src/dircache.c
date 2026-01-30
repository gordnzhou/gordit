#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa.inet.h>
#endif

#include "filesystem.h"
#include "dircache.h"

#define INDEX_HEADER_SIG "DIRC"
#define INDEX_HEADER_SIZE 12

unsigned int read_u32_big_endian(const void *buf) {
    unsigned int ret = 0;
    memcpy(&ret, buf, 4);
    return ntohl(ret);
}

void write_u32_big_endian(void *buf, unsigned int in) {
    in = htonl(in);
    memcpy(buf, &in, 4);
}

git_index_entry *parse_index_entry(unsigned char **index_start, unsigned char *buf_start) {
    unsigned char *pos = *index_start;
    git_index_entry *entry = malloc(sizeof(*entry));
    entry->info = malloc(sizeof(*(entry->info)));
    entry->hash = malloc(sizeof(*(entry->hash)));
    
    entry->info->fi_ctime = read_u32_big_endian(pos);
    pos += 8;
    entry->info->fi_mtime = read_u32_big_endian(pos);
    pos += 8;
    entry->info->fi_dev = read_u32_big_endian(pos);
    pos += 4;
    entry->info->fi_ino = read_u32_big_endian(pos);
    pos += 4;
    entry->git_mode = stat_mode_to_git(read_u32_big_endian(pos));
    entry->unix_perm = entry->git_mode & 0x1FF;
    pos += 4;
    entry->info->fi_uid = read_u32_big_endian(pos);
    pos += 4;
    entry->info->fi_gid= read_u32_big_endian(pos);
    pos += 4;
    entry->info->fi_size = read_u32_big_endian(pos);
    pos += 4;

    for (int i = 0; i < 20; i++, pos++) {
        snprintf(*(entry->hash) + i * 2, OBJ_HASH_SIZE, "%02x", *pos);
    }

    short flags = ((*pos) << 8) | (*(pos + 1));
    entry->stage_num = flags & 0x3000;
    entry->namelen = flags & 0x0FFF;
    entry->name = malloc(entry->namelen + 1);
    pos += 2;
    
    snprintf(entry->name, entry->namelen + 1, "%s", pos);
    pos += entry->namelen + 1;

    int actual_pos = pos - buf_start;
    int padding = ((actual_pos + 7) & ~7) - actual_pos;

    *index_start = pos + padding;
    return entry;
}

void free_index_entry(git_index_entry *entry) {
        free(entry->hash);
        free(entry->name);
        free(entry->info);
        free(entry);
}

git_dircache *create_dircache(const git_repo * repo) {
    if (!fs_file_exists(repo->index_path)) {
        git_dircache *dircache = malloc(sizeof(*dircache));
        dircache->num_entries = 0;
        dircache->entries = NULL;
        return dircache;
    }

    FILE *fptr;
    fs_fileinfo info;
    
    if (fs_getinfo(repo->index_path, &info) != 0) {
        return NULL;
    }
    if ((fptr = fs_fopen(repo->index_path, "rb")) == NULL) {
        return NULL;
    }

    char header_buf[INDEX_HEADER_SIZE];
    if (fs_readbytes(header_buf, 1, INDEX_HEADER_SIZE, fptr) != INDEX_HEADER_SIZE || 
        memcmp(header_buf, INDEX_HEADER_SIG, 4) != 0) {
        // TODO: standardize logging
        // - msg, warnings, errors, fatal/die errors, 
        // - debug only prints: logging msg, debug/assert errors
        fprintf(stderr, "ERROR: index is empty or corrupted");
        fs_fclose(fptr);
        return NULL;
    }

    int version_number = read_u32_big_endian(header_buf + 4);
    if (version_number != 2) {
        printf("WARNING: index version is not 2");
    }
    
    git_dircache *dircache = malloc(sizeof(*dircache));
    dircache->num_entries = read_u32_big_endian(header_buf + 8);
    if (info.fi_size <= INDEX_HEADER_SIZE || dircache->num_entries == 0) {
        dircache->entries = NULL;
        fs_fclose(fptr);
        return dircache;
    }
 
    dircache->entries = calloc(dircache->num_entries, sizeof(git_index_entry *));
    
    size_t buf_size = info.fi_size - INDEX_HEADER_SIZE; 
    unsigned char *buf = malloc(buf_size);
    if (fs_readbytes(buf, 1, buf_size, fptr) != buf_size) {
        fs_fclose(fptr);
        free(buf);
        free(dircache->entries);
        free(dircache);
        return NULL;
    }

    unsigned char *entry_start = buf;
    int i = 0;
    while (((dircache->entries[i++] = parse_index_entry(&entry_start, buf)) != NULL) && i < dircache->num_entries) {
        if (entry_start > buf + buf_size) {
            fs_fclose(fptr);
            free(buf);
            for (int j = 0; j < i; j++) {
                free_index_entry(dircache->entries[j]);
            }
            free(dircache->entries);
            free(dircache);
            return NULL;
        }
    }

    fs_fclose(fptr);
    free(buf);
    return dircache;
}

int cmp_index_entry(const void *a, const void *b) {
    const git_index_entry *ea = *(const git_index_entry **)a;
    const git_index_entry *eb = *(const git_index_entry **)b;
    return strcmp(ea->name, eb->name);
}

int add_index_entry(git_dircache *dircache, git_index_entry *entry) {
    (void)dircache;
    (void)entry;
    return 0;
}

int add_file_to_index(const git_repo *repo, git_dircache *dircache, char *filepath) {
    FILE *fptr;
    if ((fptr = fs_fopen(repo->index_path, "wb")) == NULL) {
        return -1;
    }
 
    if (!fs_file_exists(repo->index_path)) {
        static const char header_buf[INDEX_HEADER_SIZE] = INDEX_HEADER_SIG "\x00\x00\x02\x00\x00\x00\x00";
        fs_writebytes(header_buf, 1, INDEX_HEADER_SIZE, fptr);
    }
    
    struct fs_fileinfo info;
    if (fs_getinfo(filepath, &info) != 0) {
        return -1;
    }

    git_index_entry entry;
    entry.info = &info;
    entry.stage_num = 0;
    entry.git_mode = stat_mode_to_git(info.fi_mode);
    entry.namelen = strlen(filepath);
    entry.name = malloc(entry.namelen + 1);
    snprintf(entry.name, PATH_MAX, "%s", filepath);

    git_index_entry **found_entry = (git_index_entry **)bsearch(&entry, 
        dircache->entries, dircache->num_entries, 
        sizeof(git_index_entry *), cmp_index_entry);

    if (found_entry != NULL) {
        int same = 1;
        same &= (*found_entry)->info->fi_size == info.fi_size;
        same &= (*found_entry)->info->fi_mtime == info.fi_mtime;
        same &= (*found_entry)->info->fi_atime == info.fi_atime;
        same &= (*found_entry)->info->fi_ctime == info.fi_ctime;

        if (same) {
            return 0;
        }
    }

    git_obj_blob *blob = create_blob_from_path(filepath);
    if (blob == NULL) {
        return -1;
    }

    snprintf(blob->obj.hash, OBJ_HASH_SIZE, "%s", *(entry.hash));

    int ret = add_index_entry(dircache, &entry);

    free(entry.name);

    return ret;
}

int add_tree_entry_to_index(const git_repo *repo, git_dircache *dircache, git_tree_entry *tree_entry) {
    (void)repo;
    (void)dircache;
    (void)tree_entry;
    return 0;
}

int remove_file_from_index(const git_repo *repo, git_dircache *dircache, char *filepath) {
    (void)repo;
    (void)dircache;
    (void)filepath;
    return 0;
}

void free_dircache(git_dircache *dircache) {
    for (int i = 0; i < dircache->num_entries; i++) {
        free_index_entry(dircache->entries[i]);
    }
    free(dircache->entries);
    free(dircache);
}