#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <ctype.h>

#include "repo.h"
#include "filesystem.h"
#include "objects.h"
#include "dircache.h"
#include "fileinfo.h"
#include "utils.h"
#include "commit.h"
#include "pathspec.h"

#define TEST_REPO_PATH "test/testrepo"

#define faiL_test(fmt, ...) do {                              \
    fprintf(stderr, "Test Failed: " fmt "\n", ##__VA_ARGS__); \
    exit(EXIT_FAILURE);                                       \
} while(0);

void assert_str_equals(const char *actual, const char *expected) {
    if (strcmp(actual, expected) != 0) {
        faiL_test("Expected: %s, but got: %s\n", expected, actual);
    }
}

void mkdir_rel(const char *path, const char *name) {
    static char temp[PATH_MAX];
    fs_path_join(path, name, temp);
    assert(fs_mkdir(temp, 0700) != -1);
}

// set contents to NULL to add filler data to file
void mkfile_rel(const char* path, const char *name, const char *contents) {
    static char temp[PATH_MAX];
    time_t now;
    FILE *fptr;

    fs_path_join(path, name, temp);
    fptr = sfopen(temp, "w");
    if (contents == NULL) {
        time(&now);
        sfputs(ctime(&now), fptr, temp);
    } else {
        sfputs(contents, fptr, temp);
    }
    fclose(fptr);
}

void test_repo_setup(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        faiL_test("Test folder '%s' does not exist", path);
    }
    char full_path[PATH_MAX];
    assert(fs_path_abs(path, full_path) != -1);
    create_repo_folder(full_path);

    mkdir_rel(full_path, "src");
    mkdir_rel(full_path, "src/utils");
    mkdir_rel(full_path, "src/utils/bob");
    mkdir_rel(full_path, "src/utils/alice");
    mkdir_rel(full_path, "src/components");
    mkdir_rel(full_path, "src/core");
    mkdir_rel(full_path, "src/core/app");
    mkdir_rel(full_path, "src/core/app/frontend");
    mkdir_rel(full_path, "test");
    mkdir_rel(full_path, "bin");
    mkfile_rel(full_path, ".gorditignore", "bin\n**/*.exe\nsrc/**/*.log");
    mkfile_rel(full_path, "README.md", NULL);
    mkfile_rel(full_path, "LICENSE", NULL);
    mkfile_rel(full_path, "src/index.html", NULL);
    mkfile_rel(full_path, "src/app.js", NULL);
    mkfile_rel(full_path, "src/profile.js", NULL);
    mkfile_rel(full_path, "src/page.js", NULL);
    mkfile_rel(full_path, "src/help.txt", NULL);
    mkfile_rel(full_path, "src/utils/profile-utils.js", NULL);
    mkfile_rel(full_path, "src/utils/profile-utils.c", NULL);
    mkfile_rel(full_path, "src/utils/profile.js", NULL);
    mkfile_rel(full_path, "src/utils/bob/a.txt", NULL);
    mkfile_rel(full_path, "src/utils/bob/f.log", NULL);
    mkfile_rel(full_path, "src/utils/alice/aslkdfjs.log", NULL);
    mkfile_rel(full_path, "src/utils/alice/hack.exe", NULL);
    mkfile_rel(full_path, "src/utils/alice/a-1.cs", NULL);
    mkfile_rel(full_path, "src/utils/alice/a-2.cs", NULL);
    mkfile_rel(full_path, "src/components/a-3.cs", NULL);
    mkfile_rel(full_path, "src/components/test.exe", NULL);
    mkfile_rel(full_path, "src/core/unique.js", NULL);
    mkfile_rel(full_path, "src/core/app/frontend.js", NULL);
    mkfile_rel(full_path, "src/core/app/cheese.c", NULL);
    mkfile_rel(full_path, "src/core/app/.gorditignore", "frontend/*.c\n**/unique.js");
    mkfile_rel(full_path, "src/core/app/frontend/cheese.c", NULL);
    mkfile_rel(full_path, "test/index.html", NULL);
    mkfile_rel(full_path, "test/test.js", NULL);
    mkfile_rel(full_path, "bin/garbage.js", NULL);
    mkfile_rel(full_path, "bin/out.log", NULL);
}

