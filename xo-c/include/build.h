#ifndef XO_BUILD_H
#define XO_BUILD_H

#include "xo.h"

// Build cache entry
typedef struct {
    char *filepath;
    char *hash;
} xo_build_cache_entry_t;

// Build cache
typedef struct {
    xo_build_cache_entry_t *entries;
    size_t count;
    size_t capacity;
} xo_build_cache_t;

// Dependency tracking
typedef struct {
    char *filepath;
    char **dependencies;
    size_t dep_count;
    size_t dep_capacity;
} xo_dependency_entry_t;

// Dependency tracker
typedef struct {
    xo_dependency_entry_t *entries;
    size_t count;
    size_t capacity;
} xo_dependency_tracker_t;

// Function declarations
int xo_build_cache_init(xo_build_cache_t *cache);
void xo_build_cache_free(xo_build_cache_t *cache);
int xo_build_cache_add(xo_build_cache_t *cache, const char *filepath, const char *hash);
const char *xo_build_cache_get(const xo_build_cache_t *cache, const char *filepath);
int xo_build_cache_save(const xo_build_cache_t *cache, const char *cache_path);
int xo_build_cache_load(xo_build_cache_t *cache, const char *cache_path);

int xo_dependency_tracker_init(xo_dependency_tracker_t *tracker);
void xo_dependency_tracker_free(xo_dependency_tracker_t *tracker);
int xo_dependency_tracker_add(xo_dependency_tracker_t *tracker, const char *filepath, const char *dependency);
char **xo_dependency_tracker_get_reverse(const xo_dependency_tracker_t *tracker, const char *dependency, size_t *count);
int xo_dependency_tracker_save(const xo_dependency_tracker_t *tracker, const char *filepath);
int xo_dependency_tracker_load(xo_dependency_tracker_t *tracker, const char *filepath);

int xo_build_file(const xo_config_t *config, const char *filepath, xo_dependency_tracker_t *tracker);
int xo_build_directory(const xo_config_t *config, const char *dirpath, xo_dependency_tracker_t *tracker);
int xo_compute_file_hash(const char *filepath, char **hash);
bool xo_should_rebuild(const xo_build_cache_t *cache, const char *filepath);

#endif /* XO_BUILD_H */ 