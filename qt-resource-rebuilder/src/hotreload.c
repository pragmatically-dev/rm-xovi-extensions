#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include "types.h"
#include "qmldiff.h"
#include "hotreload.h"
#include "utlist.h"
#include "../../util.h"
#include "../xovi.h"

/*
    One record per Qt resource root we have forwarded to qRegisterResourceData.
    `orig*` point at Xochitl's static qrc memory and are never freed. `cur*` is
    whatever is currently live: it equals the originals when we didn't modify the
    root, otherwise it is our malloc'd rebuild (tracked by `modified`, which tells
    us whether we own it and must free it on the next reload).
*/
struct RegisteredRoot {
    int version;
    uint8_t *origTree, *origName, *origData;
    uint8_t *curTree, *curData;
    bool modified;

    struct RegisteredRoot *next, *prev;
};

static struct RegisteredRoot *ROOTS = NULL;

void hrRegisterRoot(int version, uint8_t *origTree, uint8_t *origName, uint8_t *origData,
                    uint8_t *curTree, uint8_t *curData, bool modified) {
    struct RegisteredRoot *root = malloc(sizeof(struct RegisteredRoot));
    root->version = version;
    root->origTree = origTree;
    root->origName = origName;
    root->origData = origData;
    root->curTree = curTree;
    root->curData = curData;
    root->modified = modified;
    DL_APPEND(ROOTS, root);
}

// Re-register one root: drop whatever is currently live, then rebuild it fresh
// from the pristine originals through the (now updated) qmldiff. Caller must hold
// mainMutex and have checked that qUnregisterResourceData is available.
static void reloadOneRoot(struct RegisteredRoot *root) {
    // Drop whatever is currently live for this root...
    $_Z23qUnregisterResourceDataiPKhS0_S0_(root->version, root->curTree, root->origName, root->curData);
    if(root->modified) {
        free(root->curTree);
        free(root->curData);
    }

    // ...and rebuild it fresh from the pristine originals.
    uint8_t *newTree, *newData;
    bool modified = rebuildResourceRoot(root->origTree, root->origName, root->origData,
                                        &newTree, &newData);
    root->curTree = newTree;
    root->curData = newData;
    root->modified = modified;

    $_Z21qRegisterResourceDataiPKhS0_S0_(root->version, newTree, root->origName, newData);
}

int hrReloadAllRoots(void) {
    if(!$_Z23qUnregisterResourceDataiPKhS0_S0_) {
        fprintf(stderr, "[%s]: qUnregisterResourceData is unavailable - cannot hot reload.\n", NAME);
        return 0;
    }

    int reloaded = 0;
    struct RegisteredRoot *root;
    DL_FOREACH(ROOTS, root) {
        reloadOneRoot(root);
        reloaded++;
    }
    LOG("[%s]: Hot reload re-registered %d resource root(s).\n", NAME, reloaded);
    return reloaded;
}

// Whether the qrc trie rooted at tree/name contains any of the newline-separated
// `targets` paths (as returned by qmldiff_targets_of).
static bool rootContainsAnyTarget(uint8_t *tree, uint8_t *name, const char *targets) {
    const char *line = targets;
    char path[512];
    while(*line) {
        int len = 0;
        while(line[len] && line[len] != '\n') len++;
        if(len > 0 && len < (int)sizeof(path)) {
            memcpy(path, line, len);
            path[len] = 0;
            if(rootContainsPath(tree, name, path)) return true;
        }
        line += len;
        while(*line == '\n') line++;
    }
    return false;
}

// Re-register only the roots that contain at least one of the changed diff's
// target files. Falls back to reloading every root when `targets` is NULL or
// empty, so a diff whose targets can't be resolved is never silently skipped.
// Caller must hold mainMutex.
static int hrReloadRootsForTargets(const char *targets) {
    if(!$_Z23qUnregisterResourceDataiPKhS0_S0_) {
        fprintf(stderr, "[%s]: qUnregisterResourceData is unavailable - cannot hot reload.\n", NAME);
        return 0;
    }
    if(targets == NULL || *targets == 0) return hrReloadAllRoots();

    int reloaded = 0;
    struct RegisteredRoot *root;
    DL_FOREACH(ROOTS, root) {
        if(rootContainsAnyTarget(root->origTree, root->origName, targets)) {
            reloadOneRoot(root);
            reloaded++;
        }
    }
    LOG("[%s]: Hot reload re-registered %d resource root(s) for the changed targets.\n", NAME, reloaded);
    return reloaded;
}

int qrr_reload_all_roots(void) {
    pthread_mutex_lock(&mainMutex);
    int reloaded = hrReloadAllRoots();
    pthread_mutex_unlock(&mainMutex);
    return reloaded;
}

int qrr_reload_external_diff(const char *id, const char *contents) {
    pthread_mutex_lock(&mainMutex);
    if(!qmldiff_replace_external_diff(contents, id)) {
        pthread_mutex_unlock(&mainMutex);
        fprintf(stderr, "[%s]: Failed to replace diff '%s' - keeping previous resources.\n", NAME, id);
        return -1;
    }
    // Re-register only the roots holding the files this diff targets.
    char *targets = qmldiff_targets_of(id);
    int reloaded = hrReloadRootsForTargets(targets);
    if(targets) qmldiff_free_string(targets);
    pthread_mutex_unlock(&mainMutex);
    return reloaded;
}
