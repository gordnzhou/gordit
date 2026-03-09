#ifndef FILEINFO_H
#define FILEINFO_H

#include "repo.h"

typedef struct fileinfo {
    char norm_path[PATH_MAX]; 
    char abs_path[PATH_MAX]; 
    fs_statinfo stat;
    FILE *fptr;
} fileinfo;

// will crash if unable to get file info
// @param norm_path should be paths returned by a pathspec
struct fileinfo *start_fileinfo(const git_repo *repo, const char *norm_path, const char *mode);

// closes file stream
void end_fileinfo(struct fileinfo *info);


#endif