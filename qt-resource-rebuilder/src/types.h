#pragma once
#include <stdbool.h>
#include <pthread.h>
#include "uthash.h"
#include "hash.h"

#define NAME "qt-resource-rebuilder"

#define HASH_ADD_HT(head, keyfield_name, item_ptr) HASH_ADD(hh, head, keyfield_name, sizeof(hash_t), item_ptr)
#define HASH_FIND_HT(head, hash_ptr, out) HASH_FIND(hh, head, hash_ptr, sizeof(hash_t), out)

#define TREE_ENTRY_SIZE     22

#define COMPRESSED          0x01
#define DIRECTORY           0x02
#define COMPRESSED_ZSTD     0x04

#define MODIF_REPLACE       1
#define MODIF_INJECT        2
// Defined in main.c. Shared so hotreload.c can take the same lock when it
// re-registers resource roots at runtime.
extern pthread_mutex_t mainMutex;

struct ReplacementEntry {
    int node;

    uint8_t *data;
    uint32_t size;
    uint32_t copyToOffset;
    bool freeAfterwards;

    UT_hash_handle hh;
};

struct InjectionEntryFile {
    char *name;
    uint8_t *data;
    uint32_t size;

    struct InjectionFile *next;
};

struct InjectionEntry {
    int parentNode;

    struct InjectionEntryFile *toInjectHead;

    UT_hash_handle hh;
};

struct ModificationDefinition {
    hash_t entryNameHash;

    int modificationType;
    char *entryName; // Either the directory name to inject to, or the full path of the file to replace.
    union {
        struct InjectionEntry *toInject;
        struct ReplacementEntry *toReplace;
    } action;

    UT_hash_handle hh;
};

