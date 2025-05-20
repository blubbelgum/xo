#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "xo.h"  // Include xo.h first to get XO_MAX_PATH definition

// Forward declaration of xo_watcher_t which is defined in watcher.h
typedef struct xo_watcher_s xo_watcher_t;

#ifdef _WIN32
    #include <winsock2.h> // Include this first to avoid conflicts with windows.h
    #include <windows.h>
    
    // Windows threading
    typedef HANDLE thread_handle_t;
    typedef DWORD WINAPI thread_func_t(LPVOID);
    #define thread_create(handle, func, arg) (((*(handle)) = CreateThread(NULL, 0, (func), (arg), 0, NULL)) == NULL)
    #define thread_join(handle) WaitForSingleObject((handle), INFINITE); CloseHandle((handle))
    #define thread_sleep(ms) Sleep(ms)
    
    // Windows-specific watcher data
    typedef struct {
        HANDLE dir_handle;
        OVERLAPPED overlapped;
        char buffer[8192];  // Buffer for change notifications
        char path[XO_MAX_PATH];
        xo_watcher_t *watcher;
    } xo_win32_watch_data_t;
#else
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <unistd.h>
    #include <poll.h>
    
    // POSIX threading
    typedef pthread_t thread_handle_t;
    typedef void *(*thread_func_t)(void *);
    #define thread_create(handle, func, arg) pthread_create((handle), NULL, (func), (arg))
    #define thread_join(handle) pthread_join((handle), NULL)
    #define thread_sleep(ms) usleep((ms) * 1000)
    
    // Size of the inotify event buffer
    #define XO_EVENT_BUF_LEN (10 * (sizeof(struct inotify_event) + XO_MAX_PATH))
    
    // POSIX-specific watcher data
    typedef struct {
        int inotify_fd;
        int *watch_descriptors;
        char **watch_paths;
        size_t watch_count;
        thread_handle_t thread;
        bool running;
    } xo_posix_watch_data_t;
#endif

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
    
    // Free platform-specific data
    if (watcher->handle) {
#ifdef _WIN32
        xo_win32_watch_data_t **watch_data = (xo_win32_watch_data_t **)watcher->handle;
        for (size_t i = 0; i < watcher->path_count; i++) {
            if (watch_data[i]) {
                if (watch_data[i]->dir_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(watch_data[i]->dir_handle);
                }
                free(watch_data[i]);
            }
        }
        free(watch_data);
#else
        xo_posix_watch_data_t *watch_data = (xo_posix_watch_data_t *)watcher->handle;
        if (watch_data->inotify_fd >= 0) {
            close(watch_data->inotify_fd);
        }
        free(watch_data->watch_descriptors);
        free(watch_data->watch_paths);
        free(watch_data);
#endif
    }
    
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

#ifdef _WIN32
// Windows-specific file change callback
static VOID CALLBACK xo_win32_file_change_callback(DWORD error_code, DWORD bytes_transferred, LPOVERLAPPED overlapped) {
    if (error_code == ERROR_OPERATION_ABORTED) {
        return;  // Watcher was stopped
    }
    
    xo_win32_watch_data_t *watch_data = (xo_win32_watch_data_t *)overlapped;
    if (!watch_data || !watch_data->watcher || !watch_data->watcher->callback) {
        return;
    }
    
    // Process the FILE_NOTIFY_INFORMATION records
    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)watch_data->buffer;
    
    while (true) {
        // Extract the filename from the notification
        WCHAR filename_w[XO_MAX_PATH];
        memcpy(filename_w, info->FileName, info->FileNameLength);
        filename_w[info->FileNameLength / sizeof(WCHAR)] = L'\0';
        
        // Convert to ASCII
        char filename[XO_MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, filename_w, -1, filename, sizeof(filename), NULL, NULL);
        
        // Construct the full path
        char filepath[XO_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s\\%s", watch_data->path, filename);
        
        // Determine the event type
        xo_file_event_type_t event_type;
        switch (info->Action) {
            case FILE_ACTION_ADDED:
                event_type = XO_FILE_CREATED;
                break;
            case FILE_ACTION_REMOVED:
                event_type = XO_FILE_DELETED;
                break;
            case FILE_ACTION_MODIFIED:
                event_type = XO_FILE_MODIFIED;
                break;
            default:
                // Skip other events
                goto next_record;
        }
        
        // Create and dispatch the event
        xo_file_event_t event;
        event.type = event_type;
        event.filepath = filepath;
        watch_data->watcher->callback(&event, watch_data->watcher->user_data);
        
    next_record:
        // Move to the next record if there is one
        if (info->NextEntryOffset == 0) {
            break;
        }
        info = (FILE_NOTIFY_INFORMATION *)((BYTE *)info + info->NextEntryOffset);
    }
    
    // Reregister for notifications
    ReadDirectoryChangesW(
        watch_data->dir_handle,
        watch_data->buffer,
        sizeof(watch_data->buffer),
        TRUE,  // Watch subdirectories
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        NULL,
        &watch_data->overlapped,
        xo_win32_file_change_callback
    );
}

