#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <openssl/sha.h>

#include "hash.h"

void copy_hash(obj_hash *out, const obj_hash *in) {
    snprintf(*out, OBJ_HASH_SIZE, "%s", *in);
}

void string_to_hash(obj_hash *out, const char *in) {
    assert(strlen(in) < sizeof(obj_hash));
    copy_hash(out, (obj_hash *)in);
}

int hash_eq(const obj_hash a, const obj_hash b) {
    return strcmp(a, b) == 0;
}

void hash_from_bytes(const unsigned char *bytes, obj_hash *out_hash) {
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        snprintf(*out_hash + (i << 1), OBJ_HASH_SIZE, "%02x", bytes[i]);
    } 
}

void hash_to_bytes(const obj_hash hash, unsigned char *out_bytes) {
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        char hex[3] = {hash[i << 1], hash[(i << 1) + 1], '\0'};
        sscanf(hex, "%2hhx", &out_bytes[i]);
    }
}

void hash_data(unsigned char *data, size_t size, obj_hash *o_hash) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(data, size, hash);
    hash_from_bytes(hash, o_hash);
}

