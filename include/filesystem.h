#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
    
#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

#define fs_fopen fopen
#define fs_fclose fclose
#define fs_rename rename
#define fs_remove remove

#define fs_readbytes fread
#define fs_writebytes fwrite
#define fs_readline fgets
#define fs_writeline fputs

#define fs_getcwd getcwd
#define fs_closedir closedir

typedef struct fs_fileinfo {
    size_t fi_size;
    unsigned int fi_mode;
    time_t fi_atime;
    time_t fi_mtime;
    time_t fi_ctime;
    dev_t fi_dev;
    ino_t fi_ino;
    unsigned int fi_uid;
    unsigned int fi_gid;
} fs_fileinfo;

#define FS_ISFILE 1
#define FS_ISDIR 0

typedef struct fs_dirent {
    char de_name[PATH_MAX];
    char de_path[PATH_MAX];
    long de_ino;
    int de_type; // `FS_FILE` or `FS_ISDIR`
} fs_dirent;

// Wrapper around `readdir` that has more guaranteed fields.
fs_dirent *fs_readdir(DIR *);

// Same as `mkdir` in POSIX. Note: mode is ignored on Win32.
// @return 0 on success, -1 if any errors
int fs_mkdir(const char *, mode_t);

// Same as `opendir` in POSIX, except path is const
DIR * fs_opendir(const char *);

// Same as `stat()` function in POSIX 
// @return 0 on success, -1 if any errors
int fs_getinfo(const char *path, struct fs_fileinfo *fileinfo);

// @brief Gets path in absolute form.  
// @return 0 on success, -1 if any errors.
int fs_path_abs(const char *path, char *out);

// @brief Gets directory component of path, removing any trailing slashes.
// @return 0 on success, 1 if path has no directory component
int fs_path_dirname(const char* path, char* out);

// @brief Gets final component of path (name of rightmost file or folder).
void fs_path_basename(const char* path, char* out);

// @brief Joins two paths together.
// @param path1 a folder path (rel or abs)
// @param path2 a file OR folder path (rel or abs)
void fs_path_join(const char *path1, const char *path2, char *out);

// @return 1 if path exists, 0 otherwise
int fs_file_exists(const char *);

#endif