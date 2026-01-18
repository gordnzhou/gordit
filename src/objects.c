#include <objects.h>

struct git_obj_blob *create_blob(const char *filepath);

int write_blob_to_disk(const struct git_obj_blob *, const char *repo_root);

struct git_obj_blob *read_blob_from_disk(char hash[HASH_SIZE], const char *repo_root);

int create_file_from_blob(const char *path, const struct git_obj_blob *);

void free_blob(struct git_obj_blob *);

struct git_obj_tree *create_tree(const char *folderpath);

int write_tree_to_disk(const struct git_obj_tree *, const char *repo_root);

struct git_obj_tree *read_tree_from_disk(char hash[HASH_SIZE]);

int tree_find(const struct git_obj_tree *, char hash[HASH_SIZE], struct git_obj_tree_entry *obj, char *path);

struct git_obj_blob **tree_get_blobs(struct git_obj_tree *, char *regex_filter, int *size);

void free_tree(struct git_obj_tree *);

int delete_obj_from_disk(char obj_hash[HASH_SIZE], const char *repo_root);

int get_obj_contents(char obj_hash[HASH_SIZE], const char *repo_root, char *buf, int buf_size);