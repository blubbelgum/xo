#ifndef XO_WATCHER_H
#define XO_WATCHER_H

#include "xo.h"
#include "build.h"

// File event type
typedef enum {
    XO_FILE_CREATED,
    XO_FILE_MODIFIED,
    XO_FILE_DELETED
} xo_file_event_type_t;

// File event structure
typedef struct {
    xo_file_event_type_t type;
    char *filepath;
} xo_file_event_t;

// File watcher callback function type
typedef void (*xo_watcher_callback_t)(const xo_file_event_t *event, void *user_data);

// File watcher structure
typedef struct xo_watcher_s {
    void *handle;
    char **watch_paths;
    size_t path_count;
    bool running;
    xo_watcher_callback_t callback;
    void *user_data;
} xo_watcher_t;

// Function declarations
int xo_watcher_init(xo_watcher_t *watcher);
void xo_watcher_free(xo_watcher_t *watcher);
int xo_watcher_add_path(xo_watcher_t *watcher, const char *path);
int xo_watcher_start(xo_watcher_t *watcher, xo_watcher_callback_t callback, void *user_data);
int xo_watcher_stop(xo_watcher_t *watcher);

// File event handling
void xo_handle_file_event(const xo_file_event_t *event, void *user_data);

#endif /* XO_WATCHER_H */ 