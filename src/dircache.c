#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

#include "filesystem.h"
#include "dircache.h"
#include "logging.h"
#include "utils.h"
#include "tree.h"

#define INDEX_HEADER_SIG "DIRC"
#define INDEX_HEADER_SIZE 12

#define INDEX_STAGENUM_OK 0
#define INDEX_STAGENUM_BASE 1
#define INDEX_STAGENUM_OURS 2
#define INDEX_STAGENUM_THEIRS 3

unsigned int read_u32_big_endian(unsigned char **buf_ptr) {
    unsigned int ret = 0;
    memcpy(&ret, *buf_ptr, 4);
    *buf_ptr += 4;
    return ntohl(ret);
}

void write_u32_big_endian(unsigned char **buf_ptr, unsigned int in) {
    in = htonl(in);
    memcpy(*buf_ptr, &in, 4);
    *buf_ptr += 4;
}

void free_dircache(git_dircache *dircache) {
    for (int i = 0; i < dircache->num_entries; i++) {
        free(dircache->entries[i]);
    }
    free(dircache->entries);
    free(dircache);
}

void print_dircache(git_dircache *dircache) {
    printf("num of entries: %d\n", dircache->num_entries);
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        assert(entry != NULL);
        printf("name: %s, hash: %s size: %d\n", entry->name, entry->hash, (int)entry->info.fi_size);
    }
}

git_index_entry *parse_index_entry(unsigned char **buf_entry_start) {
    unsigned char *buf_ptr = *buf_entry_start;
    git_index_entry *entry = smalloc(sizeof(*entry));
    
    entry->info.fi_ctime = read_u32_big_endian(&buf_ptr);
    read_u32_big_endian(&buf_ptr); // ctime ns
    entry->info.fi_mtime = read_u32_big_endian(&buf_ptr);
    read_u32_big_endian(&buf_ptr); // mtime ns
    entry->info.fi_dev = read_u32_big_endian(&buf_ptr);
    entry->info.fi_ino = read_u32_big_endian(&buf_ptr);
    entry->git_mode = stat_mode_to_git(read_u32_big_endian(&buf_ptr));
    entry->unix_perm = entry->git_mode & 0x1FF;
    entry->info.fi_uid = read_u32_big_endian(&buf_ptr);
    entry->info.fi_gid= read_u32_big_endian(&buf_ptr);
    entry->info.fi_size = read_u32_big_endian(&buf_ptr);
    hash_from_bytes(buf_ptr, &(entry->hash));
    buf_ptr += 20;

    short flags = ((*buf_ptr) << 8) | (*(buf_ptr + 1));
    entry->stage_num = flags & 0x3000;
    entry->namelen = flags & 0x0FFF;
    buf_ptr += 2;

    snprintf(entry->name, entry->namelen + 1, "%s", buf_ptr);
    buf_ptr += entry->namelen + 1;

    *buf_entry_start = buf_ptr;
    return entry;
}

void write_index(const git_repo *repo, git_dircache *dircache) {
    unsigned char *buf; 

    size_t buf_size = INDEX_HEADER_SIZE;
    for (int i = 0; i < dircache->num_entries; i++) {
        buf_size += 62 + dircache->entries[i]->namelen + 9;
    }

    buf = smalloc(buf_size);
    snprintf((char *)buf, INDEX_HEADER_SIZE, "%s", INDEX_HEADER_SIG);
    unsigned char *buf_ptr = buf + sizeof(INDEX_HEADER_SIG) - 1;
    write_u32_big_endian(&buf_ptr, 2);
    write_u32_big_endian(&buf_ptr, dircache->num_entries);

    unsigned char *buf_start = buf_ptr;
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        write_u32_big_endian(&buf_ptr, entry->info.fi_ctime);
        write_u32_big_endian(&buf_ptr, 0);
        write_u32_big_endian(&buf_ptr, entry->info.fi_mtime);
        write_u32_big_endian(&buf_ptr, 0);
        write_u32_big_endian(&buf_ptr, entry->info.fi_dev);
        write_u32_big_endian(&buf_ptr, entry->info.fi_ino);
        write_u32_big_endian(&buf_ptr, entry->git_mode);
        write_u32_big_endian(&buf_ptr, entry->info.fi_uid);
        write_u32_big_endian(&buf_ptr, entry->info.fi_gid);
        write_u32_big_endian(&buf_ptr, entry->info.fi_size);
        hash_to_bytes(entry->hash, buf_ptr);
        buf_ptr += 20;

        short flags = ((entry->stage_num & 0b11) << 12) | (entry->namelen & 0xFFF);
        *buf_ptr = (flags & 0xFF00) >> 8;
        *(buf_ptr + 1) = flags & 0xFF;
        buf_ptr += 2;
        
        snprintf((char *)buf_ptr, entry->namelen + 1, "%s", entry->name);
        buf_ptr += entry->namelen + 1;       

        int abs_pos_in_buffer = buf_ptr - buf_start;
        int padding = ((abs_pos_in_buffer + 0b111) & ~0b111) - abs_pos_in_buffer;
        buf_ptr += padding;
    }

    size_t actual_size = buf_ptr - buf;
    assert(actual_size <= buf_size);

    FILE *fptr = sfopen(repo->index_path, "wb");
    fwriteb_full(buf, actual_size, fptr, repo->index_path);
    fclose(fptr);
    free(buf);
}

