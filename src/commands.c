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
#include "textdiff.h"

int cmd_add_file(const git_repo *repo, git_dircache *dircache, const char *git_path_name) {
    fileinfo *info = start_fileinfo(repo, git_path_name, "rb");

    if (dircache_matching_entry(dircache, info)) {
        DEBUG_PRINT("skipping add to '%s' to index", info->norm_path);
        end_fileinfo(info);
        return 0;
    }

    git_obj *blob = create_blob_from_file(info);
    if (!blob) {
        return error("could create blob from '%s'", info->norm_path);
    }

    dircache_add(dircache, info, blob);

    write_obj_to_disk(repo, blob);
    DEBUG_PRINT("wrote %s to disk for %s\n", blob->hash, info->norm_path);

    free_obj(blob);
    end_fileinfo(info);

    return 0;
}

int cmd_rm_file(const git_repo *repo, git_dircache *dircache, const git_obj_tree *head_tree, 
    const char *git_path_name, int staged_only, int force
) {
    git_index_entry **in_index = dircache_find_file(dircache, git_path_name);
    if (!in_index) {
        return 0;
    }

    git_index_entry *entry = *in_index;
    fileinfo *info = start_fileinfo(repo, git_path_name, "rb");
    
    if (!force) {
        if (head_tree != NULL) {
            git_tree_entry **found = tree_find_blob(head_tree, git_path_name);
            if (found != NULL && !hash_eq(entry->hash, (*found)->u.blob_hash)) {
                fatal("cannot remove '%s' as it has uncommited changes", git_path_name);
            }
        }

        if (is_file_changed_unstaged(entry, info)) {
            fatal("cannot remove '%s' as it has unstaged changes", git_path_name);
        }
    }
    
    if (dircache_remove(dircache, info->norm_path) != 0) {
        DEBUG_PRINT("removing '%s' from index but it is not in dircache", info->abs_path); 
    }

    if (!staged_only) {
        sremove(info->abs_path);
    }

    end_fileinfo(info);

    return 0;
}

int cmd_commit_index(const git_repo *repo, git_dircache *dircache, 
    const char *author_name, const char *author_email, const char *msg
) {
    if (dircache_has_conflicts(dircache)) {
        return error("there are unmerged files in index, cannot commit");
    }
    
    git_obj_tree *tree = build_tree_from_index(dircache);
    if (tree->size == 0) {
        warn("commiting an empty tree..");
    }

    git_ref *head_ref = read_head(repo);
    if (is_head_detached(head_ref)) {
        warn("head is not pointing to any branch, commit will be easily lost!");
    }

    // TODO: read MERGE_HEAD for possibly 1+ other commit parents
    if (!head_ref->empty_hash) {
        git_obj_commit *parent_commit = commit_read(repo, head_ref->hash);
        git_obj_tree *parent_tree = read_tree_from_disk(repo, parent_commit->tree_hash);
        if (is_tree_and_dc_same(dircache, parent_tree)) {
            free_ref(head_ref);
            free_tree(parent_tree);
            commit_free(parent_commit);
            return info("nothing new for me to commit!");
        }

        free_tree(parent_tree);
        commit_free(parent_commit);
    }

    int num_parents = head_ref->empty_hash ? 0 : 1;
    obj_hash *parents = head_ref->empty_hash ? NULL : &(head_ref->hash); 

    git_obj_commit *commit = commit_new(tree, num_parents, parents, author_name, author_email, msg);
    git_obj *commit_obj = create_commit_obj(commit);
    obj_hash *commit_hash = &(commit_obj->hash);

    write_obj_to_disk(repo, commit_obj);
    write_tree_to_disk(repo, tree, 1);
    // print_tree(tree);

    commit_print(commit);

    if (head_ref->type == DIRECT) {
        copy_hash(&(head_ref->hash), commit_hash);
        move_head(repo, head_ref);
    } else {
        write_ref(repo, head_ref->type, head_ref->name, commit_hash);
    }

    free_obj(commit_obj);
    free(commit);
    free_tree(tree);
    free_ref(head_ref);
    
    return 0;
}

int cmd_checkout_branch(const git_repo *repo, const git_obj_tree *target_tree, git_ref *target_ref) {
    git_ref *head_ref = read_head(repo);
    if (head_ref->type != DIRECT) {
        if (strcmp(head_ref->name, target_ref->name) == 0) {
            warn("already on branch '%s'", target_ref->name);
        }
    }

    git_dircache *dircache = create_dircache(repo);
    git_obj_commit *head_commit = head_ref->empty_hash ? NULL : commit_read(repo, head_ref->hash);
    git_obj_tree *head_tree = head_commit ? read_tree_from_disk(repo, head_commit->tree_hash) : NULL;

    int head_size = tree_num_blobs(head_tree);
    int target_size = tree_num_blobs(target_tree);

    git_diff *diff = diff_new(head_size + target_size);
    diff_trees(diff, target_tree, target_size, head_tree, head_size);
    for (int i = 0; i < diff->size; i++) {
        diff_entry *entry = &(diff->entries[i]);
        git_index_entry **in_index = dircache_find_file(dircache, entry->name);
        if (in_index) {
            git_index_entry *index_entry = *in_index;
            if (entry->type == MODIFIED && !hash_eq(*(entry->new_hash), index_entry->hash)) {
                fatal("cannot checkout: '%s' still has uncommitted changes!", entry->name);
            }
            if (diff_file_repo_index(repo, index_entry) == 1) {
                fatal("cannot checkout: '%s' still has unstaged changes!", entry->name);
            }
        }
    }

    char full_path[PATH_MAX];
    for (int i = 0; i < diff->size; i++) {
        diff_entry *entry = &(diff->entries[i]);
        fs_path_join(repo->root_path, entry->name, full_path);

        if (entry->type == REMOVED) {
            if (fs_file_exists(full_path)) {
                sremove(full_path);
            }
            dircache_remove(dircache, entry->name);
        } else {
            git_obj *blob = read_blob_from_disk(repo, *(entry->new_hash));
            if (create_file_from_blob(full_path, blob)) {
                fatal("failed to restore '%s' to object %s", entry->name, blob->hash);
            }

            struct fileinfo *info = start_fileinfo(repo, entry->name, "rb");
            dircache_add(dircache, info, blob);
            DEBUG_PRINT("restored '%s' to object %s", full_path, blob->hash);
            
            free_obj(blob);
            end_fileinfo(info);
        }
    }

    write_index(repo, dircache);
    move_head(repo, target_ref);

    free_dircache(dircache);
    free_ref(head_ref);
    if (head_commit) commit_free(head_commit);
    if (head_tree) free_tree(head_tree);

    return 0;
}

