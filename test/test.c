#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "repo.h"
#include "filesystem.h"
#include "objects.h"
#include "dircache.h"
#include "fileinfo.h"
#include "utils.h"
#include "commit.h"

#define ASSERT_STREQ(act, exp) \
    if (strcmp(exp, act) != 0) { \
        printf("Expected: %s, but got: %s\n", exp, act); assert(0); \
    } \

void test_filesystem() {
    char out[PATH_MAX];

    #define TEST_PATH_PARENT(in, exp, ok) \
        do { \
        int res = fs_path_dirname(in, out); \
        assert(res == ok); \
        ASSERT_STREQ(out, exp) \
        } while (0)

    #define TEST_PATH_BASENAME(in, exp) \
        do { \
        fs_path_basename(in, out); \
        ASSERT_STREQ(out, exp) \
        } while (0)

#ifdef _WIN32

    TEST_PATH_PARENT("C:\\home", "C:", 0);
    TEST_PATH_PARENT("C:\\a\\b\\", "C:\\a", 0);
    TEST_PATH_PARENT("/", "/", 0);
    TEST_PATH_PARENT(".\\bob", ".", 0);
    TEST_PATH_PARENT(".\\a\\b\\c/d", ".\\a\\b\\c", 0);
    TEST_PATH_PARENT("C:/A\\b\\", "C:/A", 0);
    TEST_PATH_PARENT("C:", ".", 1);
    TEST_PATH_PARENT("C:\\", ".", 0);
    TEST_PATH_PARENT("\\\\share\\a", "\\\\share", 0);
    TEST_PATH_PARENT("file.txt", ".", 1);

    TEST_PATH_PARENT("", "", 1);
    TEST_PATH_PARENT(".", ".", 1);
    TEST_PATH_PARENT("\\\\share", ".", 0);
    TEST_PATH_PARENT("/home", ".", 0);

    TEST_PATH_BASENAME("C:\\home", "home");
    TEST_PATH_BASENAME("C:\\a\\b\\", "b");
    TEST_PATH_BASENAME("/", "/");
    TEST_PATH_BASENAME(".\\bob", "bob");
    TEST_PATH_BASENAME(".\\a\\b\\c/d", "d");
    TEST_PATH_BASENAME("C:/A\\b\\", "b");
    TEST_PATH_BASENAME("C:", "C:");
    TEST_PATH_BASENAME("C:\\", "C:");
    TEST_PATH_BASENAME("\\\\shar\\a", "a");
    TEST_PATH_BASENAME("file.txt", "file.txt");

    TEST_PATH_BASENAME("", "");
    TEST_PATH_BASENAME(".", ".");
    TEST_PATH_BASENAME("\\\\share", "share");
    TEST_PATH_BASENAME("/home", "home");
#else
    TEST_PATH_PARENT("C:/home", "C:", 0);
    TEST_PATH_PARENT("C:/a/b/", "C:/a", 0);
    TEST_PATH_PARENT("/", "/", 0);
    TEST_PATH_PARENT("./bob", ".", 0);
    TEST_PATH_PARENT("./a/b/c/d", "./a/b/c", 0);
    TEST_PATH_PARENT("C:/A/b/", "C:/A", 0);
    TEST_PATH_PARENT("C:", ".", 1);
    TEST_PATH_PARENT("C:/", ".", 0);
    TEST_PATH_PARENT("/share/a", "/share", 0);
    TEST_PATH_PARENT("file.txt", ".", 1);
    
    TEST_PATH_PARENT("", ".", 1);
    TEST_PATH_PARENT(".", ".", 1);
    TEST_PATH_PARENT("/home", "/", 0);

    TEST_PATH_BASENAME("C:/home", "home");
    TEST_PATH_BASENAME("C:/a/b/", "b");
    TEST_PATH_BASENAME("/", "/");
    TEST_PATH_BASENAME("./bob", "bob");
    TEST_PATH_BASENAME("./a/b/c/d", "d");
    TEST_PATH_BASENAME("C:/A/b/", "b");
    TEST_PATH_BASENAME("C:", "C:");
    TEST_PATH_BASENAME("C:/", "C:");
    TEST_PATH_BASENAME("file.txt", "file.txt");

    TEST_PATH_BASENAME("", ".");
    TEST_PATH_BASENAME(".", ".");
    TEST_PATH_BASENAME("/home/", "home");
#endif

    printf("================FILESYSTEM TESTS PASSED================\n");
}

