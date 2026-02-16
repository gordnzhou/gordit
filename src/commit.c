#include <time.h>

#include "commit.h"
#include "utils.h"

git_obj_commit *create_commit(git_obj_tree *tree, 
    int num_parents, 
    obj_hash *parents, 
    char *author_name, char *author_email,
    char *msg) {
    
    // TODO: for now assume lifetime of parameters are all larger than commit
    git_obj_commit *commit = smalloc(sizeof(*commit));
    commit->timestamp = time(NULL);
    commit->tree = tree;
    commit->author_name = author_name;
    commit->author_email = author_email;
    commit->msg = msg;
    commit->num_parents = num_parents;
    commit->parents = parents; 

    return commit;
}

git_obj *create_commit_obj(const git_obj_commit *commit) {
    git_obj *obj = smalloc(sizeof(*obj));

    size_t content_size = 0;
    size_t buf_len = strlen(commit->msg) 
        + strlen(commit->author_name) + strlen(commit->author_email) + 100 
        + (commit->num_parents + 1) * (OBJ_HASH_SIZE + 15);

    char *buf = smalloc(buf_len);

    content_size += snprintf(buf, buf_len, "tree: %s\nauthor: %s <%s> %lld\n", 
        commit->tree->obj.hash, 
        commit->author_name, commit->author_email, (long long)commit->timestamp);
    for (int i = 0; i < commit->num_parents; i++) {
        content_size += snprintf(buf + content_size, buf_len - content_size, "parent: %s\n", commit->parents[i]);
    }
    content_size += snprintf(buf + content_size, buf_len - content_size, "\n%s", commit->msg);
    buf[content_size] = '\0';
    assert(content_size < buf_len);

    create_git_obj((unsigned char *)buf, content_size, O_TYPE_COMMIT, obj);

    free(buf);

    return obj;
}
