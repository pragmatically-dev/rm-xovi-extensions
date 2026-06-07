# Hot reload

This lets a `.qmd` be re-applied while Xochitl is running — no `.so` rebuild, no
restart. It pairs with the qmldiff exports of the same name (see
`qmldiff/docs/HOT_RELOAD.md`).

## What changed

Normally the `qRegisterResourceData` override applies every modification once, at
startup. Two additions make that repeatable:

1. **A root registry** (`hotreload.c`). Every root the override forwards is
   recorded with its **pristine originals** (Xochitl's static qrc memory, which
   stays valid for the process lifetime) and the buffers currently live.
2. **`qUnregisterResourceData`** (imported, optional). To reload, each tracked
   root is unregistered, rebuilt from the pristine originals through the
   now-updated qmldiff, and registered again.

The rebuild logic the override always used is factored into
`rebuildResourceRoot()` (in `main.c`) and shared with the reload path, so a
reload rebuilds from the same pristine bytes rather than cumulatively.

## Exports

```c
// Replace one external diff (by id) and re-register the affected roots.
// Returns the number of roots reloaded, or -1 if the new diff failed to parse
// (previous resources are kept).
int qrr_reload_external_diff(const char *id, const char *contents);

// Re-register every tracked root without changing any diff (e.g. after a batch
// of qmldiff_replace_external_diff calls).
int qrr_reload_all_roots(void);
```

After this returns, the caller still needs to invalidate the live UI — clear the
QML component cache and re-instantiate the affected items. That part lives in the
calling tool, not here, to keep this extension focused on resources.

## Ownership & safety

- Originals are **never freed**. Rebuilt buffers are owned by the registry
  (`modified` flag) and freed exactly once, on the next reload of that root.
- A root can flip modified/unmodified across reloads (a new diff adds/removes a
  target); the flag tracks ownership through the transition.
- Everything runs under `mainMutex` (now shared across translation units), so a
  reload can't race the register override.
- If `qUnregisterResourceData` isn't present on the device's Qt, reload is a
  no-op and logs — it never registers duplicates.

## Status

First cut re-registers **all** tracked roots on any change (correct, slightly
coarse). `qmldiff_targets_of(id)` is available to narrow this to only the roots
containing a changed diff's target files — a planned optimisation. Build with the
toolchain `make`; validate on-device (see the workspace plan).