// Windows-specific watcher start
static int xo_win32_watcher_start(xo_watcher_t *watcher) {
    // Allocate array of watch data pointers
    xo_win32_watch_data_t **watch_data = malloc(watcher->path_count * sizeof(xo_win32_watch_data_t *));
    if (!watch_data) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Create a watch for each path
    for (size_t i = 0; i < watcher->path_count; i++) {
        // Allocate watch data
        watch_data[i] = malloc(sizeof(xo_win32_watch_data_t));
        if (!watch_data[i]) {
            for (size_t j = 0; j < i; j++) {
                if (watch_data[j]->dir_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(watch_data[j]->dir_handle);
                }
                free(watch_data[j]);
            }
            free(watch_data);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        // Open the directory
        watch_data[i]->dir_handle = CreateFile(
            watcher->watch_paths[i],
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );
        
        if (watch_data[i]->dir_handle == INVALID_HANDLE_VALUE) {
            xo_utils_console_error("Failed to open directory for watching: %s", watcher->watch_paths[i]);
            for (size_t j = 0; j < i; j++) {
                if (watch_data[j]->dir_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(watch_data[j]->dir_handle);
                }
                free(watch_data[j]);
            }
            free(watch_data);
            return XO_ERROR_FILE_NOT_FOUND;
        }
        
        // Initialize the watch data
        watch_data[i]->overlapped.hEvent = 0;  // No event
        strcpy(watch_data[i]->path, watcher->watch_paths[i]);
        watch_data[i]->watcher = watcher;
        
        // Register for change notifications
        BOOL success = ReadDirectoryChangesW(
            watch_data[i]->dir_handle,
            watch_data[i]->buffer,
            sizeof(watch_data[i]->buffer),
            TRUE,  // Watch subdirectories
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL,
            &watch_data[i]->overlapped,
            xo_win32_file_change_callback
        );
        
        if (!success) {
            xo_utils_console_error("Failed to watch directory: %s", watcher->watch_paths[i]);
            for (size_t j = 0; j <= i; j++) {
                if (watch_data[j]->dir_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(watch_data[j]->dir_handle);
                }
                free(watch_data[j]);
            }
            free(watch_data);
            return XO_ERROR_FILE_NOT_FOUND;
        }
    }
    
    // Set the handle
    watcher->handle = watch_data;
    
    return XO_SUCCESS;
}

// Windows-specific watcher stop
static int xo_win32_watcher_stop(xo_watcher_t *watcher) {
    if (!watcher->handle) {
        return XO_SUCCESS;
    }
    
    xo_win32_watch_data_t **watch_data = (xo_win32_watch_data_t **)watcher->handle;
    
    // Cancel all watches
    for (size_t i = 0; i < watcher->path_count; i++) {
        if (watch_data[i]->dir_handle != INVALID_HANDLE_VALUE) {
            // Cancel pending I/O operations
            CancelIo(watch_data[i]->dir_handle);
            CloseHandle(watch_data[i]->dir_handle);
            watch_data[i]->dir_handle = INVALID_HANDLE_VALUE;
        }
    }
    
    return XO_SUCCESS;
}
#else
// POSIX-specific watcher thread
static void *xo_posix_watcher_thread(void *arg) {
    xo_watcher_t *watcher = (xo_watcher_t *)arg;
    if (!watcher || !watcher->handle) {
        return NULL;
    }
    
    xo_posix_watch_data_t *watch_data = (xo_posix_watch_data_t *)watcher->handle;
    
    // Allocate buffer for events
    char buffer[XO_EVENT_BUF_LEN];
    
    // Set up polling
    struct pollfd pfd = {
        .fd = watch_data->inotify_fd,
        .events = POLLIN,
        .revents = 0
    };
    
    // Run until stopped
    while (watch_data->running) {
        // Wait for events with a timeout
        int poll_result = poll(&pfd, 1, 500);  // 500ms timeout
        
        // Check if we should exit
        if (!watch_data->running) {
            break;
        }
        
        // Check for errors
        if (poll_result < 0) {
            xo_utils_console_error("Error while polling for file events");
            break;
        }
        
        // Check for events
        if (poll_result > 0 && (pfd.revents & POLLIN)) {
            // Read events
            ssize_t length = read(watch_data->inotify_fd, buffer, XO_EVENT_BUF_LEN);
            if (length < 0) {
                xo_utils_console_error("Error reading inotify events");
                break;
            }
            
            // Process events
            ssize_t i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                
                // Find the path for this watch descriptor
                char *path = NULL;
                for (size_t j = 0; j < watch_data->watch_count; j++) {
                    if (watch_data->watch_descriptors[j] == event->wd) {
                        path = watch_data->watch_paths[j];
                        break;
                    }
                }
                
                if (path && event->len > 0) {
                    // Construct the full path
                    char filepath[XO_MAX_PATH];
                    snprintf(filepath, sizeof(filepath), "%s/%s", path, event->name);
                    
                    // Determine the event type
                    xo_file_event_type_t event_type;
                    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                        event_type = XO_FILE_CREATED;
                    } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                        event_type = XO_FILE_DELETED;
                    } else if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                        event_type = XO_FILE_MODIFIED;
                    } else {
                        // Skip other events
                        goto next_event;
                    }
                    
                    // Create and dispatch the event
                    xo_file_event_t file_event;
                    file_event.type = event_type;
                    file_event.filepath = filepath;
                    watcher->callback(&file_event, watcher->user_data);
                }
                
            next_event:
                // Move to the next event
                i += sizeof(struct inotify_event) + event->len;
            }
        }
    }
    
    return NULL;
}

