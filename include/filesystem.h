#ifndef GIT_FILE
#define GIT_FILE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
    
#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

#define u32 unsigned int
#define u16 unsigned short

#define fs_fopen fopen
#define fs_fclose fclose
#define fs_rename rename
#define fs_remove remove

#define fs_readbytes fread
#define fs_writebytes fwrite
#define fs_readline fgets
#define fs_writeline fputs

#define fs_getcwd getcwd
#define fs_mkdir mkdir
#define fs_readdir readdir
#define fs_closedir closedir

struct fs_fileinfo {
    long long fi_size;
    u32 fi_mode;
    time_t fi_atime;
    time_t fi_mtime;
    time_t fi_ctime;
    dev_t fi_dev;
    ino_t fi_ino;
    u32 fi_uid;
    u32 fi_gid;
};

DIR * fs_opendir(char *);

int fs_getinfo(const char *path, struct fs_fileinfo *fileinfo);

// @brief Gets path in absolute form.  
// @return 0 on success, -1 if any errors.
int fs_path_abs(const char *path, char *out);

// @brief Gets directory component of path, removing any trailing slashes.
// @return 0 on success, 1 no directory component, -1 if any errors.
int fs_path_dirname(const char* path, char* out);

// @brief Gets final component of path (name of rightmost file or folder).
// @return 0 on success, -1 if any errors.
int fs_path_basename(const char* path, char* out);

// @brief Joins two paths together.
// @param path1 a folder path (rel or abs)
// @param path2 a file OR folder path (rel or abs)
// @return 0 on success, -1 if any errors.
int fs_path_join(const char *path1, const char *path2, char *out);

#endif