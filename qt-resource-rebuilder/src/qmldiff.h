#pragma once
extern int qmldiff_build_change_files(const char *rootDir);
extern char *qmldiff_process_file(const char *fileName, char *contents, size_t contentsLength);
extern char qmldiff_is_modified(const char *fileName);
extern char qmldiff_add_external_diff(const char *contents, const char *id);
extern void qmldiff_start_saving_thread();
extern void qmldiff_load_rules(const char *rules);
extern void qmldiff_set_version(const char *version);

// Hot reload (see qmldiff/src/lib.rs). replace_external_diff swaps a diff
// previously loaded under `id`; targets_of returns the newline-joined qrc paths
// that diff modifies (free with qmldiff_free_string).
extern char qmldiff_replace_external_diff(const char *contents, const char *id);
extern char *qmldiff_targets_of(const char *id);
extern void qmldiff_free_string(char *pointer);
