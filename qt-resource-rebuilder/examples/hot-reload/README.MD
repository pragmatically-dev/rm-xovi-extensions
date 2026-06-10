# Hot-reload PoC

Drop a `.qmd` into a folder on the tablet — see the change on screen, live,
no xochitl restart. This is the smallest possible demonstration of the
hot-reload exports added to qt-resource-rebuilder on this branch.

```
your machine                              tablet (xochitl running)
────────────                              ────────────────────────
edit demo/tag-icon.qmd
scp it to /home/root/hot-reload/  ──────► hot-reload-poc (QFileSystemWatcher)
                                              │ qrr_reload_external_diff(id, qmd)
                                              │   └─ qmldiff swaps the diff, then only
                                              │      the affected qrc roots re-register
                                              │ clearComponentCache
                                              │ re-fire the affected URL-Loaders
                                              ▼
                                          the Tags tool icon changes ON SCREEN
```

## Prebuilt binaries (rM2 / armv7)

If you just want to try it, the
[hot-reload-poc release](https://github.com/pragmatically-dev/rm-xovi-extensions/releases/tag/hot-reload-poc)
ships both binaries built from this branch with the eeems/remarkable-toolchain:

- `qt-resource-rebuilder.so` — this branch's extension (the hot-reload exports,
  with its qmldiff built from the companion `qmldiff#hot-reload` branch)
- `hot-reload-poc.so` — this example

Drop both into xovi's `extensions.d/` (back up your current
`qt-resource-rebuilder.so` first) and continue at "Run" below.

## Build

Same environment as the rest of the repo (cross toolchain + Qt 6 + a clone of
[xovi](https://github.com/asivery/xovi) for xovigen):

```bash
cd qt-resource-rebuilder/examples/hot-reload
export XOVI_REPO=/path/to/xovi
qmake6 && make
```

## Run (on a reMarkable with xovi + this branch's qt-resource-rebuilder)

1. Copy `hot-reload-poc.so` into xovi's `extensions.d/` and restart xochitl
   (with `QML_DISABLE_DISK_CACHE=1`, as qt-resource-rebuilder already requires).
   The log shows: `[hot-reload-poc] watching /home/root/hot-reload`.
2. Open any document so the toolbar is on screen.
3. From your machine:

```bash
scp demo/tag-icon.qmd root@10.11.99.1:/home/root/hot-reload/
```

The Tags tool icon turns into the cloud-upload icon, live. Edit the icon name
inside the `.qmd` (`eye`, `cog`, `keyboard`, ...) and scp again — it changes
again, every time, with no restart. A diff that fails to parse is rejected and
the previous one stays applied (check stderr).

The diff id is the file name, so re-copying the same file *replaces* that diff
(slots and templates re-resolve from pristine state — see
`qmldiff/docs/HOT_RELOAD.md`); different file names load as additional diffs.

## What this PoC is (and is not)

- It demonstrates the **resource layer**: live diff replacement, targeted root
  re-registration, repeatability.
- The visible update covers components loaded through **URL-`source` Loaders**
  (toolbar tools/menus — `TagsMenu.qml`, `SettingsMenu.qml`, every `*Menu.qml`):
  ~150 such loaders are live in a running xochitl.
- Content inlined into long-lived views (e.g. `DocumentView` internals) swaps
  at the resource level but needs its mount restructured behind a reloadable
  Loader to update visibly — that belongs to the *caller* of these exports and
  is intentionally out of scope here (see `docs/HOT_RELOAD.md`).

~210 lines of C++, no protocol, no daemon on the host: just a folder watch.
