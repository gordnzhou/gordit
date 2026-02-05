#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "repo.h"
#include "filesystem.h"
#include "objects.h"
#include "dircache.h"
#include "filespec.h"

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
    git_obj_blob *blob, *blob2;

    fileinfo *info = start_fileinfo(repo, "notes.md", "rb");
    assert(info != NULL);
    blob = create_blob_from_file(info);
    end_fileinfo(info);
    assert(blob != NULL);
    ASSERT_STREQ(blob->obj.type, "blob")
    hash = blob->obj.hash;
    printf("hash of blob: %s\n", hash);

    assert(write_blob_to_disk(repo, blob) == 0);
    
    blob2 = create_blob_from_disk(repo, hash);
    assert(blob2 != NULL);
    ASSERT_STREQ(blob2->obj.hash, hash)
    assert(blob2->obj.size == blob->obj.size);

    assert(create_file_from_blob("build/notes.md", blob2) == 0);
    assert(fs_file_exists("build/notes.md") == 1);
    printf("================BLOB TESTS PASSED=============\n");

    char path2[] = "./include";
    git_obj_tree *tree, *tree2;

    tree = create_tree_from_path(repo, path2);
    assert(tree != NULL);
    assert(tree->size > 0);
    printf("hash of tree: %s\n", tree->obj.hash);
    print_tree(tree);
     
    assert(write_tree_to_disk(repo, tree) == 0);

    tree2 = create_tree_from_disk(repo, tree->obj.hash);
    assert(tree2 != NULL);
    ASSERT_STREQ(tree2->obj.hash, tree->obj.hash);
    assert(tree2->size == tree->size);

    assert(tree_cmp(tree, tree2, path2) == 0);


    free_blob(blob);
    free_blob(blob2);
    free_tree(tree);
    printf("================TREE TESTS PASSED=============\n");
    // fs_remove("build/notes.md");
}

void test_index(const git_repo * repo) {
    git_dircache *dircache = create_dircache(repo);
    print_dircache(dircache);

    char *path = "build/test.o";
    printf("adding %s to index...\n", path);

    struct fileinfo *info = start_fileinfo(repo, path, "rb");
    assert(info != NULL);
    assert(add_file_to_dc(dircache, info) == 0);
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

    assert(remove_file_from_dc(dircache, info) != -1);

    // assert(write_index(repo, dircache) == 0);

    free_dircache(dircache);
    free_tree(tree);
    printf("================INDEX TESTS PASSED=============\n");
}

int main() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }
    
    const git_repo *repo = get_working_repo(cwd);
    assert(repo != NULL && "Cannot get repo"); 

    test_filesystem();
    test_objects(repo);
    test_index(repo);

    free((void *)repo);
    printf("Success! All tests passed!\n");
    return 0;
}