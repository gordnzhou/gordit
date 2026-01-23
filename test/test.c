#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "global.h"
#include "filesystem.h"
#include "objects.h"

#define ASSERT_STREQ(act, exp) \
    if (strcmp(exp, act) != 0) { \
        printf("Expected: %s, but got: %s", exp, act); assert(0); \
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
    TEST_PATH_PARENT("/", "/", 1);
    TEST_PATH_PARENT(".\\bob", ".", 0);
    TEST_PATH_PARENT(".\\a\\b\\c/d", ".\\a\\b\\c", 0);
    TEST_PATH_PARENT("C:/A\\b\\", "C:/A", 0);
    TEST_PATH_PARENT("C:", ".", 1);
    TEST_PATH_PARENT("C:\\", ".", 1);
    TEST_PATH_PARENT("\\\\share\\a", "\\\\share", 0);
    TEST_PATH_PARENT("file.txt", ".", 1);

    TEST_PATH_PARENT("", "", 1);
    TEST_PATH_PARENT(".", ".", 1);
    TEST_PATH_PARENT("\\\\share", ".", 1);
    TEST_PATH_PARENT("/home", ".", 1);

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
    TEST_PATH_PARENT("/", "/", 1);
    TEST_PATH_PARENT("./bob", ".", 0);
    TEST_PATH_PARENT("./a/b/c/d", "./a/b/c", 0);
    TEST_PATH_PARENT("C:/A/b/", "C:/A", 0);
    TEST_PATH_PARENT("C:", ".", 0);
    TEST_PATH_PARENT("C:/", ".", 0);
    TEST_PATH_PARENT("/share/a", "/share", 0);
    TEST_PATH_PARENT("file.txt", ".", 0);
    
    TEST_PATH_PARENT("", ".", 1);
    TEST_PATH_PARENT(".", ".", 0);
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

void test_objects() {
    char path[] = "notes.md";
    char *hash;
    git_obj_blob *blob, *blob2;

    fs_fileinfo fileinfo;
    FILE *fptr;

    if (fs_getinfo(path, &fileinfo) == -1) {
        perror("Could not get file info");
        exit(EXIT_FAILURE);
    }

    if ((fptr = fs_fopen(path, "rb")) == NULL) {
        perror("Could not open file");
        exit(EXIT_FAILURE);
    }

    blob = create_blob(fileinfo.fi_size, fptr);
    assert(blob != NULL);
    ASSERT_STREQ(blob->type, "blob");
    hash = blob->hash;
    printf("hash of blob: %s\n", hash);
    fs_fclose(fptr);

    assert(write_blob_to_disk(blob) == 0);
    
    blob2 = read_blob_from_disk(hash);
    assert(blob2 != NULL);
    ASSERT_STREQ(blob2->hash, hash)
    assert(blob2->size == blob->size);

    assert(create_file_from_blob("build/notes.md", blob2) == 0);
    assert(fs_file_exists("build/notes.md") == 1);
    printf("================BLOB TESTS PASSED=============\n");

    char path2[] = "./include";
    git_obj_tree *tree, *tree2;

    tree = create_tree(path2);
    assert(tree != NULL);
    assert(tree->num_entries > 0);
    printf("hash of tree: %s\n", tree->hash);

    assert(write_tree_to_disk(tree) == 0);

    tree2 = read_tree_from_disk(tree->hash);
    assert(tree2 != NULL);
    ASSERT_STREQ(tree2->hash, tree->hash);
    assert(tree2->num_entries == tree->num_entries);
    assert(tree_cmp(tree, tree2, path) == 0);

    printf("================TREE TESTS PASSED=============\n");

    free_blob(blob);
    free_blob(blob2);
    free_tree(tree);
    // fs_remove("build/notes.md");
}

int main() {
    assert(get_working_repo() == 0);
    
    test_filesystem();
    test_objects();

    printf("Success! All tests passed!\n");
    return 0;
}