// POSIX-specific watcher start
static int xo_posix_watcher_start(xo_watcher_t *watcher) {
    // Allocate watch data
    xo_posix_watch_data_t *watch_data = malloc(sizeof(xo_posix_watch_data_t));
    if (!watch_data) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize watch data
    watch_data->inotify_fd = -1;
    watch_data->watch_descriptors = NULL;
    watch_data->watch_paths = NULL;
    watch_data->watch_count = 0;
    watch_data->running = false;
    
    // Initialize inotify
    watch_data->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (watch_data->inotify_fd < 0) {
        xo_utils_console_error("Failed to initialize inotify");
        free(watch_data);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Allocate arrays for watch descriptors and paths
    watch_data->watch_descriptors = malloc(watcher->path_count * sizeof(int));
    watch_data->watch_paths = malloc(watcher->path_count * sizeof(char *));
    
    if (!watch_data->watch_descriptors || !watch_data->watch_paths) {
        if (watch_data->watch_descriptors) free(watch_data->watch_descriptors);
        if (watch_data->watch_paths) free(watch_data->watch_paths);
        close(watch_data->inotify_fd);
        free(watch_data);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Add watches for each path
    for (size_t i = 0; i < watcher->path_count; i++) {
        // Add the watch
        int wd = inotify_add_watch(
            watch_data->inotify_fd,
            watcher->watch_paths[i],
            IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM | IN_CLOSE_WRITE
        );
        
        if (wd < 0) {
            xo_utils_console_error("Failed to watch directory: %s", watcher->watch_paths[i]);
            for (size_t j = 0; j < i; j++) {
                inotify_rm_watch(watch_data->inotify_fd, watch_data->watch_descriptors[j]);
            }
            free(watch_data->watch_descriptors);
            free(watch_data->watch_paths);
            close(watch_data->inotify_fd);
            free(watch_data);
            return XO_ERROR_FILE_NOT_FOUND;
        }
        
        // Store the watch descriptor and path
        watch_data->watch_descriptors[i] = wd;
        watch_data->watch_paths[i] = watcher->watch_paths[i];
    }
    
    watch_data->watch_count = watcher->path_count;
    
    // Start the watcher thread
    watch_data->running = true;
    if (thread_create(&watch_data->thread, xo_posix_watcher_thread, watcher) != 0) {
        xo_utils_console_error("Failed to start watcher thread");
        for (size_t i = 0; i < watcher->path_count; i++) {
            inotify_rm_watch(watch_data->inotify_fd, watch_data->watch_descriptors[i]);
        }
        free(watch_data->watch_descriptors);
        free(watch_data->watch_paths);
        close(watch_data->inotify_fd);
        free(watch_data);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Set the handle
    watcher->handle = watch_data;
    
    return XO_SUCCESS;
}

// POSIX-specific watcher stop
static int xo_posix_watcher_stop(xo_watcher_t *watcher) {
    if (!watcher->handle) {
        return XO_SUCCESS;
    }
    
    xo_posix_watch_data_t *watch_data = (xo_posix_watch_data_t *)watcher->handle;
    
    // Stop the thread
    watch_data->running = false;
    thread_join(watch_data->thread);
    
    // Remove all watches
    for (size_t i = 0; i < watch_data->watch_count; i++) {
        inotify_rm_watch(watch_data->inotify_fd, watch_data->watch_descriptors[i]);
    }
    
    return XO_SUCCESS;
}
#endif

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
    
    // Start the platform-specific watcher
    int result;
#ifdef _WIN32
    result = xo_win32_watcher_start(watcher);
#else
    result = xo_posix_watcher_start(watcher);
#endif
    
    if (result == XO_SUCCESS) {
        watcher->running = true;
        xo_utils_console_info("Started watching %zu paths", watcher->path_count);
        for (size_t i = 0; i < watcher->path_count; i++) {
            xo_utils_console_info("  %s", watcher->watch_paths[i]);
        }
    }
    
    return result;
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
    
    // Stop the platform-specific watcher
    int result;
#ifdef _WIN32
    result = xo_win32_watcher_stop(watcher);
#else
    result = xo_posix_watcher_stop(watcher);
#endif
    
    if (result == XO_SUCCESS) {
        watcher->running = false;
        xo_utils_console_info("Stopped watching");
    }
    
    return result;
}

// Handle a file event
void xo_handle_file_event(const xo_file_event_t *event, void *user_data) {
    if (!event || !user_data) {
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
    
    // Get the extension
    const char *ext = xo_utils_get_extension(event->filepath);
    
    // Check if we should rebuild
    if (ext && (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0)) {
        // This is a markdown file, rebuild it
        xo_utils_console_info("Rebuilding: %s", event->filepath);
        xo_config_t *config = (xo_config_t *)user_data;
        
        if (event->type == XO_FILE_DELETED) {
            // If the file was deleted, we need to remove the corresponding HTML file
            char *html_path = xo_utils_str_replace(event->filepath, 
                config->content_dir, config->output_dir);
            if (html_path) {
                char *ext_pos = strrchr(html_path, '.');
                if (ext_pos) {
                    strcpy(ext_pos, ".html");
                    xo_utils_console_info("Removing: %s", html_path);
                    remove(html_path);
                }
                free(html_path);
            }
        } else {
            // Otherwise rebuild the file
            xo_dependency_tracker_t tracker;
            xo_dependency_tracker_init(&tracker);
            xo_build_file(config, event->filepath, &tracker);
            xo_dependency_tracker_free(&tracker);
        }
    } else if (ext && (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0 || 
                       strcmp(ext, "css") == 0 || strcmp(ext, "js") == 0)) {
        // This is a static asset, we should reload the browser
        xo_utils_console_info("Reloading browser");
        xo_server_t *server = (xo_server_t *)((xo_config_t *)user_data)->user_data;
        if (server) {
            xo_server_broadcast_ws(server, "reload", 6);
        }
    }
} 