#ifndef FILESPEC_H
#define FILESPEC_H

#include "repo.h"

typedef struct fileinfo {
    char name[PATH_MAX]; // relative to repo root
    char path[PATH_MAX]; // absolute path, guaranteed to be inside of repo
    fs_statinfo stat;
    FILE *fptr;
} fileinfo;


int is_file_args_valid(const git_repo *repo, char **args, int num_args);

// assume path is a valid file inside repo root
struct fileinfo *start_fileinfo(const git_repo *repo, const char *path, const char *mode);

// closes file stream
void end_fileinfo(struct fileinfo *info);

int is_file_ignored(const git_repo *repo, struct fileinfo *info);

#endif