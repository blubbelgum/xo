#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watcher.h"
#include "utils.h"

// Initialize a file watcher
int xo_watcher_init(xo_watcher_t *watcher) {
    if (!watcher) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    watcher->handle = NULL;
    watcher->watch_paths = NULL;
    watcher->path_count = 0;
    watcher->running = false;
    watcher->callback = NULL;
    watcher->user_data = NULL;
    
    return XO_SUCCESS;
}

// Free resources used by a file watcher
void xo_watcher_free(xo_watcher_t *watcher) {
    if (!watcher) {
        return;
    }
    
    // Stop the watcher if it's running
    if (watcher->running) {
        xo_watcher_stop(watcher);
    }
    
    // Free watch paths
    for (size_t i = 0; i < watcher->path_count; i++) {
        free(watcher->watch_paths[i]);
    }
    
    free(watcher->watch_paths);
    
    // Reset structure
    watcher->handle = NULL;
    watcher->watch_paths = NULL;
    watcher->path_count = 0;
    watcher->running = false;
    watcher->callback = NULL;
    watcher->user_data = NULL;
}

// Add a path to watch
int xo_watcher_add_path(xo_watcher_t *watcher, const char *path) {
    if (!watcher || !path) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if the path exists
    if (!xo_utils_dir_exists(path)) {
        xo_utils_console_error("Directory does not exist: %s", path);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Resize the paths array
    size_t new_count = watcher->path_count + 1;
    char **new_paths = realloc(watcher->watch_paths, new_count * sizeof(char *));
    if (!new_paths) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    watcher->watch_paths = new_paths;
    
    // Add the new path
    watcher->watch_paths[watcher->path_count] = strdup(path);
    if (!watcher->watch_paths[watcher->path_count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    watcher->path_count = new_count;
    
    return XO_SUCCESS;
}

// Start watching for file changes
int xo_watcher_start(xo_watcher_t *watcher, xo_watcher_callback_t callback, void *user_data) {
    if (!watcher || !callback) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    if (watcher->running) {
        xo_utils_console_warning("File watcher is already running");
        return XO_SUCCESS;
    }
    
    if (watcher->path_count == 0) {
        xo_utils_console_warning("No paths to watch");
        return XO_SUCCESS;
    }
    
    // Set the callback and user data
    watcher->callback = callback;
    watcher->user_data = user_data;
    
    // This is a placeholder that doesn't actually start watching
    xo_utils_console_info("Would start watching %zu paths", watcher->path_count);
    for (size_t i = 0; i < watcher->path_count; i++) {
        xo_utils_console_info("  %s", watcher->watch_paths[i]);
    }
    
    watcher->running = true;
    
    return XO_SUCCESS;
}

// Stop watching for file changes
int xo_watcher_stop(xo_watcher_t *watcher) {
    if (!watcher) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    if (!watcher->running) {
        xo_utils_console_warning("File watcher is not running");
        return XO_SUCCESS;
    }
    
    // This is a placeholder that doesn't actually stop watching
    xo_utils_console_info("Would stop watching");
    
    watcher->running = false;
    
    return XO_SUCCESS;
}

// Handle a file event
void xo_handle_file_event(const xo_file_event_t *event, void *user_data) {
    if (!event) {
        return;
    }
    
    const char *event_type;
    switch (event->type) {
        case XO_FILE_CREATED:
            event_type = "created";
            break;
        case XO_FILE_MODIFIED:
            event_type = "modified";
            break;
        case XO_FILE_DELETED:
            event_type = "deleted";
            break;
        default:
            event_type = "unknown";
            break;
    }
    
    xo_utils_console_info("File %s: %s", event_type, event->filepath);
    
    // In a real implementation, we would rebuild affected files here
    // This is just a placeholder for now
} 