#include <string.h>

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
#include "textdiff.h"
#include "commit.h"
#include "commands.h"
#include "colour.h"
#include "inifile.h"

int run_init(const arglist_init_t *arglist) {
    char cwd[PATH_MAX];
    sgetcwd(cwd, sizeof(cwd));

    if (repo_folder_initialize(cwd, arglist->start_branch, arglist->bare) == 1) {
        printf("Existing repository detected, reinitialized\n");
    } else {
        printf("Initalized an empty repository in '%s'\'n", cwd);
    }

    (void)arglist;
    return 0;
}

int run_add(const arglist_add_t *arglist) {
    if (arglist->size_pargs <= 0) {
        return info("No files specified, nothing added.");
    }
    int status = 0;
    int check_ignores = arglist->force == 0;
    const git_repo *repo = git_repo_init();
    git_dircache *dircache = create_dircache(repo);
    git_file_list *files = git_fl_init();

    for (int i = 0; i < arglist->size_pargs; i++) {
        if (strlen(arglist->pargs[i]) == 0) {
            continue;
        }
        pathspec_item *pathspec = pathspec_parse(repo, NULL, arglist->pargs[i]);
        if (!git_fl_working_tree_files(files, repo, pathspec, check_ignores)) {
            fatal("pathspec '%s' did not match any files", arglist->pargs[i]);
        }
        pathspec_free(pathspec);
    }
    
    for (int i = 0; i < files->size; i++) {
        status |= cmd_add_file(repo, dircache, files->items[i]->name);
    }

    write_index(repo, dircache);

    git_fl_free(files);
    free_dircache(dircache);
    free((void *)repo);

    return status;
}

int run_rm(const arglist_rm_t *arglist) {
    if (arglist->size_pargs <= 0) {
        return info("No files specified, nothing added.");
    }

    int status = 0;
    const git_repo *repo = git_repo_init();
    git_dircache *dircache = create_dircache(repo);
    git_ref *head_ref = read_head(repo);
    git_obj_commit *head_commit = head_ref->empty_hash ? NULL : commit_read(repo, head_ref->hash);
    git_obj_tree *head_tree = head_commit ? read_tree_from_disk(repo, head_commit->tree_hash) : NULL;
    git_file_list *files = git_fl_init();

    for (int i = 0; i < arglist->size_pargs; i++) {
        if (strlen(arglist->pargs[i]) == 0) {
            continue;
        }
        pathspec_item *pathspec = pathspec_parse(repo, NULL, arglist->pargs[i]);
        if (!git_fl_dircache_files(files, dircache, pathspec)) {
            fatal("pathspec '%s' did not match any files", arglist->pargs[i]);
        }
        pathspec_free(pathspec);
    }
    
    for (int i = 0; i < files->size; i++) {
        status |= cmd_rm_file(repo, dircache, head_tree, files->items[i]->name, arglist->staged_only, arglist->force);
    }

    write_index(repo, dircache);

    git_fl_free(files);
    free_ref(head_ref);
    if (head_tree) free_tree(head_tree);
    if (head_commit) commit_free(head_commit);
    free_dircache(dircache);
    free((void *)repo);

    return status;
}

int run_commit(const arglist_commit_t *arglist) {
    if (arglist->message == NULL) {
        return error("No commit message");
    }

    const git_repo *repo = git_repo_init();
    inifile *repo_config = inifile_read(repo->config_path);
    inifile *global_config = inifile_read(git_global_config_path());

    const char *author = arglist->name;
    if (!author) author = inifile_get_str(repo_config, "user", "name");
    if (!author) author = inifile_get_str(global_config, "user", "name");
    if (!author || !strlen(author)) {
        fatal("cannot commit as no username is set!\n(use './gordit config [-g] user.name <NAME>')");
    }

    const char *author_email = arglist->email;
    if (!author_email) author_email = inifile_get_str(repo_config, "user", "email");
    if (!author_email) author_email = inifile_get_str(global_config, "user", "email");
    if (!author_email || !strlen(author_email)) {
        fatal("cannot commit as no email is set!\n(use './gordit config [-g] user.email <EMAIL>')");
    }

    git_dircache *dircache = create_dircache(repo);

    int status = cmd_commit_index(repo, dircache, author, author_email, arglist->message);

    inifile_free(global_config);
    inifile_free(repo_config);
    free_dircache(dircache);
    free((void *)repo);

    return status;
}

int run_status(const arglist_status_t *arglist) {
    const git_repo *repo = git_repo_init();
    git_dircache *dircache = create_dircache(repo);
    git_ref *head_ref = read_head(repo);
    git_obj_commit *head_commit = head_ref->empty_hash ? NULL : commit_read(repo, head_ref->hash);
    git_obj_tree *head_tree = head_commit ? read_tree_from_disk(repo, head_commit->tree_hash) : NULL;

    git_file_list *files = git_fl_init();
    for (int i = 0; i < arglist->size_pargs; i++) {
        if (strlen(arglist->pargs[i]) == 0) {
            continue;
        }
        pathspec_item *pathspec = pathspec_parse(repo, NULL, arglist->pargs[i]);
        if (!git_fl_dircache_files(files, dircache, pathspec) && 
            !git_fl_tree_obj_files(files, head_tree, pathspec) &&
            !git_fl_working_tree_files(files, repo, pathspec, 1)) {
            fatal("pathspec '%s' did not match any files", arglist->pargs[i]);
        }
        pathspec_free(pathspec);
    }
    
    print_repo_status(repo, files);

    git_fl_free(files);
    free_ref(head_ref);
    if (head_tree) free_tree(head_tree);
    if (head_commit) commit_free(head_commit);
    free_dircache(dircache);
    free((void *)repo);

    return 0;
}

