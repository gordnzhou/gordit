
#include "status.h"
#include "dircache.h"
#include "commit.h"
#include "utils.h"
#include "refs.h"

void print_repo_status(const git_repo *repo) {
    int detached_head;
    git_dircache *dircache = create_dircache(repo);
    git_obj_commit *commit = read_head_commit(repo, &detached_head);
    if (detached_head) {
        printf("HEAD is detached\n");
    } else {
        char *head_content = read_head(repo, &detached_head);
        printf("On branch '%s'\n", fs_path_pbasename(head_content));
        free(head_content);
    }

    if (commit == NULL) {
        printf("\nThis repo has no commits\n");
        free_dircache(dircache);
        return;
    }
    
    git_obj_tree *commit_tree = read_tree_from_disk(repo, commit->tree_hash);
    int commit_tree_size = tree_num_blobs(commit_tree);
    git_tree_entry **commit_blob_list = smalloc(commit_tree_size * sizeof(git_tree_entry *));
    tree_blobs_flat(commit_blob_list, commit_tree_size, commit_tree);

    int list_size = commit_tree_size + dircache->num_entries;
    int names_idx = 0;
    char **names  = scalloc(list_size, sizeof(char *));
    char **status = scalloc(list_size, sizeof(char *));

    int tree_i = 0;
    int index_i = 0;
    while (tree_i < commit_tree_size || index_i < dircache->num_entries) {
        git_tree_entry *tree_entry = NULL;
        git_index_entry *index_entry = NULL;

        int cmp = 0;
        if (tree_i == commit_tree_size) {
            index_entry = dircache->entries[index_i];
            cmp = -1;
        } else if (index_i == dircache->num_entries) {
            tree_entry = commit_blob_list[tree_i];
            cmp = 1;
        } else {
            tree_entry = commit_blob_list[tree_i];
            index_entry = dircache->entries[index_i];
            cmp = strcmp(index_entry->name, tree_entry->name);
        }

        if (cmp == 0) {
            if (strcmp(tree_entry->u.blob_hash, index_entry->hash) != 0) {
                names[names_idx] = index_entry->name;
                status[names_idx] = "modified";
                names_idx++;
            }

            tree_i++;
            index_i++;
        } else if (cmp < 0) {
            names[names_idx] = index_entry->name;
            status[names_idx] = "   added";
            names_idx++;

            index_i++;
        } else {
            names[names_idx] = tree_entry->name;
            status[names_idx] = " deleted";
            names_idx++;

            tree_i++;
        }
    }

    if (names_idx > 0) {
        printf("\nChanges to be commited:\n");
        for (int i = 0; i < names_idx; i++) {
            printf("    %s:  %s\n", status[i], names[i]);
        }
    } else {
        printf("\nNo changes to be commited\n");
    }


    printf("\nChanges not staged for commit:\n");
    char path[PATH_MAX];
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        fs_path_join(repo->root_path, entry->name, path);

        if (!fs_file_exists(path)) {
            printf("     deleted: %s\n", entry->name);
            continue;
        }

        fileinfo *info = start_fileinfo(repo, path, "rb");
        
        if (!is_stat_same(&(info->stat), &(entry->info))) {
            git_obj *blob = create_blob_from_file(info);
            if (!blob) {
                continue;
            }

            if (strcmp(blob->hash, entry->hash) != 0) {
                printf("    modified: %s\n", entry->name);
            }

            free_obj(blob);
        }
    }

    // TODO: show untracked files (files NOT in index AND NOT ignnored)

    free(names);
    free(status);

    free(commit_blob_list);
    free_tree(commit_tree);
    free_commit(commit);
    free_dircache(dircache);
}