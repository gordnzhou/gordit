#include <time.h>
#include <string.h>

#include "commit.h"
#include "refs.h"
#include "tree.h"
#include "utils.h"
#include "logging.h"

void free_commit(git_obj_commit *commit) {
    free(commit->parents);
    free(commit->author_name);
    free(commit->author_email);
    free(commit->msg);
    free(commit);
}

git_obj_commit *create_commit(git_obj_tree *tree, 
    int num_parents, 
    obj_hash *parents, 
    const char *author_name, 
    const char *author_email,
    const char *msg) {
    
    git_obj_commit *commit = smalloc(sizeof(*commit));
    commit->timestamp = time(NULL);
    copy_hash(&(commit->tree_hash), &(tree->obj.hash));
    commit->author_name = sstrdup(author_name);
    commit->author_email = sstrdup(author_email);
    commit->msg = sstrdup(msg);
    commit->num_parents = num_parents;
    commit->parents = calloc(num_parents, sizeof(obj_hash));
    for (int i = 0; i < num_parents; i++) {
        copy_hash(commit->parents + i, &(parents[i]));
    }

    return commit;
}

size_t serialize_person_and_timestamp(char *out, size_t size, const char *name, const char *email, time_t timestamp) {
    return snprintf(out, size, "%s <%s> %lld\n", name, email, (long long)timestamp);
}

time_t deserialize_person_and_timestamp(const char *line, char **name, char **email) {
    char *e_start = strstr(line, " <");
    char *e_end = strstr(line, "> "); 
    *name = sstrndup(line, e_start - line);
    *email = sstrndup(e_start + 2, e_end - e_start - 2); 
    return atoll(e_end + 2);
}

char *serialize_commit(int *out_size, const git_obj_commit *commit) {
    size_t content_size = 0;
    size_t buf_len = strlen(commit->msg) 
        + strlen(commit->author_name) + strlen(commit->author_email) + 100 
        + (commit->num_parents + 1) * (sizeof(obj_hash) + 15);

    char *buf = smalloc(buf_len);

    content_size += snprintf(buf, buf_len, "tree: %s\nauthor: ", commit->tree_hash);
    content_size += serialize_person_and_timestamp(
        buf + content_size, buf_len,
        commit->author_name, commit->author_email, commit->timestamp);

    for (int i = 0; i < commit->num_parents; i++) {
        content_size += snprintf(buf + content_size, buf_len - content_size, "parent: %s\n", commit->parents[i]);
    }
    content_size += snprintf(buf + content_size, buf_len - content_size, "\n%s", commit->msg);
    assert(content_size < buf_len);

    *out_size = content_size;
    return buf;
}

git_obj_commit *deserialize_commit(char *in_str) {
    git_obj_commit *commit = smalloc(sizeof(*commit));
    commit->num_parents = 0;
    commit->parents = NULL;
     
    char *message = strstr(in_str, "\n\n");
    if (!message) {
        error("could not parse to commit: no header");
        return NULL;
    }
    commit->msg = sstrdup(message + 2);
    *message = '\0';
    
    char *saveptr_lines = NULL;
    char *line = strtok_r(in_str, "\n", &saveptr_lines);
    while (line != NULL) {
        char *key = line;
        char *value = strchr(line, ':');
        *value = '\0';
        value += 2;
        if (!value) {
            error("could not parse to commit: has invalid line");
            return NULL;
        }

        if (strcmp(key, "tree") == 0) {
            string_to_hash(&(commit->tree_hash), value);
        } else if (strcmp(key, "parent") == 0) {
            commit->num_parents++;
            commit->parents = realloc(commit->parents, commit->num_parents * sizeof(obj_hash));
            string_to_hash(commit->parents + commit->num_parents - 1, value);
        } else if (strcmp(key, "author") == 0) {
            commit->timestamp = deserialize_person_and_timestamp(
                value,
                &(commit->author_name),
                &(commit->author_email));
        } else if (strcmp(key, "committer") == 0) {
            // ignore for now
        } else {
            error("could not parse to commit: unknown key '%s'", key);
            return NULL;
        }

        line = strtok_r(NULL, "\n", &saveptr_lines);
    }

    if (strlen(commit->tree_hash) <= 0) {
        error("could not parse to commit: missing tree field");
        return NULL;
    }

    return commit;
}

git_obj *create_commit_obj(const git_obj_commit *commit) {
    int content_size;
    char *buf = serialize_commit(&content_size, commit);

    git_obj *obj = smalloc(sizeof(*obj));
    create_git_obj((unsigned char *)buf, content_size, OBJ_TYPE_COMMIT, obj);
    free(buf);

    return obj;
}

void print_commit(const git_obj_commit *commit) { 
    printf("Author: %s <%s>\n", commit->author_name, commit->author_email);
    printf("Date:   %s\n", ctime(&(commit->timestamp)));
    printf("    %s\n", commit->msg);
}

git_obj_commit *read_commit_from_disk(const git_repo *repo, const obj_hash hash) { 
    git_obj *obj = smalloc(sizeof(*obj));
    read_obj_from_disk(obj, repo, hash, OBJ_TYPE_COMMIT);

    char *contents = obj_content_string(obj);

    git_obj_commit *commit = deserialize_commit(contents);
    if (commit == NULL) {
        fatal("could not parse object %s to commit", hash);
    }

    free(contents);
    free_obj(obj);
    return commit;
}