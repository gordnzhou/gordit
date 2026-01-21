#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "filesystem.h"
    
#ifdef _WIN32
    #include <windows.h>
#endif

int fs_mkdir(const char *path, mode_t mode) {
#ifdef _WIN32
    return mkdir(path);
    (void)mode;
#else
    return mkdir(path, mode);
#endif
}

DIR *fs_opendir(const char *path) {
    char copy[PATH_MAX];
    strcpy(copy, path);

#ifdef _WIN32
    char temp[2] = {'\\', '\0'};
    strncat(copy, temp, 1);
#endif

    return opendir(copy); 
}

fs_dirent *fs_readdir(DIR *dir) {
    static fs_dirent ret;
    static struct stat st;

    struct dirent *ent;
    if ((ent = readdir(dir)) == NULL) {
        return NULL;
    }

    char path[PATH_MAX], temp[PATH_MAX];
    assert(fs_path_dirname(dir->dd_name, temp) == 0);
    fs_path_join(temp, ent->d_name, path);

    if (stat(path, &st) != 0) {
        return NULL;
    }

    ret.de_ino = ent->d_ino;
    strncpy(ret.de_path, path, PATH_MAX);
    strncpy(ret.de_name, ent->d_name, PATH_MAX);
    ret.de_type = S_ISDIR(st.st_mode) ? FS_ISDIR : FS_ISFILE;

    return &ret;
}

int fs_getinfo(const char *path, struct fs_fileinfo *fileinfo) {
    struct stat st;
    int result = stat(path, &st);

    fileinfo->fi_atime = st.st_atime;
    fileinfo->fi_mtime = st.st_mtime;
    fileinfo->fi_ctime = st.st_ctime;
    fileinfo->fi_dev = st.st_dev;
    fileinfo->fi_gid = st.st_gid;
    fileinfo->fi_ino = st.st_ino;
    fileinfo->fi_mode = st.st_mode;
    fileinfo->fi_size = st.st_size;
    fileinfo->fi_uid = st.st_uid;

    return result;
}

int _rem_trailing_slashes(char *c) {
    if (strlen(c) == 0) return 1;
    char *p = c + strlen(c) - 1;

    while (p >= c && (*p == '/' || *p == '\\'))
        p--;
    
    if (p < c) return 1; 

    *(p + 1) = '\0';
    return 0; 
}

#ifdef _WIN32

int fs_path_abs(const char *path, char *out) {    
    if (!path || !out) return -1;

    DWORD len = GetFullPathName(path, PATH_MAX, out, NULL);
    return (len > 0 && len < PATH_MAX) ? 0 : -1;
}

int fs_path_dirname(const char* path, char *out) {
    strcpy(out, path);
    // path is empty or all slashes 
    if (_rem_trailing_slashes(out) == 1) {
        out[1] = '\0';
        return 1;
    }

    char *lfs = strrchr(out, '/');
    char *lbs = strrchr(out, '\\');
    char *ls = (lfs > lbs) ? lfs : lbs;

    // path has no slashes (other than trailing slashes)
    if (ls == NULL) {
        out[0] = '.';
        out[1] = '\0';
        return 1;
    }

    *ls = '\0';

    // 'directory component' is all slashes
    if (strspn(out, "/\\") == strlen(out)) {
        out[0] = '.';
        out[1] = '\0';
        return 1;
    }

    return 0;
}

void fs_path_basename(const char* path, char *out) {
    strcpy(out, path);
    // path is empty or all slashes 
    if (_rem_trailing_slashes(out) == 1) {
        out[1] = '\0';
        return;
    }
    
    const char *start = path;
    char *lfs = strrchr(out, '/');
    char *lbs = strrchr(out, '\\');
    char *ls = (lfs > lbs) ? lfs : lbs;

    if (ls != NULL) {
        start = ls + 1;
        strcpy(out, start);
    }

    return;
}

void fs_path_join(const char *path1, const char *path2, char *out) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    assert(len1 + len2 + 2 <= PATH_MAX);

    strncpy(out, path1, PATH_MAX);
    out[PATH_MAX - 1] = '\0';

    if (len1 > 0 && out[len1 - 1] != '\\' && out[len1 - 1] != '/') 
        strcat(out, "\\");

    const char *p2 = path2;
    if (*p2 == '\\' || *p2 == '/') p2++;

    strncat(out, p2, PATH_MAX - strlen(out) - 1);
}

#else

int fs_path_abs(const char *path, char *out) {
    if (!path || !out) return -1;

    return realpath(path, out) == NULL ? -1 : 0;
}

int fs_path_dirname(const char* path, char *out) {
    char *path_copy = strdup(path);
    char *res = dirname(path_copy);
    strcpy(out, res);
    free(path_copy);

    return path[0] == '\0' || strcmp(path, "/") == 0;
}

void fs_path_basename(const char* path, char *out) {
    char *path_copy = strdup(path);
    char *res = basename(path_copy);
    strcpy(out, res);
    free(path_copy);

    return;
}

void fs_path_join(const char *path1, const char *path2, char *out) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    assert(len1 + len2 + 2 > PATH_MAX);

    strcpy(out, path1);
    if (len1 > 0 && out[len1 - 1] != '/') strcat(out, "/");
    strcat(out, path2);
}
#endif

int fs_file_exists(const char *filename) {
    FILE *file;
    if ((file = fopen(filename, "r")) != NULL) {
        fclose(file);
        return 1;
    } else {
        return 0;
    }
}