void test_filesystem() {
    char out[PATH_MAX];

    #define TEST_PATH_PARENT(in, exp, ok) do { \
        int res = fs_path_dirname(in, out);    \
        assert(res == ok);                     \
        assert_str_equals(out, exp);           \
    } while (0)

    #define TEST_PATH_BASENAME(in, exp) do { \
        fs_path_basename(in, out);           \
        assert_str_equals(out, exp);         \
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
    assert_str_equals(blob2->hash, hash);
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
    assert_str_equals(tree2->obj.hash, tree->obj.hash);
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
    assert_str_equals(commit2->tree_hash, commit->tree_hash);
    assert_str_equals(commit2->msg, commit->msg);
    assert_str_equals(commit2->author_name, commit->author_name);
    assert_str_equals(commit2->author_email, commit->author_email);
    assert(commit2->num_parents == commit->num_parents);
    assert_str_equals(commit2->parents[0], commit->parents[0]);
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

int same_path(const char *path1, const char *path2) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    if (len1 != len2) {
        return 0;
    }

    for (size_t i = 0; i < len1; i++) {
        if (path1[i] == '/' || path2[i] == '\\') {
            if (path2[i] != '/' && path2[i] != '\\') {
                return 0;
            }
        } else {
            if (tolower(path1[i]) != tolower(path2[i])) {
                return 0;
            }
        }
    }
    return 1;
}
void test_pathspec_files(const git_repo *repo, const char *pathspec_arg, const char *const *expected, int exp_size) {
    git_file_list *files = git_fl_init();
    pathspec_item *pathspec = pathspec_parse(repo, NULL, pathspec_arg);
    
    int ret = git_fl_working_tree_files(files, repo, pathspec, 1);
    pathspec_free(pathspec);

    int pass = 1;
    if ((ret = 0 && exp_size) || (files->size != exp_size)) {
        pass = 0;
    } else {
        for (int i = 0; i < files->size; i++) {
            int found = 0;
            for (int j = 0; j < exp_size; j++) {
                if (same_path(files->items[i]->name, expected[j])) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("extra: %s\n", files->items[i]->name);
                pass = 0;
                break;
            }
        }
    }

    if (!pass) {
        printf("got (%d): ", files->size);
        for (int i = 0; i < files->size; i++) {
            fprintf(stderr, "%s ", files->items[i]->name);
        }
        printf("\nexpected (%d): ", exp_size);
        for (int i = 0; i < exp_size; i++) {
            fprintf(stderr, "%s", expected[i]);
        }
        printf("\n");
        faiL_test("pathspec %s matched wrong files", pathspec_arg);
    }

    git_fl_free(files);
}

