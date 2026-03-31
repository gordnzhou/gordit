#include <string.h>

#include "status.h"
#include "dircache.h"
#include "commit.h"
#include "utils.h"
#include "refs.h"
#include "pathspec.h"
#include "logging.h"
#include "diff.h"
#include "colour.h"

void print_repo_status(const git_repo *repo, git_file_list *show_files) {
    git_dircache *dircache = create_dircache(repo);
    git_ref *head_ref = read_head(repo);
    
    git_obj_commit *commit = head_ref->empty_hash ? NULL : 
        commit_read(repo, head_ref->hash);
    git_obj_tree *commit_tree = commit == NULL ? NULL : read_tree_from_disk(repo, commit->tree_hash);
    int commit_tree_size = commit == NULL ? 0 : tree_num_blobs(commit_tree);

    strarr_t *unignored_files = working_tree_all_files(repo, 1);

    git_diff uncommitted = { 0 };
    git_diff unstaged    = { 0 };
    git_diff untracked   = { 0 };
    uncommitted.entries  = smalloc((dircache->num_entries + commit_tree_size)*sizeof(diff_entry));
    unstaged.entries     = smalloc(dircache->num_entries*sizeof(diff_entry));
    untracked.entries    = smalloc(unignored_files->len*sizeof(diff_entry));

    diff_index_tree(&uncommitted, dircache, commit_tree, commit_tree_size, show_files);
    diff_repo_index_tracked(&unstaged, repo, dircache, show_files);
    diff_repo_index_untracked(&untracked, unignored_files, dircache, show_files);

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
            if (uncommitted.entries[i].type == ADDED)    status = "added";
            if (uncommitted.entries[i].type == MODIFIED) status = "modified";
            if (uncommitted.entries[i].type == REMOVED)  status = "removed";
            assert(status);
            colour_print(COLOUR_GREEN, "%10s: %s\n", status, uncommitted.entries[i].name);
        }
    }

    if (unstaged.size) {
        printf("\nChanges not staged for commit:\n");
        for (int i = 0; i < unstaged.size; i++) {
            char *status = NULL;
            if (unstaged.entries[i].type == MODIFIED) status = "modified";
            if (unstaged.entries[i].type == REMOVED)  status = "removed";
            assert(status);
            colour_print(COLOUR_RED, "%10s: %s\n", status, unstaged.entries[i].name);
        }
    }
   
    if (untracked.size) {
        printf("\nUntracked files:\n");
        for (int i = 0; i < untracked.size; i++) {
            assert(untracked.entries[i].type == ADDED);
            colour_print(COLOUR_RED, "%s%s\n", INDENT, untracked.entries[i].name);
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
    if (commit) commit_free(commit);
    free_ref(head_ref);
    free_dircache(dircache);
}

void print_commit_tree(const git_repo *repo, const obj_hash commit_hash, const char *colour) { 
    git_obj_commit *commit = commit_read(repo, commit_hash);
    colour_print(colour, "commit %s\n", commit_hash);
    commit_print(commit);
    printf("\n");

    for (int i = 0; i < commit->num_parents; i++) {
        print_commit_tree(repo, commit->parents[i], COLOUR_YELLOW);
    }
    commit_free(commit);
}