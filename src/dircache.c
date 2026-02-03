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

#define INDEX_HEADER_SIG "DIRC"
#define INDEX_HEADER_SIZE 12

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
    git_index_entry *entry = malloc(sizeof(*entry));
    
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

int write_index(const git_repo *repo, git_dircache *dircache) {
    size_t buf_size = INDEX_HEADER_SIZE;
    for (int i = 0; i < dircache->num_entries; i++) {
        buf_size += 62 + dircache->entries[i]->namelen + 9;
    }

    unsigned char *buf = malloc(buf_size);

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

    FILE *fptr;
    if ((fptr = fs_fopen(repo->index_path, "wb")) == NULL) {
        free(buf);
        return -1;
    }
   
    size_t written = fs_writebytes(buf, 1, actual_size, fptr);

    fs_fclose(fptr);
    free(buf);
 
    return (written == actual_size) ? 0 : -1;
}



git_dircache *create_dircache(const git_repo * repo) {
    if (!fs_file_exists(repo->index_path)) {
        git_dircache *dircache = malloc(sizeof(*dircache));
        dircache->num_entries = 0;
        dircache->capacity = 1;
        dircache->entries = calloc(1, sizeof(git_index_entry *));
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

    unsigned char header_buf[INDEX_HEADER_SIZE];
    if (fs_readbytes(header_buf, 1, INDEX_HEADER_SIZE, fptr) != INDEX_HEADER_SIZE || 
        memcmp(header_buf, INDEX_HEADER_SIG, 4) != 0) {
        // TODO: standardize logging
        // - msg, warnings, errors, fatal/die errors, 
        // - debug only prints: logging msg, debug/assert errors
        fprintf(stderr, "ERROR: index is empty or corrupted");
        fs_fclose(fptr);
        return NULL;
    }

    unsigned char *buf_ptr = header_buf + 4;
    int version_number = read_u32_big_endian(&buf_ptr);
    if (version_number != 2) {
        printf("WARNING: index version is not 2\n");
    }
    
    git_dircache *dircache = malloc(sizeof(*dircache));
    dircache->num_entries = read_u32_big_endian(&buf_ptr);
    if (info.fi_size <= INDEX_HEADER_SIZE || dircache->num_entries == 0) {
        dircache->entries = NULL;
        fs_fclose(fptr);
        return dircache;
    }
    
    dircache->entries = calloc(dircache->num_entries, sizeof(git_index_entry *));
    dircache->capacity = dircache->num_entries;
    
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
    while (i < dircache->num_entries
        && ((dircache->entries[i++] = parse_index_entry(&entry_start)) != NULL)) {

        int abs_pos_in_buffer = entry_start - buf;
        int padding = ((abs_pos_in_buffer + 0b111) & ~0b111) - abs_pos_in_buffer;
        entry_start += padding;

        if (entry_start > buf + buf_size) {
            fs_fclose(fptr);
            free_dircache(dircache);
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

int index_sort_cmp(char *name1, char *name2) {
    size_t size1 = strlen(name1) + 1;
    size_t size2 = strlen(name2) + 1;
    
    int res = memcmp(name1, name2, size1 < size2 ? size1 : size2);
    return res == 0 ? (int)(size1 - size2) : res;
}

int add_index_entry(git_dircache *dircache, git_index_entry *entry) {
    // TODO: if entry->name is currently in merge conflict, replace its 3 entries with this one
    int first_not_smaller = 0;
    while (first_not_smaller < dircache->num_entries
        && index_sort_cmp(dircache->entries[first_not_smaller]->name, entry->name) < 0 ) {

        first_not_smaller++;
    }

    int num_same = 0;
    while (first_not_smaller + num_same < dircache->num_entries 
        && index_sort_cmp(dircache->entries[first_not_smaller + num_same]->name, entry->name) == 0) {

        num_same++;
    }
    
    if (first_not_smaller == dircache->num_entries || (first_not_smaller < dircache->num_entries && num_same == 0)) {   
        if (dircache->num_entries >= dircache->capacity) {
            dircache->capacity *= 2;
            git_index_entry **tmp;
            if ((tmp = realloc(dircache->entries, dircache->capacity * sizeof(git_index_entry *))) == NULL) {
                perror("could not realloc");
                return -1;
            }
            dircache->entries = tmp;
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

int add_file_to_dc(const git_repo *repo, git_dircache *dircache, char *filepath) {
    struct fs_fileinfo info;
    if (fs_getinfo(filepath, &info) != 0) {
        return -1;
    }

    git_index_entry *entry = malloc(sizeof(*entry));
    entry->info = info;
    entry->stage_num = 0;
    entry->git_mode = stat_mode_to_git(info.fi_mode);
    repo_rel_path(repo, filepath, entry->name);
    entry->namelen = strlen(entry->name);

    git_index_entry **found_entry = (git_index_entry **)bsearch(&entry, 
        dircache->entries, dircache->num_entries, 
        sizeof(git_index_entry *), cmp_index_entry);

    if (found_entry != NULL) {
        int same = 1;
        same &= (*found_entry)->info.fi_size == info.fi_size;
        same &= (*found_entry)->info.fi_mtime == info.fi_mtime;
        same &= (*found_entry)->info.fi_atime == info.fi_atime;
        same &= (*found_entry)->info.fi_ctime == info.fi_ctime;

        if (same) {
            return 0;
        }
    }

    git_obj_blob *blob = create_blob_from_path(filepath);
    if (blob == NULL) {
        free(entry);
        return -1;
    }

    snprintf(entry->hash, OBJ_HASH_SIZE, "%s", blob->obj.hash);
    free_blob(blob);

    if (add_index_entry(dircache, entry) != 0) {
        free(entry);
        return -1;
    }

    return 0;
}

int remove_file_from_dc(const git_repo *repo, git_dircache *dircache, char *filepath) {
    char name[PATH_MAX];
    repo_rel_path(repo, filepath, name);

    int found = -1, count_match = 0;
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];

        if (index_sort_cmp(name, entry->name) == 0) {
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

int add_tree_entry_to_dc(const git_repo *repo, git_dircache *dircache, git_tree_entry *tree_entry) {
    (void)repo;
    (void)dircache;
    (void)tree_entry;
    return 0;
}

git_obj_tree *build_tree_from_index(git_dircache *dircache) {
    git_obj_tree *tree = init_tree();

    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        git_obj_tree *parent_tree = tree;

        char dir_parts[PATH_MAX];
        if (fs_path_dirname(entry->name, dir_parts) == 1) {
            goto add_entry;
        }
        
        char *part = strtok(dir_parts, "/");
        while (part != NULL) {
            int is_new_tree = 1;

            for (int j = 0; j < parent_tree->size; j++) {
                git_tree_entry *t_entry = parent_tree->entries[j];        
                if (t_entry->type == TREE_OBJ && strcmp(part, t_entry->name) == 0) {
                    parent_tree = t_entry->u.tree;
                    is_new_tree = 0;
                    break;
                }
            }

            if (is_new_tree) {
                git_tree_entry *new_tree_entry = malloc(sizeof(*new_tree_entry));
                new_tree_entry->type = TREE_OBJ;
                new_tree_entry->git_mode = GIT_MODE_DIR;
                snprintf(new_tree_entry->name, PATH_MAX, "%s", part);

                new_tree_entry->u.tree = init_tree();
                new_tree_entry->u.tree->obj.size = 0;
                new_tree_entry->u.tree->obj.data = NULL;
                new_tree_entry->u.tree->obj.type = O_TYPE_TREE;
                
                if (add_tree_entry(new_tree_entry, parent_tree) != 0) {
                    free_tree_entry(new_tree_entry);
                    free_tree(tree);
                    return NULL;
                }
                parent_tree = new_tree_entry->u.tree;
            }

            part = strtok(NULL, "/");
        }

    add_entry:;
        git_tree_entry *b_entry = malloc(sizeof(*b_entry));
        b_entry->type = BLOB_OBJ;
        b_entry->git_mode = entry->git_mode;
        fs_path_basename(entry->name, b_entry->name);
        b_entry->u.blob = malloc(sizeof(git_obj_blob));
        b_entry->u.blob->obj.size = 0;
        b_entry->u.blob->obj.data = NULL;
        b_entry->u.blob->obj.type = O_TYPE_BLOB;
        snprintf(b_entry->u.blob->obj.hash, OBJ_HASH_SIZE, "%s", entry->hash);

        if (add_tree_entry(b_entry, parent_tree) != 0) {
            free_tree_entry(b_entry);
            free_tree(tree);
            return NULL;
        }
    }

    hash_tree_full(tree); 
        
    return tree;
}