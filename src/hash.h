#ifndef GIT_HASH_H
#define GIT_HASH_H

#define OBJ_HASH_SIZE 41
typedef char obj_hash[OBJ_HASH_SIZE];

void hash_from_bytes(const unsigned char *bytes, obj_hash *out_hash);

void hash_to_bytes(const obj_hash hash, unsigned char *out_bytes);

void copy_hash(obj_hash *out, const obj_hash* in);

void string_to_hash(obj_hash *out, const char *in);

// uses SHA1 function from openssl/sha.h
void hash_data(unsigned char *data, size_t size, obj_hash *o_hash);

#endif