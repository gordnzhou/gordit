#include <string.h>

#include "status.h"
#include "dircache.h"
#include "commit.h"
#include "utils.h"
#include "refs.h"
#include "pathspec.h"
#include "logging.h"
#include "diff.h"

void print_repo_status(const git_repo *repo) {
    git_dircache *dircache = create_dircache(repo);
    git_ref *head_ref = read_head(repo);
    
    git_obj_commit *commit = head_ref->empty_hash ? NULL : 
        read_commit_from_disk(repo, head_ref->hash);
    git_obj_tree *commit_tree = commit == NULL ? NULL : read_tree_from_disk(repo, commit->tree_hash);
    int commit_tree_size = commit == NULL ? 0 : tree_num_blobs(commit_tree);

    strarr_t *unignored_files = repo_all_files(repo, 1);

    git_diff uncommitted = { 0 };
    git_diff unstaged    = { 0 };
    git_diff untracked   = { 0 };
    uncommitted.entries  = smalloc((dircache->num_entries + commit_tree_size)*sizeof(diff_entry));
    unstaged.entries     = smalloc(dircache->num_entries*sizeof(diff_entry));
    untracked.entries    = smalloc(unignored_files->len*sizeof(diff_entry));

    diff_index_tree(&uncommitted, dircache, commit_tree, commit_tree_size);
    diff_repo_index_tracked(&unstaged, repo, dircache);
    diff_repo_index_untracked(&untracked, unignored_files, dircache);

    #define INDENT "   "

    if (is_head_detached(head_ref)) {
        printf("HEAD is detached\n");
    } else {
        printf("On branch '%s'\n", head_ref->name);
    }

    if (uncommitted.size) {
        printf("\nChanges to be committed:\n");
        for (int i = 0; i < uncommitted.size; i++) { 
            char *status = NULL;
            if (uncommitted.entries[i].type == ADDED)    status = "   added";
            if (uncommitted.entries[i].type == MODIFIED) status = "modified";
            if (uncommitted.entries[i].type == REMOVED)  status = " removed";
            assert(status);
            printf("%s%s:  %s\n", INDENT, status, uncommitted.entries[i].name);
        }
    }

    if (unstaged.size) {
        printf("\nChanges not staged for commit:\n");
        for (int i = 0; i < unstaged.size; i++) {
            char *status = NULL;
            if (unstaged.entries[i].type == MODIFIED) status = "modified";
            if (unstaged.entries[i].type == REMOVED)  status = " removed";
            assert(status);
            printf("%s%s: %s\n", INDENT, status, unstaged.entries[i].name);
        }
    }
   
    if (untracked.size) {
        printf("\nUntracked files:\n");
        for (int i = 0; i < untracked.size; i++) {
            assert(untracked.entries[i].type == ADDED);
            printf("%s%s\n", INDENT, untracked.entries[i].name);
        }
    }

    if (!commit) {
        printf("\nThis repo has no commits\n");
    } else if (uncommitted.size == 0 && unstaged.size == 0 && untracked.size == 0) {
        printf("\nWorking tree is clean, nothing to commit or add\n");
    } else if (unstaged.size == 0) {
        if (untracked.size == 0) {
            printf("\nNo changes added to be committed\n");
        } else {
            printf("\nNo changes added but there are untracked files\n");
        }
    } else if (uncommitted.size == 0) {
        printf("No changes to commit");
    }

    free(uncommitted.entries);
    free(unstaged.entries);
    free(untracked.entries);
    strarr_free(unignored_files);
    if (commit_tree) free_tree(commit_tree);
    if (commit) free_commit(commit);
    free_ref(head_ref);
    free_dircache(dircache);
}

void print_commit_tree(const git_repo *repo) {
    git_ref *head_ref = read_head(repo);

    if (head_ref->empty_hash) {
        fatal("no commits on branch '%s'", head_ref->name);
    }

    if (is_head_detached(head_ref)) {
        printf("HEAD -> %s (detached)\n\n", head_ref->hash);
    } else {
        printf("HEAD -> %s -> %s\n\n", head_ref->name, head_ref->hash);
    }

    // TODO: print commits reachable by HEAD
    // - better print_commit
    // - handle commits multiple 2+ parents, 
    // - option to filter log by branch

    git_obj_commit *commit = read_commit_from_disk(repo, head_ref->hash);
    printf("COMMIT %s\n", head_ref->hash);
    while (1) {
        print_commit(commit);
        printf("\n");

        if (commit->num_parents < 1) {
            break;
        }

        git_obj_commit *parent_commit = read_commit_from_disk(repo, commit->parents[0]);
        printf("COMMIT %s\n", commit->parents[0]);
        free_commit(commit);
        commit = parent_commit;
    }

    free_commit(commit);
}