# qt-resource-rebuilder

This extension lets you rebuild the QT resources (e.g., icons, fonts, QML files which dictate the UI) loaded by an application on the fly. By rebuilding QT resources loaded by Xochitl, reMarkable's main application, on the fly as XOVI starts up, qt-resource-rebuilder enables UI modifications.  

qt-resource-rebuilder reads modification files in `$XOVI_EXTHOME/exthome/qt-resource-rebuilder` when XOVI starts and modifies the application's UI accordingly.

Note: By default, `$XOVI_EXTHOME` is `/home/root/xovi` (i.e., the location where XOVI is installed by default).

There are three types of modification files that qt-resource-rebuilder supports: `.qrr` files for replacing existing QT resources, `.qmd` files for modifying exsting QT resources, and `.rcc` files for adding new resources.

Note: Xochitl's frontend elements change from version to version; that means that a mod file that works for one version (particularly `.qmd` files, which build on Xochitl's frontend elements) may not work for the next.

## .qrr files
`.qrr` files are used to wholesale replace an existing QT resource file in Xochitl with a local file (e.g., if you wanted to replace all occurrences of a specific icon with a new icon of your choice).

QRR files follow the format:

```
R <resource path within Xochitl> <local file path>
R <resource path within Xochitl 2> <local file path 2>
...
```
Note: Both absolute and relative paths are supported for local file path (relative paths' base is `$XOVI_EXTHOME/exthome/qt-resource-rebuilder`)

For example:
```
R /images/test.png test.png
R /images/example.qml /home/root/example.qml
``` 
## .rcc files
RCC files let you add new resources - QML files, images, etc... Simply compile a new `.rcc` file from a `.qrc` file with that enumerates the QT resources you want added. Learn more about `.qrc` and `.rcc` files [here](https://doc.qt.io/qt-6/resources.html).

## .qmd files
`.qmd` files are written in [QMLDiff](https://github.com/asivery/qmldiff) language and are meant to partially modify the QML files that define Xochitl's UI, enabling new functionalities or minor UI modificiations while keeping the tablet's UI largely intact. For more about how to write `.qmd` files, please refer to the [QMLDiff repo](https://github.com/asivery/qmldiff?tab=readme-ov-file#the-diff-language).

### (Re)building hashtable
`.qmd`s made available by the community of reMarkable modders will be ["hashed"](https://github.com/asivery/qmldiff?tab=readme-ov-file#hashing) instead of referencing QML files and their contents by their actual names. This is by design. Thus, to ensure that qt-resource-rebuilder knows how to actually apply the `.qmd` files, there has to be a version-specific hashtable that lets qt-resource-rebuilder translate between the hashes and the actual QML elements.

The script to build this hashtable is included in the installation of XOVI, at `$XOVI_EXTHOME/rebuild_hashtable`. Be sure to run the script after installing qt-resource-rebuilder for the first time, updating qt-resource-rebuilder, or updating the tablet's OS.

### Please remember to clear your qmlcache directory after creating / deleting qrr files, to make sure no cached versions are loaded

`QML_DISABLE_DISK_CACHE=1` is your friend; when qt-resource-rebuilder is installed, this environment variable is set by default when starting XOVI.

## Hot reload (runtime diff replacement)

Normally modifications apply once, as Xochitl registers its QT resources at startup. qt-resource-rebuilder also exports a runtime path so another extension can replace a loaded `.qmd` and have the affected resources re-registered live, without restarting Xochitl:

```c
// Replace one external diff (by id) and re-register only the resource roots
// that hold the files it targets. Returns the number of roots reloaded, or -1
// if the new diff failed to parse (the previous diff stays applied).
int qrr_reload_external_diff(const char *id, const char *contents);

// Re-register every tracked root without changing any diff.
int qrr_reload_all_roots(void);
```

Every registered resource root is remembered together with its pristine originals, so a reload always rebuilds from the same bytes a cold start would use; on the QMLDiff side the merged diff state is re-derived from the pristine diffs, so slots and templates re-resolve correctly on every replace (see [docs/HOT_RELOAD.md](docs/HOT_RELOAD.md) and QMLDiff's own `docs/HOT_RELOAD.md`).

Note: this swaps the *resources*. Making the change visible on screen (clearing the QML component cache and re-instantiating the affected items) is the caller's job. A minimal end-to-end example — drop a `.qmd` into a folder on the tablet and watch the toolbar change live — lives in [examples/hot-reload](examples/hot-reload).