void test_pathspec(const git_repo *repo) {
    #define TEST_PS_PARSE(pathspec, name, exp) do {                            \
        pathspec_item *ps = pathspec_parse(repo, NULL, (pathspec));            \
        int pass = pathspec_full_matches(ps, (name)) == (exp);                 \
        if (!pass) {                                                           \
            printf("Expected %s == %s to eq %d\n", (name), (pathspec), (exp)); \
            assert(0);                                                         \
        }                                                                      \
        pathspec_free(ps);                                                     \
    } while (0)
    
    #define TEST_PS_MATCH(pathspec, ...) \
        test_pathspec_files(repo, (pathspec), __VA_ARGS__, sizeof(__VA_ARGS__)/sizeof(const char *))
    
    // single depth TCs    
    TEST_PS_PARSE("notes.md1", "notes.md", 0);
    TEST_PS_PARSE("notes.m", "notes.md", 0);
    TEST_PS_PARSE("notes.md", "notes.md", 1);
    TEST_PS_PARSE("[a-z]otes.md", "notes.md", 1);
    TEST_PS_PARSE("[a-m]otes.md", "notes.md", 0);
    TEST_PS_PARSE("[!a-m]otes.md", "notes.md", 1);
    TEST_PS_PARSE("note?.md", "notes.md", 1);
    TEST_PS_PARSE("notes?.md", "notes.md", 0);

    TEST_PS_PARSE("a/b/c", "a/b", 0);
    TEST_PS_PARSE("a/b/c", "a/b/c.txt", 0);
    TEST_PS_PARSE("a/b/c", "a/b/c/d.log", 1);
    TEST_PS_PARSE("a/bob/c", "a/b/c/d.log", 0);
    TEST_PS_PARSE("**", "src/a/b/notes.md", 1);
    TEST_PS_PARSE("**/*.md", "src/total/notes.md", 1);
    TEST_PS_PARSE("**/*.md", "src/total/notes", 0);
    TEST_PS_PARSE("a/**/*.md", "src/total/notes.md", 0);
    TEST_PS_PARSE("src/**/*.md", "src/total/notes.md", 1);
    TEST_PS_PARSE("src/**/bob/*.md", "src/total/notes.md", 0);
    TEST_PS_PARSE("src/**/bob/*.md", "src/total/a/bob/notes.md", 1);
    TEST_PS_PARSE("**/src/*.md", "src/total/a/bob/notes.md", 0);
    TEST_PS_PARSE("**/src/**/*.md", "src/total/a/bob/notes.md", 1);
    TEST_PS_PARSE("notes.md", "notes.md", 1);
    TEST_PS_PARSE(".", "notes.md", 1);
    TEST_PS_PARSE("**/**", "a/b/c.txt", 1);

    TEST_PS_MATCH("README.md", (const char *const[]){ "README.md" });
    TEST_PS_MATCH("test", (const char *const[]){ "test/index.html", "test/test.js" });
    TEST_PS_MATCH("src/*.js", (const char *[]){ "src/app.js", "src/profile.js", "src/page.js" });
    TEST_PS_MATCH("src/**/*.js", (const char *const[]){ "src/app.js", "src/profile.js", "src/page.js", 
        "src/utils/profile-utils.js", "src/utils/profile.js", "src/core/unique.js", "src/core/app/frontend.js" });
    test_pathspec_files(repo, "src/frontend.js", NULL, 0);
    TEST_PS_MATCH("src/**/a-[1-3].cs", (const char *[]){ "src/utils/alice/a-1.cs",  "src/utils/alice/a-2.cs", "src/components/a-3.cs" });
    TEST_PS_MATCH("src/core/**/cheese.c", (const char *const[]){ "src/core/app/cheese.c" });
    TEST_PS_MATCH("src/core/**/**/**/cheese.c", (const char *const[]){ "src/core/app/cheese.c" });
    TEST_PS_MATCH("**/src/core/**/**/**/cheese.c", (const char *const[]){ "src/core/app/cheese.c" });
    test_pathspec_files(repo, "**/src/core/**/**/**/cheese.c/**", NULL, 0);
    
    // test ignores
    TEST_PS_MATCH("src/core/unique.js", (const char *const[]){ "src/core/unique.js" });
    test_pathspec_files(repo, "bin", NULL, 0);
    test_pathspec_files(repo, "**/*.log", NULL, 0);

    printf("================PATHSPEC TESTS PASSED=============\n");
}

int main() {
    test_repo_setup(TEST_REPO_PATH);
    chdir(TEST_REPO_PATH);

    const git_repo *repo = git_repo_init();
    assert(repo != NULL && "Cannot get repo"); 

    test_filesystem();
    // test_objects(repo);
    // test_index(repo);
    test_pathspec(repo);

    free((void *)repo);
    printf("Success! All tests passed!\n");
    return 0;
}