int cmd_restore_file(const git_repo *repo, const git_obj_tree *target_tree, const char *git_path_name) {
    static char full_path[PATH_MAX];
    git_tree_entry **in_other = tree_find_blob(target_tree, git_path_name);
    if (in_other == NULL) {
        return 0;
    }

    git_tree_entry *entry = *in_other;
    assert(entry->type == OBJ_TYPE_BLOB);
    
    repo_full_path(repo, entry->name, full_path);
    git_obj *blob = read_blob_from_disk(repo, entry->u.blob_hash);

    if (create_file_from_blob(full_path, blob)) {
        fatal("failed to restore '%s' to object %s", full_path, blob->hash);
    }

    DEBUG_PRINT("restored '%s' to object %s", full_path, blob->hash);
    free_obj(blob);

    return 0;
}

int cmd_diff_repo_index(const git_repo *repo, git_dircache *dircache) {
    strarr_t *files_all = working_tree_all_files(repo, 1);
    git_diff *diff = diff_new(dircache->num_entries + files_all->len);
    diff_repo_index(diff, repo, files_all, dircache, NULL);

    for (int i = 0; i < diff->size; i++) {
        diff_entry *entry = diff->entries + i;
        if (entry->type == REMOVED) {
            printf("removed: %s\n", entry->name); 
        } else if (entry->type == ADDED) {
            printf("added: %s\n", entry->name);
        } else if (entry->type == MODIFIED) {
            printf("modified: %s\n", entry->name);

            fileinfo *info = start_fileinfo(repo, entry->name, "rb");
            if (is_like_binary(info->fptr)) {
                end_fileinfo(info);
                continue;
            }
            git_textfile *new_text = textfile_from_file(info);
            end_fileinfo(info);

            git_obj *blob = read_blob_from_disk(repo, *(entry->old_hash));
            git_textfile *old_text = textfile_from_blob(blob);
            free_obj(blob);

            textdiff_myers_print(new_text, entry->name, old_text, entry->name);

            textfile_free(old_text);
            textfile_free(new_text);
        }
    }

    diff_free(diff);
    strarr_free(files_all);
 
    return 0;
}

int cmd_branch_new(const git_repo *repo, git_ref *head_ref, const char *branch_name) {
    if (branch_name == NULL) {
        return error("specify name of branch to add!");
    }
    if (!is_valid_branch_name(branch_name)) {
        return error("invalid branch name '%s'", branch_name);
    }

    if (ref_exists(repo, REF_LOCAL, branch_name)) {
        fatal("a local branch named '%s' already exists", branch_name);
    }

    if (head_ref->empty_hash) {
        fatal("cannot create new branch: HEAD has no commits");
    }

    write_ref(repo, REF_LOCAL, branch_name, &(head_ref->hash));

    printf("created branch '%s' -> %s\n", branch_name, head_ref->hash);
    return 0;
}

int cmd_branch_delete(const git_repo *repo, const git_ref *head_ref, const char *branch_name) {
    if (branch_name == NULL) {
        return error("specify name of branch to remove!");
    }
    if (strcmp(head_ref->name, branch_name) == 0) {
        fatal("cannot remove current branch HEAD is pointing to!");
    }

    git_ref *target_ref = read_ref(repo, REF_LOCAL, branch_name, 1);

    int reachable = target_ref->empty_hash;
    if (!target_ref->empty_hash) {

        strarr_t *refs = refs_all_names(repo, REF_LOCAL);
        for (size_t i = 0; i < refs->len; i++) {
            git_ref *ref = read_ref(repo, REF_LOCAL, refs->data[i], 0);
            reachable = !ref->empty_hash && commit_find_parent(repo, ref->hash, target_ref->hash);
            free_ref(ref);
            if (reachable) {
                break;
            }
        }
        strarr_free(refs);
    }   

    if (!reachable) {
        fatal("cannot delete '%s' as it points to commit(s) unreachable from other branches", branch_name);
    }

    del_ref(repo, REF_LOCAL, branch_name);
    printf("removed branch '%s'\n", branch_name);

    free_ref(target_ref);

    return 0;
}

int cmd_branch_list(const git_repo *repo, const git_ref *head_ref) {
    strarr_t *ref_names = refs_all_names(repo, REF_LOCAL);
    for (size_t i = 0; i < ref_names->len; i++) {
        char *ref_name = ref_names->data[i];
        int is_current = head_ref->type != DIRECT ? strcmp(ref_name, head_ref->name) == 0 : 0;
        printf("%s %s\n", is_current ? "*" : " ", ref_name);
    } 
    strarr_free(ref_names);

    return 0;
}