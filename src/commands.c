#include <string.h>

#include "commit.h"
#include "diff.h"
#include "dircache.h"
#include "filesystem.h"
#include "fileinfo.h"
#include "logging.h"
#include "status.h"
#include "refs.h"
#include "pathspec.h"
#include "utils.h"
#include "args.h"

int do_commit(const git_repo *repo, 
    git_dircache *dircache, 
    char *author_name, 
    char *author_email, 
    const char *msg
) {
    if (dircache_has_conflicts(dircache)) {
        return error("there are unmerged files in index, cannot commit");
    }

    obj_hash head_commit_hash;
    int num_parents = 1;
    int is_detached;
    char *head_content = read_head(repo, &is_detached);
    char *head_ref_path = NULL;
    if (is_detached) {
        warn("head is not pointing to any branch, commit will be easily lost!");
        string_to_hash(&head_commit_hash, head_content);
    } else {
        head_ref_path = head_content;
        if (read_ref(repo, head_ref_path, &head_commit_hash) < 0) {
            num_parents = 0;
        }
    }

    // TODO: read MERGE_HEAD for possibly 1+ other commit parents
    
    if (num_parents == 1) {
        git_obj_commit *parent_commit = create_commit_from_disk(repo, head_commit_hash);
        git_obj_tree *parent_tree = read_tree_from_disk(repo, parent_commit->tree_hash);
        if (is_tree_and_dc_same(dircache, parent_tree)) {
            free(head_content);
            free_tree(parent_tree);
            free_commit(parent_commit);
            return info("nothing new for me to commit!");
        }

        free_tree(parent_tree);
        free_commit(parent_commit);
    }

    git_obj_tree *tree = build_tree_from_index(dircache);
    if (tree->size == 0) {
        warn("commiting an empty tree..");
    } 

    git_obj_commit *commit = create_commit(tree, num_parents, &head_commit_hash,
        author_name, author_email, msg);
    git_obj *commit_obj = create_commit_obj(commit);
    obj_hash *commit_hash = &(commit_obj->hash);

    write_obj_to_disk(repo, commit_obj);
    write_tree_to_disk(repo, tree, 1);
    print_tree(tree);
    print_commit(commit);
    printf("COMMIT: %s\n", *commit_hash);

    if (is_detached) {
        detach_head(repo, commit_hash);
    } else {
        write_ref(repo, head_ref_path, commit_hash);
    }

    free_obj(commit_obj);
    free(commit);
    free_tree(tree);
    free(head_content);
    
    return 0;
}

// ASSUME files are valid 
void do_command_each_file(const git_repo *repo, 
    git_dircache *dircache, 
    char *mode, 
    char **files, int num_files, 
    void (*cmd_cb)(const git_repo *, git_dircache *, const struct fileinfo *)) {
    
        for (int i = 0; i < num_files; i++) {
            struct fileinfo *info;
            if ((info = start_fileinfo(repo, files[i], mode)) == NULL) {
                error("could not open file: %s", files[i]);
                continue;
            }

            (*cmd_cb)(repo, dircache, info);
            
            end_fileinfo(info);
        }
}

void do_add(const git_repo *repo, git_dircache *dircache, const struct fileinfo *info) {
    if (is_file_ignored(repo, info)) {
        return;
    }

    git_obj *blob;
    if (add_file_to_dc(dircache, info, &blob) != 0) {
        error("could not add file: %s", info->norm_path);
        return;
    }

    if (blob) {
        write_obj_to_disk(repo, blob);
    }
}

void do_remove(const git_repo *repo, git_dircache *dircache, const struct fileinfo *info) {
    (void)repo;
    if (remove_file_from_dc(dircache, info) != 0) {
       DEBUG_PRINT("removing %s from index but it is not in dircache", info->abs_path); 
    }
}

int run_init(arg_list *args) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    if (create_repo_folder(cwd) == 1) {
        printf("Existing repository detected, reinitialized\n");
    } else {
        printf("Initalized empty repository in %s\n", cwd);
    }

    (void)args;
    return 0;
}

int run_add(arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));
    
    const git_repo *repo = get_working_repo(cwd);
    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    filter_ignores(result, repo);
    git_dircache *dircache = create_dircache(repo);
    do_command_each_file(repo, dircache, "rb", result->norm_paths, result->size, do_add);
    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return 0;
}

int run_rm(arg_list *args) {
    if (args->cmd_args_count <= 0) {
        return info("No files specified, nothing added.");
    }
    
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    pathspec_result *result = init_pathspec_result();
    for (int i = 0; i < args->cmd_args_count; i++) {
        expand_arg(result, repo, args->cmd_args[i]);
    }
    git_dircache *dircache = create_dircache(repo);
    do_command_each_file(repo, dircache, "rb", result->norm_paths, result->size, do_remove);
    write_index(repo, dircache);

    free_dircache(dircache);
    free_pathspec_result(result);
    free((void *)repo);

    return 0;
}

int run_commit(arg_list *args) {
    if (args->message == NULL) {
        return error("No commit message");
    }

    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    git_dircache *dircache = create_dircache(repo);
    do_commit(repo, dircache, "Walter White", "wwhite@hotmail.com", args->message);
    free_dircache(dircache);
    free((void *)repo);

    return 0;
}

int run_status(arg_list *args) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    const git_repo *repo = get_working_repo(cwd);
    print_repo_status(repo);

    free((void *)repo);

    (void)args;
    return 0;
}