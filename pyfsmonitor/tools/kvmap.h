#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "hashmap.h"

#ifndef KVMAP_H
#define KVMAP_H

typedef struct hashmap* hashmap;

typedef struct _key_value {
    char *key;
    void* value;
} key_value;


int compare(const void *a, const void *b, void *udata);

uint64_t hash(const void *item, uint64_t seed0, uint64_t seed1);

#define CREATE_KEY_VALUE_HASHMAP(name) \
    name = hashmap_new(sizeof(key_value), 0, 0, 0, \
                        hash, compare, NULL, NULL);

#define CAST_VALUE(kvname, _type) \
        (_type)(kvname->value)
        
#endif