git_dircache *create_dircache(const git_repo * repo) {
    git_dircache *dircache = smalloc(sizeof(*dircache));
    dircache->num_entries = 0;
    dircache->capacity = 1;
    dircache->entries = scalloc(1, sizeof(git_index_entry *));

    if (!fs_file_exists(repo->index_path)) {
        return dircache;
    }

    fs_statinfo info;
    if (fs_getinfo(repo->index_path, &info) < 0) {
        fatal("could not open index file");
    }
    FILE *fptr = sfopen(repo->index_path, "rb");

    unsigned char header_buf[INDEX_HEADER_SIZE];
    if (fread(header_buf, 1, INDEX_HEADER_SIZE, fptr) != INDEX_HEADER_SIZE || 
        memcmp(header_buf, INDEX_HEADER_SIG, 4) != 0) {
        fclose(fptr);
        fatal("index is corrupted: header is invalid");
    }

    unsigned char *buf_ptr = header_buf + 4;
    int version_number = read_u32_big_endian(&buf_ptr);
    if (version_number != 2) {
        warn("index version is not 2");
    }
    
    int num_entries = read_u32_big_endian(&buf_ptr);
    if (info.fi_size <= INDEX_HEADER_SIZE || num_entries == 0) {
        fclose(fptr);
        return dircache;
    }
    
    dircache->num_entries = num_entries;
    dircache->capacity = dircache->num_entries;
    dircache->entries = srealloc(dircache->entries, dircache->num_entries * sizeof(git_index_entry *));
    
    size_t buf_size = info.fi_size - INDEX_HEADER_SIZE; 
    unsigned char *buf = smalloc(buf_size);

    freadb_full(buf, buf_size, fptr, repo->index_path);

    unsigned char *entry_start = buf;
    int i = 0;
    while (i < dircache->num_entries
        && ((dircache->entries[i++] = parse_index_entry(&entry_start)) != NULL)) {

        int abs_pos_in_buffer = entry_start - buf;
        int padding = ((abs_pos_in_buffer + 0b111) & ~0b111) - abs_pos_in_buffer;
        entry_start += padding;
        
        if (entry_start > buf + buf_size)  {
            fatal("index is corrupted: could not parse entries");
        }
    }

    fclose(fptr);
    free(buf);
    return dircache;
}

int cmp_index_entry(const void *a, const void *b) {
    const git_index_entry *ea = *(const git_index_entry **)a;
    const git_index_entry *eb = *(const git_index_entry **)b;
    return strcmp(ea->name, eb->name);
}

int index_sort_cmp(const char *name1, const char *name2) {
    size_t size1 = strlen(name1) + 1;
    size_t size2 = strlen(name2) + 1;
    
    int res = memcmp(name1, name2, size1 < size2 ? size1 : size2);
    return res == 0 ? (int)(size1 - size2) : res;
}

git_index_entry **find_dircache_entry(const git_dircache *dircache, const git_index_entry *entry) {
    return (git_index_entry **)bsearch(
        &entry, 
        dircache->entries, dircache->num_entries, 
        sizeof(git_index_entry *), 
        cmp_index_entry);
}

git_index_entry **dircache_find_file(const git_dircache *dircache, const char *name) {
    git_index_entry entry = { 0 };
    snprintf(entry.name, PATH_MAX, "%s", name);
    return find_dircache_entry(dircache, &entry);
}


int is_stat_same(const fs_statinfo *s1, const fs_statinfo *s2) {
    int same = 1;
    same &= s1->fi_size == s2->fi_size;
    same &= s1->fi_mtime == s2->fi_mtime;
    same &= s1->fi_ctime == s2->fi_ctime;
    return same; 
}

