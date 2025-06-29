#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tools/kvmap.h"

int compare(const void *a, const void *b, void *udata) {
    const key_value *kva = a;
    const key_value *kvb = b;
    return strcmp(kva->key, kvb->key);
}

uint64_t hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const key_value *kv = item;
    return hashmap_sip(kv->key, strlen(kv->key), seed0, seed1);
}