int run_log(const arglist_log_t *arglist) {
    const git_repo *repo = git_repo_init();
    git_ref *head_ref = read_head(repo);

    if (head_ref->empty_hash) {
        fatal("no commits on branch '%s'", head_ref->name);
    }

    if (is_head_detached(head_ref)) {
        printf("HEAD -> %s (detached)\n\n", head_ref->hash);
    } else {
        printf("HEAD -> %s -> %s\n\n", head_ref->name, head_ref->hash);
    }

    print_commit_tree(repo, head_ref->hash, COLOUR_GREEN);
    free((void *)repo);

    (void)arglist;
    return 0;
}

int run_branch(const arglist_branch_t *arglist) {
    const git_repo *repo = git_repo_init();
    git_ref *head_ref = read_head(repo);

    int status = 0;
    if (arglist->new_branch != NULL) {
        status = cmd_branch_new(repo, head_ref, arglist->new_branch);
    } else if (arglist->del_branch != NULL) {
        status = cmd_branch_delete(repo, head_ref, arglist->del_branch);
    } else {
        status = cmd_branch_list(repo, head_ref);
    }

    free((void *)repo);
    free_ref(head_ref);

    return status;
}

int run_checkout(const arglist_checkout_t *arglist) {
    if (arglist->branch && arglist->size_pargs) {
        return error("cannot specify both a branch and file(s) to checkout");
    } else if (arglist->branch == NULL && arglist->size_pargs == 0) {
        return error("nothing to checkout");
    }
    char *branch_name = arglist->branch; 

    int status = 0;
    const git_repo *repo = git_repo_init();
    git_ref* target_ref = branch_name ? read_ref(repo, REF_LOCAL, branch_name, 1) : read_head(repo);
    git_obj_commit *target_commit = commit_read(repo, target_ref->hash);
    git_obj_tree *target_tree = read_tree_from_disk(repo, target_commit->tree_hash);

    if (branch_name && !arglist->size_pargs) {
        return cmd_checkout_branch(repo, target_tree, target_ref);
    }
    
    git_file_list *files = git_fl_init();

    for (int i = 0; i < arglist->size_pargs; i++) {
        if (strlen(arglist->pargs[i]) == 0) {
            continue;
        }
        pathspec_item *pathspec = pathspec_parse(repo, NULL, arglist->pargs[i]);
        if (!git_fl_tree_obj_files(files, target_tree, pathspec)) {
            fatal("pathspec '%s' did not match any files", arglist->pargs[i]);
        }
        pathspec_free(pathspec);
    }
    
    for (int i = 0; i < files->size; i++) {
        status |= cmd_restore_file(repo, target_tree, files->items[i]->name);
    }

    git_fl_free(files); 
    free((void *)repo);
    free_ref(target_ref);
    commit_free(target_commit);
    free_tree(target_tree);

    return status;
}

/*
TODO: can also diff:
- dircache and working tree (unstaged) COMPLETE
- dircache and tree
    - dircache and HEAD tree (staged)
- two trees
    - two commits tree
    - two branch tips
- two commits and LCA commit

- "name only" flag
- "show names and lines changed only" flag
*/
int run_diff(const arglist_diff_t *arglist) {
    const git_repo *repo = git_repo_init();
    git_dircache *dircache = create_dircache(repo);

    int status = cmd_diff_repo_index(repo, dircache);

    free_dircache(dircache);
    free((void *)repo);

    (void)arglist;
    return status;
}

int run_config(const arglist_config_t *arglist) {
    const git_repo *repo = git_repo_init();
    const char *config_path = repo->config_path;
    if (arglist->global) {
        config_path = git_global_config_path();
    }

    inifile *config = inifile_read(config_path);

    if (arglist->show_all) {
        inifile_print(config);
        goto config_cleanup;
    }

    if (arglist->size_pargs < 1) {
        inifile_free(config);
        return error("a name must be specified");
    } 

    char *name = sstrdup(arglist->pargs[0]);
    char *section = strtok(arglist->pargs[0], ".");
    char *key = NULL;
    if (section == NULL || (key = strtok(NULL, "")) == NULL) {
        free(name);
        inifile_free(config);
        return error("invalid name '%s' (a name is a section and a key seperated by a '.')", arglist->pargs[0]);
    }

    if (arglist->size_pargs == 1) {
        if (arglist->delete) {
            if (inifile_delete_item(config, section, key)) {
                inifile_write(config, config_path);
                printf("deleted %s.%s from '%s'\n", section, key, config_path);
            }
        } else {
            const char *value = inifile_get_str(config, section, key);
            printf("%s%s", value ? value : "", value ? "\n" : "");
        }
    } else {
        const char *value = arglist->pargs[1];
        inifile_update(config, section, key, value);
        inifile_write(config, config_path);
        printf("set %s.%s to '%s' in '%s'\n", section, key, value, config_path);
    }
    free(name);

config_cleanup:;
    inifile_free(config);
    free((void *)repo);

    return 0;
}