int add_file_to_dc(git_dircache *dircache, const fileinfo *finfo, git_obj **blob) {
    git_index_entry *entry = smalloc(sizeof(*entry));
    entry->info = finfo->stat;
    entry->stage_num = INDEX_STAGENUM_OK;
    entry->git_mode = stat_mode_to_git(finfo->stat.fi_mode);
    snprintf(entry->name, PATH_MAX, "%s", finfo->norm_path);
    entry->namelen = strlen(entry->name);

    *blob = NULL;

    git_index_entry **found = find_dircache_entry(dircache, entry);
    if (found) {
        if (is_stat_same(&(entry->info), &((*found)->info))) {
            DEBUG_PRINT("skipping file: %s", finfo->abs_path);
            return 0;
        }
    }

    *blob = create_blob_from_file(finfo);
    if (*blob == NULL) {
        free(entry);
        return -1;
    }

    copy_hash(&(entry->hash), &((*blob)->hash));

    int first_not_smaller = 0 ;
    while (first_not_smaller < dircache->num_entries
        && index_sort_cmp(dircache->entries[first_not_smaller]->name, entry->name) < 0 ) {
        first_not_smaller++;
    }

    int num_same = 0;
    while (first_not_smaller + num_same < dircache->num_entries 
        && index_sort_cmp(dircache->entries[first_not_smaller + num_same]->name, entry->name) == 0) {
        num_same++;
    }
    
    // not removing any case
    if (first_not_smaller == dircache->num_entries || (first_not_smaller < dircache->num_entries && num_same == 0)) {   
        if (dircache->num_entries >= dircache->capacity) {
            dircache->capacity *= 2;
            dircache->entries = srealloc(dircache->entries, dircache->capacity * sizeof(git_index_entry *));
        }

        if (first_not_smaller == dircache->num_entries) {
            dircache->entries[dircache->num_entries++] = entry;
            return 0;
        }
    }

    for (int i = 0; i < num_same; i++) {
        free(dircache->entries[first_not_smaller + i]);
        dircache->entries[first_not_smaller + i] = NULL;
    }

    if (num_same != 1) {
        memmove(dircache->entries + first_not_smaller + 1, 
            dircache->entries + first_not_smaller + num_same, 
            sizeof(git_index_entry *) * (dircache->num_entries - first_not_smaller - num_same));
    }
    
    dircache->entries[first_not_smaller] = entry;
    dircache->num_entries += 1 - num_same;

    return 0;
}

int remove_file_from_dc(git_dircache *dircache, const fileinfo *finfo) {
    int found = -1, count_match = 0;
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];

        if (index_sort_cmp(finfo->norm_path, entry->name) == 0) {
            if (found == -1) {
                found = i;
            }
            count_match += 1;

            free(entry);
            dircache->entries[i] = NULL;
        }
    }

    if (found >= 0) {
        memmove(dircache->entries + found, 
            dircache->entries + found + count_match, 
            sizeof(git_index_entry *) * (dircache->num_entries - found - count_match));
    }

    dircache->num_entries -= count_match;

    return found >= 0 ? 0 : -1;
}

git_obj_tree *build_tree_from_index(git_dircache *dircache) {
    git_obj_tree *root = init_tree();

    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        git_obj_tree *parent_tree = root;

        char dir_parts[PATH_MAX];
        if (fs_path_dirname(entry->name, dir_parts) == 1) {
            goto add_blob;
        }

        char *part = strtok(dir_parts, "/");
        while (part != NULL) {
            if (parent_tree->size > 0) {
                // works assuming index is sorted by name
                git_tree_entry *tail_entry = parent_tree->entries[parent_tree->size-1];
                if (tail_entry->type == OBJ_TYPE_TREE && strcmp(part, tail_entry->name) == 0) {
                    parent_tree = tail_entry->u.tree;
                    part = strtok(NULL, "/");
                    continue;
                }
            }

            git_tree_entry *new_tree_entry = smalloc(sizeof(*new_tree_entry));
            new_tree_entry->type = OBJ_TYPE_TREE;
            new_tree_entry->git_mode = GIT_MODE_DIR;
            snprintf(new_tree_entry->name, PATH_MAX, "%s", part);
            new_tree_entry->u.tree = init_tree(); 
            
            add_tree_entry(new_tree_entry, parent_tree);
            parent_tree = new_tree_entry->u.tree;

            part = strtok(NULL, "/");
        }

    add_blob:;
        git_tree_entry *b_entry = smalloc(sizeof(*b_entry));
        b_entry->type = OBJ_TYPE_BLOB;
        b_entry->git_mode = entry->git_mode;
        snprintf(b_entry->name, PATH_MAX, "%s", entry->name);
        copy_hash(&(b_entry->u.blob_hash), &(entry->hash));
        add_tree_entry(b_entry, parent_tree);
    }

    hash_tree_full(root); 
        
    return root;
}

int dircache_has_conflicts(git_dircache *dircache) {
    for (int i = 0; i < dircache->num_entries; i++) {
        if (dircache->entries[i]->stage_num != INDEX_STAGENUM_OK) {
            return 1;
        }
    }
    return 0;
}