void test_objects(const git_repo *repo) {
    char *hash;
    git_obj *blob, *blob2;

    fileinfo *info = start_fileinfo(repo, "notes.md", "rb");
    assert(info != NULL);
    blob = create_blob_from_file(info);
    end_fileinfo(info);
    assert(blob != NULL);
    assert(blob->type == OBJ_TYPE_BLOB);
    hash = blob->hash;
    printf("hash of blob: %s\n", hash);

    write_obj_to_disk(repo, blob);
    
    blob2 = read_blob_from_disk(repo, hash);
    assert(blob2 != NULL);
    ASSERT_STREQ(blob2->hash, hash)
    assert(blob2->size == blob->size);

    assert(create_file_from_blob("bin/notes.md", blob2) == 0);
    assert(fs_file_exists("bin/notes.md") == 1);
    printf("================BLOB TESTS PASSED=============\n");

    char path2[] = "./src";
    git_obj_tree *tree, *tree2;

    tree = create_tree_from_path(repo, path2);
    assert(tree != NULL);
    assert(tree->size > 0);
    printf("hash of tree: %s\n", tree->obj.hash);
    print_tree(tree);
     
    write_tree_to_disk(repo, tree, 0);

    tree2 = read_tree_from_disk(repo, tree->obj.hash);
    assert(tree2 != NULL);
    ASSERT_STREQ(tree2->obj.hash, tree->obj.hash);
    assert(tree2->size == tree->size);

    printf("================TREE TESTS PASSED=============\n");

    git_obj_commit *commit, *commit2;
    git_obj *commit_obj;
    obj_hash *commit_hash;

    commit = create_commit(tree, 1, &(blob->hash), "Gordon Zhou", "gordonzhou223@gmail.com", "hello world");

    commit_obj = create_commit_obj(commit);
    commit_hash = &(commit_obj->hash);
    write_obj_to_disk(repo, commit_obj);

    commit2 = read_commit_from_disk(repo, *commit_hash);
    ASSERT_STREQ(commit2->tree_hash, commit->tree_hash);
    ASSERT_STREQ(commit2->msg, commit->msg);
    ASSERT_STREQ(commit2->author_name, commit->author_name);
    ASSERT_STREQ(commit2->author_email, commit->author_email);
    assert(commit2->num_parents == commit->num_parents);
    ASSERT_STREQ(commit2->parents[0], commit->parents[0]);
    assert(commit2->timestamp == commit->timestamp);

    print_commit(commit2);
    printf("================COMMIT TESTS PASSED=============\n");
    
    free_commit(commit);
    free_commit(commit2);
    free_obj(blob);
    free_obj(blob2);
    free_tree(tree);
    // fs_remove("bin/notes.md");
}

void test_index(const git_repo * repo) {
    git_dircache *dircache = create_dircache(repo);
    print_dircache(dircache);

    char *path = "notes.md";
    printf("adding %s to index...\n", path);

    struct fileinfo *info = start_fileinfo(repo, path, "rb");
    assert(info != NULL);
    git_obj *blob = create_blob_from_file(info);
    dircache_add(dircache, info, blob);
    end_fileinfo(info);

    print_dircache(dircache);

    char *prev = "";
    for (int i = 0; i < dircache->num_entries; i++) {
        git_index_entry *entry = dircache->entries[i];
        assert(strcmp(prev, entry->name) <= 0 && "entries are not sorted");
        prev = entry->name;
    }

    git_obj_tree *tree = build_tree_from_index(dircache);
    assert(tree != NULL);
    print_tree(tree);

    // assert(dircache_remove(dircache, info->norm_path) != -1);

    write_index(repo, dircache);

    free_dircache(dircache);
    free_tree(tree);
    free_obj(blob);
    printf("================INDEX TESTS PASSED=============\n");
}

int main() {
    const git_repo *repo = repo_init_context();
    assert(repo != NULL && "Cannot get repo"); 

    test_filesystem();
    test_objects(repo);
    test_index(repo);

    free((void *)repo);
    printf("Success! All tests passed!\n");
    return 0;
}