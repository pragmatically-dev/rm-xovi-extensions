#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
    Hot reload support.

    qt-resource-rebuilder normally applies every modification once, as Xochitl
    registers its Qt resource roots at startup. To reload a `.qmd` without
    restarting, we have to:
      1. remember every root we forwarded to qRegisterResourceData, keeping the
         pristine originals (Xochitl's static qrc memory) around, and
      2. when a diff changes, qUnregisterResourceData the affected roots, rebuild
         them from the originals through the (now updated) qmldiff, and
         qRegisterResourceData them again.

    The UI invalidation (clearing the QML component cache and re-instantiating the
    live items) is done by the caller after qrr_reload_external_diff returns.
*/

// Defined in main.c. Rebuild a root from pristine originals applying all current
// modifications. *outTree/*outData are freshly malloc'd when modified, else they
// alias the originals (do not free). Returns whether anything was modified.
bool rebuildResourceRoot(uint8_t *origTree, uint8_t *origName, uint8_t *origData,
                         uint8_t **outTree, uint8_t **outData);

// Defined in main.c. Whether the qrc trie rooted at tree/name contains `path`
// (a full qrc path as qmldiff reports it). Lets a reload re-register only the
// roots a changed diff actually targets.
bool rootContainsPath(uint8_t *tree, uint8_t *name, const char *path);

// Record a root the register override has forwarded. originals must stay valid
// for the process lifetime (they are Xochitl's static qrc memory).
void hrRegisterRoot(int version, uint8_t *origTree, uint8_t *origName, uint8_t *origData,
                    uint8_t *curTree, uint8_t *curData, bool modified);

// Re-register every tracked root from its pristine originals. Returns the number
// of roots re-registered, or 0 if qUnregisterResourceData is unavailable. Caller
// must hold mainMutex.
int hrReloadAllRoots(void);

// Exported (see qt-resource-rebuilder.xovi). Replace one external diff (by id)
// and re-register the affected roots. Returns the number of roots reloaded, or
// -1 if the new diff failed to parse (previous resources are kept). Takes the
// lock itself.
int qrr_reload_external_diff(const char *id, const char *contents);

// Exported. Re-register every tracked root without changing any diff (useful
// after a batch of qmldiff_replace_external_diff calls). Takes the lock itself.
int qrr_reload_all_roots(void);
