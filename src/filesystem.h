#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h> 

#ifdef _WIN32 
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif

typedef struct fs_statinfo {
    size_t fi_size;
    unsigned int fi_mode;
    time_t fi_atime;
    time_t fi_mtime;
    time_t fi_ctime;
    dev_t fi_dev;
    ino_t fi_ino;
    unsigned int fi_uid;
    unsigned int fi_gid;
} fs_statinfo;

#define FS_ISFILE 1
#define FS_ISDIR 0

typedef struct fs_dirent {
    long de_ino;
    size_t de_size;
    unsigned int de_mode;
    int de_type; // `FS_FILE` or `FS_ISDIR`
    char de_path[PATH_MAX];
    char de_name[PATH_MAX];
} fs_dirent;

// Wrapper around `readdir` that has more guaranteed fields.
// Skips "." and ".." entries
// @return 1 on success, 0 if no more entries, -1 if error
int fs_readdir(DIR *dir, fs_dirent *out, const char *foldername);

// Same as `mkdir` in POSIX. Note: mode is ignored on Win32.
// @return 1 if folder already exists, 0 on success, otherwise -1
int fs_mkdir(const char *, mode_t);

// Same as `opendir` in POSIX, except path is const
DIR * fs_opendir(const char *);

int fs_closedir(DIR *);

void fs_clean_path(char *path);

// Same as `stat()` function in POSIX 
// @return 0 on success, otherwise -1
int fs_getinfo(const char *path, struct fs_statinfo *statinfo);

// @brief Gets path in absolute form.  
// @return 0 on success, otherwise -1.
int fs_path_abs(const char *path, char *out);

// @brief Gets directory component of path, removing any trailing slashes.
// @return 1 if path has no slashes, 0 otherwise
int fs_path_dirname(const char* path, char* out);

// @brief Gets final component of path (name of rightmost file or folder).
void fs_path_basename(const char* path, char* out);

const char *fs_path_pbasename(const char *path);

// @brief Joins two paths together.
// @param path1 a folder path (rel or abs)
// @param path2 a file OR folder path (rel or abs)
void fs_path_join(const char *path1, const char *path2, char *out);

// @return 1 if path exists, 0 otherwise
int fs_file_exists(const char *);

#endif