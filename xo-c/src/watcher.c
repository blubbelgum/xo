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
        OVERLAPPED overlapped; // Will use its hEvent member
        char buffer[8192];  // Buffer for change notifications
        char path[XO_MAX_PATH];
        xo_watcher_t *watcher;
        HANDLE hThread;       // Handle for the dedicated watcher thread
        HANDLE hStopEvent;    // Event to signal the thread to stop
    } xo_win32_watch_data_t;
#else
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <unistd.h>
    #include <poll.h>
    #include <dirent.h> // For opendir, readdir, closedir
    #include <sys/stat.h> // For stat, S_ISDIR
    #include <errno.h>  // For errno and strerror
    #include <string.h> // For strerror (though often included by other headers)
    
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
        size_t watch_capacity; // Added for dynamic array management
        thread_handle_t thread;
        bool running;
    } xo_posix_watch_data_t;
#endif

#include "watcher.h"
#include "utils.h"
#include "server.h"

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
        xo_win32_watch_data_t **all_watch_data = (xo_win32_watch_data_t **)watcher->handle;
        printf("[XO DEBUG] Windows watcher: Freeing watcher resources. Path count: %zu\n", watcher->path_count);
        for (size_t i = 0; i < watcher->path_count; i++) {
            if (all_watch_data[i]) {
                printf("[XO DEBUG] Windows watcher: Freeing resources for path: %s\n", all_watch_data[i]->path);
                // Note: Thread should be stopped and handle closed by xo_win32_watcher_stop
                // which should be called by xo_watcher_stop if watcher->running.
                // Here, we primarily clean up handles that are part of the watch_data structure itself.

                if (all_watch_data[i]->dir_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(all_watch_data[i]->dir_handle);
                    all_watch_data[i]->dir_handle = INVALID_HANDLE_VALUE;
                     printf("[XO DEBUG] Windows watcher: Closed dir_handle for %s\n", all_watch_data[i]->path);
                }
                if (all_watch_data[i]->overlapped.hEvent) {
                    CloseHandle(all_watch_data[i]->overlapped.hEvent);
                    all_watch_data[i]->overlapped.hEvent = NULL;
                    printf("[XO DEBUG] Windows watcher: Closed overlapped.hEvent for %s\n", all_watch_data[i]->path);
                }
                if (all_watch_data[i]->hStopEvent) {
                    CloseHandle(all_watch_data[i]->hStopEvent);
                    all_watch_data[i]->hStopEvent = NULL;
                    printf("[XO DEBUG] Windows watcher: Closed hStopEvent for %s\n", all_watch_data[i]->path);
                }
                // The thread handle (hThread) should be closed in xo_win32_watcher_stop after the thread has terminated.
                // If xo_watcher_stop was not called (e.g., watcher was not running),
                // hThread might still be open if it was created.
                // However, proper sequence is stop -> free. If stop wasn't called, thread might still be running which is problematic.
                // For robustness, if hThread is non-NULL here, it implies improper shutdown or that stop wasn't called.
                // We'll close it, but this indicates a potential logic flaw elsewhere if the thread is still running.
                if (all_watch_data[i]->hThread) {
                    printf("[XO DEBUG] Windows watcher: Warning: hThread for %s still open during free. Closing.\n", all_watch_data[i]->path);
                    CloseHandle(all_watch_data[i]->hThread);
                    all_watch_data[i]->hThread = NULL;
                }

                free(all_watch_data[i]);
                all_watch_data[i] = NULL;
            }
        }
        free(all_watch_data); // Free the array of pointers
        watcher->handle = NULL; // Ensure handle is cleared
#else
        xo_posix_watch_data_t *watch_data = (xo_posix_watch_data_t *)watcher->handle;
        if (watch_data->inotify_fd >= 0) {
            // The xo_posix_watcher_stop function should handle removing watches and joining the thread.
            // Here, we are concerned with freeing the memory allocated in xo_posix_watcher_start / add_watches_recursive.
            
            // Free the duplicated path strings stored in watch_data->watch_paths
            for (size_t i = 0; i < watch_data->watch_count; i++) {
                free(watch_data->watch_paths[i]);
            }
            // Now safe to close inotify_fd, though typically stop should be called first
            // if not already by the running check at the beginning of xo_watcher_free.
            close(watch_data->inotify_fd); 
        }
        // Free the arrays themselves
        free(watch_data->watch_descriptors);
        free(watch_data->watch_paths);
        free(watch_data); // Free the container struct
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
// xo_win32_file_change_callback is removed as its logic is now in xo_win32_watcher_thread

static DWORD WINAPI xo_win32_watcher_thread(LPVOID param) {
    xo_win32_watch_data_t *watch_data = (xo_win32_watch_data_t *)param;
    xo_watcher_t *watcher = watch_data->watcher;
    BOOL watch_active = TRUE;

    // Initial ReadDirectoryChangesW call
    if (!ReadDirectoryChangesW(
            watch_data->dir_handle,
            watch_data->buffer,
            sizeof(watch_data->buffer),
            TRUE, // Watch subdirectories
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            NULL,
            &watch_data->overlapped,
            NULL // No completion routine
        )) {
        xo_utils_console_error("Initial ReadDirectoryChangesW failed for %s: %lu", watch_data->path, GetLastError());
        printf("[XO DEBUG] Windows watcher: Initial ReadDirectoryChangesW failed for %s: %lu\n", watch_data->path, GetLastError());
        // Ensure hEvent is closed if it was created, though it might not be at this specific failure point.
        // The caller thread that created this thread should handle cleanup of watch_data if CreateThread succeeds but this fails.
        return 1; // Indicate error
    }
    printf("[XO DEBUG] Windows watcher: Initial ReadDirectoryChangesW issued for %s\n", watch_data->path);

    HANDLE wait_handles[2];
    wait_handles[0] = watch_data->hStopEvent;
    wait_handles[1] = watch_data->overlapped.hEvent; // Event from ReadDirectoryChangesW

    while (watch_active) {
        printf("[XO DEBUG] Windows watcher: Thread for %s waiting for events...\n", watch_data->path);
        DWORD wait_status = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

        switch (wait_status) {
            case WAIT_OBJECT_0: // hStopEvent
                printf("[XO DEBUG] Windows watcher: Stop event received for %s, canceling I/O and exiting thread.\n", watch_data->path);
                CancelIo(watch_data->dir_handle); // Attempt to cancel pending I/O
                watch_active = FALSE;
                break;

            case WAIT_OBJECT_0 + 1: // overlapped.hEvent
                printf("[XO DEBUG] Windows watcher: File event signaled for %s\n", watch_data->path);
                DWORD bytes_transferred;
                // Check GetOverlappedResult success. Note: TRUE for bWait means it waits, FALSE means it returns immediately.
                // Since WaitForMultipleObjects already told us the event is signaled, we use FALSE for bWait.
                if (!GetOverlappedResult(watch_data->dir_handle, &watch_data->overlapped, &bytes_transferred, FALSE)) {
                    DWORD error = GetLastError();
                    // ERROR_IO_INCOMPLETE is expected if the operation is not yet complete (should not happen here as event is signaled)
                    // ERROR_OPERATION_ABORTED (995) can happen if CancelIo was called (e.g. on stop)
                    if (error == ERROR_OPERATION_ABORTED) {
                         printf("[XO DEBUG] Windows watcher: GetOverlappedResult for %s aborted (expected on stop).\n", watch_data->path);
                    } else {
                         xo_utils_console_error("GetOverlappedResult failed for %s: %lu", watch_data->path, error);
                         printf("[XO DEBUG] Windows watcher: GetOverlappedResult failed for %s: %lu\n", watch_data->path, error);
                    }
                    watch_active = FALSE; // Stop on error or abort
                    break;
                }

                if (bytes_transferred == 0) {
                    // This means the operation completed, but no data was transferred.
                    // This can happen if the directory handle is closed, or other rare conditions.
                    // Or, it might indicate the buffer is not large enough for all notifications,
                    // though typically ReadDirectoryChangesW would still fill part of it.
                    printf("[XO DEBUG] Windows watcher: GetOverlappedResult returned 0 bytes for %s. This might indicate an issue or normal closure.\n", watch_data->path);
                    // It's often prudent to re-issue the watch if this isn't an exit signal.
                    // However, if this happens unexpectedly, it could lead to a busy loop if not handled carefully.
                    // For now, we will attempt to re-issue, but if it becomes problematic, this logic might need refinement.
                } else {
                    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)watch_data->buffer;
                    while (info) { // Loop through all notifications in the buffer
                        WCHAR filename_w[XO_MAX_PATH];
                        char filename[XO_MAX_PATH];
                        char filepath[XO_MAX_PATH];

                        // Ensure FileNameLength is reasonable to prevent buffer overflow
                        // FileNameLength is in bytes.
                        size_t name_len_bytes = info->FileNameLength;
                        if (name_len_bytes >= sizeof(filename_w)) {
                             printf("[XO DEBUG] Windows watcher: Warning: Truncating long filename from notification for %s. Original length: %u bytes.\n", watch_data->path, (unsigned int)name_len_bytes);
                             name_len_bytes = sizeof(filename_w) - sizeof(WCHAR); // Ensure space for null terminator
                        }

                        memcpy(filename_w, info->FileName, name_len_bytes);
                        filename_w[name_len_bytes / sizeof(WCHAR)] = L'\0'; // Null-terminate
                        
                        // Convert filename to multi-byte string
                        if (WideCharToMultiByte(CP_ACP, 0, filename_w, -1, filename, sizeof(filename), NULL, NULL) == 0) {
                            printf("[XO DEBUG] Windows watcher: WideCharToMultiByte failed for %S. Error: %lu\n", filename_w, GetLastError());
                            // Skip this record if conversion fails
                            goto next_record_thread;
                        }
                        
                        snprintf(filepath, sizeof(filepath), "%s\\%s", watch_data->path, filename);
                        
                        printf("[XO DEBUG] Windows watcher: Event raw in thread: Action=%lu, FileName=%s (Path: %s)\n", info->Action, filename, filepath);

                        xo_file_event_type_t event_type;
                        switch (info->Action) {
                            case FILE_ACTION_ADDED:
                            case FILE_ACTION_RENAMED_NEW_NAME:
                                event_type = XO_FILE_CREATED;
                                break;
                            case FILE_ACTION_REMOVED:
                            case FILE_ACTION_RENAMED_OLD_NAME:
                                event_type = XO_FILE_DELETED;
                                break;
                            case FILE_ACTION_MODIFIED:
                                event_type = XO_FILE_MODIFIED;
                                break;
                            default:
                                printf("[XO DEBUG] Windows watcher: Skipping unknown action %lu for %s\n", info->Action, filepath);
                                goto next_record_thread;
                        }

                        xo_file_event_t event;
                        event.type = event_type;
                        // The filepath is on the stack here. The callback must use it synchronously or copy it.
                        // xo_handle_file_event seems to use it synchronously.
                        event.filepath = filepath; 
                        
                        if (watcher && watcher->callback) {
                            watcher->callback(&event, watcher->user_data);
                            printf("[XO DEBUG] Windows watcher: Event processed by callback: type=%d, path=%s\n", event.type, event.filepath);
                        } else {
                            printf("[XO DEBUG] Windows watcher: Watcher or callback is null. Cannot dispatch event for %s.\n", filepath);
                        }

                    next_record_thread:
                        if (info->NextEntryOffset == 0) {
                            info = NULL; // End of list
                        } else {
                            info = (FILE_NOTIFY_INFORMATION *)((BYTE *)info + info->NextEntryOffset);
                        }
                    }
                }

                // Re-issue ReadDirectoryChangesW for the next set of changes
                // Reset the event associated with the OVERLAPPED structure before re-using it.
                if (!ResetEvent(watch_data->overlapped.hEvent)) {
                    xo_utils_console_error("ResetEvent failed for %s: %lu", watch_data->path, GetLastError());
                    printf("[XO DEBUG] Windows watcher: ResetEvent failed for %s: %lu. Stopping watch.\n", watch_data->path, GetLastError());
                    watch_active = FALSE; // Critical failure, stop watching
                    break; 
                }

                if (!ReadDirectoryChangesW(
                        watch_data->dir_handle,
                        watch_data->buffer,
                        sizeof(watch_data->buffer),
                        TRUE, // Watch subdirectories
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
                        NULL,
                        &watch_data->overlapped,
                        NULL // No completion routine
                    )) {
                    DWORD error = GetLastError();
                    // ERROR_IO_PENDING is not an error here with OVERLAPPED I/O but we are not using completion routine.
                    // If the operation is pending, it's fine. If it fails immediately, that's an issue.
                    // However, ReadDirectoryChangesW with OVERLAPPED but no completion routine should return FALSE,
                    // and GetLastError() should return ERROR_IO_PENDING if it's successfully queued.
                    // Let's assume any return other than FALSE with ERROR_IO_PENDING is a true failure here.
                    // Actually, it returns FALSE and GetLastError() is ERROR_IO_PENDING when successful and async.
                    // The check here should be: if it returns FALSE AND GetLastError() is NOT ERROR_IO_PENDING, then it's an error.
                    // But since we wait on the event, we expect it to be queued.
                    // The original code did not check GetLastError() for ERROR_IO_PENDING after the initial call.
                    // For simplicity, we treat any failure to queue as critical.
                    xo_utils_console_error("Re-issuing ReadDirectoryChangesW failed for %s: %lu", watch_data->path, error);
                    printf("[XO DEBUG] Windows watcher: Re-issuing ReadDirectoryChangesW failed for %s: %lu\n", watch_data->path, error);
                    watch_active = FALSE; // Stop on error
                } else {
                     printf("[XO DEBUG] Windows watcher: Re-issued ReadDirectoryChangesW for %s\n", watch_data->path);
                }
                break;

            default: // WAIT_FAILED or other error
                xo_utils_console_error("WaitForMultipleObjects failed for %s: %lu", watch_data->path, GetLastError());
                printf("[XO DEBUG] Windows watcher: WaitForMultipleObjects failed for %s: %lu. Exiting thread.\n", watch_data->path, GetLastError());
                watch_active = FALSE;
                break;
        }
    }

    printf("[XO DEBUG] Windows watcher: Thread for %s cleaning up and exiting.\n", watch_data->path);
    // Ensure the event is closed if it was created by this thread and not passed in (which it is, in OVERLAPPED)
    // However, overlapped.hEvent is managed by the main thread (created in start, closed in free).
    // So, this thread should not close overlapped.hEvent.
    return 0; // Success
}

// Windows-specific watcher start
static int xo_win32_watcher_start(xo_watcher_t *watcher) {
    // Allocate an array of pointers to xo_win32_watch_data_t structures
    // This array will be stored in watcher->handle
    xo_win32_watch_data_t **all_watch_data = malloc(watcher->path_count * sizeof(xo_win32_watch_data_t *));
    if (!all_watch_data) {
        xo_utils_console_error("Failed to allocate memory for watch data array");
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    // Initialize all pointers to NULL for easier cleanup in case of partial failure
    for (size_t k = 0; k < watcher->path_count; ++k) {
        all_watch_data[k] = NULL;
    }

    printf("[XO DEBUG] Windows watcher: Starting xo_win32_watcher_start. Number of paths to process: %zu\n", watcher->path_count);
    for (size_t i = 0; i < watcher->path_count; i++) {
        printf("[XO DEBUG] Windows watcher: Initializing watch for path: %s\n", watcher->watch_paths[i]);
        
        // Allocate individual watch data structure
        all_watch_data[i] = malloc(sizeof(xo_win32_watch_data_t));
        if (!all_watch_data[i]) {
            xo_utils_console_error("Failed to allocate memory for watch_data[%zu]", i);
            // Cleanup previously allocated structures and the main array
            for (size_t j = 0; j < i; j++) {
                if (all_watch_data[j]) {
                    if (all_watch_data[j]->dir_handle != INVALID_HANDLE_VALUE) CloseHandle(all_watch_data[j]->dir_handle);
                    if (all_watch_data[j]->overlapped.hEvent) CloseHandle(all_watch_data[j]->overlapped.hEvent);
                    if (all_watch_data[j]->hStopEvent) CloseHandle(all_watch_data[j]->hStopEvent);
                    // Thread handle is not yet created or managed by stop function
                    free(all_watch_data[j]);
                }
            }
            free(all_watch_data);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        // Initialize fields
        all_watch_data[i]->dir_handle = INVALID_HANDLE_VALUE;
        all_watch_data[i]->overlapped.hEvent = NULL;
        all_watch_data[i]->hStopEvent = NULL;
        all_watch_data[i]->hThread = NULL;
        all_watch_data[i]->watcher = watcher;
        strncpy(all_watch_data[i]->path, watcher->watch_paths[i], XO_MAX_PATH -1);
        all_watch_data[i]->path[XO_MAX_PATH -1] = '\0';


        // Open the directory
        all_watch_data[i]->dir_handle = CreateFile(
            all_watch_data[i]->path,
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // FILE_FLAG_OVERLAPPED is crucial
            NULL
        );
        
        if (all_watch_data[i]->dir_handle == INVALID_HANDLE_VALUE) {
            xo_utils_console_error("Failed to open directory for watching: %s. Error: %lu", all_watch_data[i]->path, GetLastError());
            printf("[XO DEBUG] Windows watcher: CreateFile failed for %s. Error: %lu\n", all_watch_data[i]->path, GetLastError());
            // No need to loop for cleanup here, the main error handling below will catch this path's data
            free(all_watch_data[i]); // Free this specific item
            all_watch_data[i] = NULL; // Mark as freed
            // This error will be caught by the check after the loop, which will clean up all successfully created items so far
            continue; // Try next path, or let the loop finish and then error out if no paths succeed
        }
         printf("[XO DEBUG] Windows watcher: CreateFile succeeded for %s.\n", all_watch_data[i]->path);

        // Create event for OVERLAPPED structure
        all_watch_data[i]->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (all_watch_data[i]->overlapped.hEvent == NULL) {
            xo_utils_console_error("CreateEvent for overlapped failed for %s: %lu", all_watch_data[i]->path, GetLastError());
            CloseHandle(all_watch_data[i]->dir_handle);
            free(all_watch_data[i]);
            all_watch_data[i] = NULL;
            continue;
        }
        printf("[XO DEBUG] Windows watcher: Overlapped event created for %s.\n", all_watch_data[i]->path);

        // Create stop event for this thread
        all_watch_data[i]->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // Manual reset, initially non-signaled
        if (all_watch_data[i]->hStopEvent == NULL) {
            xo_utils_console_error("CreateEvent for stop event failed for %s: %lu", all_watch_data[i]->path, GetLastError());
            CloseHandle(all_watch_data[i]->overlapped.hEvent);
            CloseHandle(all_watch_data[i]->dir_handle);
            free(all_watch_data[i]);
            all_watch_data[i] = NULL;
            continue;
        }
        printf("[XO DEBUG] Windows watcher: Stop event created for %s.\n", all_watch_data[i]->path);
        
        // Create the dedicated watcher thread
        all_watch_data[i]->hThread = CreateThread(NULL, 0, xo_win32_watcher_thread, all_watch_data[i], 0, NULL);
        if (all_watch_data[i]->hThread == NULL) {
            xo_utils_console_error("Failed to create watcher thread for %s: %lu", all_watch_data[i]->path, GetLastError());
            CloseHandle(all_watch_data[i]->hStopEvent);
            CloseHandle(all_watch_data[i]->overlapped.hEvent);
            CloseHandle(all_watch_data[i]->dir_handle);
            free(all_watch_data[i]);
            all_watch_data[i] = NULL;
            continue;
        }
        printf("[XO DEBUG] Windows watcher: Watcher thread created for %s.\n", all_watch_data[i]->path);
    }
    
    // Check if any threads were successfully started
    bool any_successful = false;
    for(size_t i = 0; i < watcher->path_count; ++i) {
        if (all_watch_data[i] != NULL && all_watch_data[i]->hThread != NULL) {
            any_successful = true;
            break;
        }
    }

    if (!any_successful && watcher->path_count > 0) {
        xo_utils_console_error("Windows watcher: Failed to start watching any of the %zu paths.", watcher->path_count);
        // Cleanup all_watch_data array and any partially initialized items
        for (size_t i = 0; i < watcher->path_count; i++) {
            if (all_watch_data[i]) {
                if (all_watch_data[i]->dir_handle != INVALID_HANDLE_VALUE) CloseHandle(all_watch_data[i]->dir_handle);
                if (all_watch_data[i]->overlapped.hEvent) CloseHandle(all_watch_data[i]->overlapped.hEvent);
                if (all_watch_data[i]->hStopEvent) CloseHandle(all_watch_data[i]->hStopEvent);
                // Thread handle might not be valid or needs specific cleanup if CreateThread failed partially
                if (all_watch_data[i]->hThread) CloseHandle(all_watch_data[i]->hThread); // Should ideally not happen if create failed
                free(all_watch_data[i]);
            }
        }
        free(all_watch_data);
        return XO_ERROR_FILE_NOT_FOUND; // Or a more specific error
    }
    
    // Store the array of watch data pointers in watcher->handle
    watcher->handle = all_watch_data;
    
    return XO_SUCCESS;
}

// Windows-specific watcher stop
static int xo_win32_watcher_stop(xo_watcher_t *watcher) {
    if (!watcher->handle) {
        return XO_SUCCESS;
    }
    
    xo_win32_watch_data_t **all_watch_data = (xo_win32_watch_data_t **)watcher->handle;
    if (!all_watch_data) {
        printf("[XO DEBUG] Windows watcher: Stop called but handle is NULL.\n");
        return XO_SUCCESS; // Or an error if appropriate
    }

    printf("[XO DEBUG] Windows watcher: Stopping watcher. Path count: %zu\n", watcher->path_count);
    for (size_t i = 0; i < watcher->path_count; i++) {
        if (all_watch_data[i]) {
            printf("[XO DEBUG] Windows watcher: Stopping watch for path: %s\n", all_watch_data[i]->path);
            if (all_watch_data[i]->hStopEvent) {
                printf("[XO DEBUG] Windows watcher: Setting stop event for thread handling %s.\n", all_watch_data[i]->path);
                SetEvent(all_watch_data[i]->hStopEvent);
            }

            if (all_watch_data[i]->hThread) {
                printf("[XO DEBUG] Windows watcher: Waiting for thread handling %s to terminate.\n", all_watch_data[i]->path);
                DWORD wait_result = WaitForSingleObject(all_watch_data[i]->hThread, INFINITE); // Or use a timeout
                if (wait_result == WAIT_OBJECT_0) {
                    printf("[XO DEBUG] Windows watcher: Thread for %s terminated successfully.\n", all_watch_data[i]->path);
                } else {
                    xo_utils_console_error("Failed to wait for watcher thread for %s to terminate. Status: %lu, Error: %lu", 
                                           all_watch_data[i]->path, wait_result, GetLastError());
                    printf("[XO DEBUG] Windows watcher: Failed to wait for thread for %s. Status: %lu, Error: %lu\n", 
                           all_watch_data[i]->path, wait_result, GetLastError());
                    // Potentially terminate thread if it doesn't stop, but this is risky.
                }
                CloseHandle(all_watch_data[i]->hThread);
                all_watch_data[i]->hThread = NULL;
                printf("[XO DEBUG] Windows watcher: Closed thread handle for %s.\n", all_watch_data[i]->path);
            }
             // dir_handle, overlapped.hEvent, and hStopEvent are closed in xo_watcher_free.
             // CancelIo is called from within the thread upon receiving the stop signal.
        }
    }
    
    // The xo_watcher_free function will handle freeing the all_watch_data array and its elements.
    return XO_SUCCESS;
}
#else
// Helper function to add watches recursively for POSIX
static int add_watches_recursive(xo_posix_watch_data_t *data, const char *dir_path, int inotify_fd) {
    // Check if the path is a directory
    struct stat path_stat;
    if (stat(dir_path, &path_stat) != 0) {
        printf("[XO DEBUG] POSIX watcher: Failed to stat directory %s: %s\n", dir_path, strerror(errno));
        xo_utils_console_error("Failed to stat directory: %s", dir_path);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    if (!S_ISDIR(path_stat.st_mode)) {
        printf("[XO DEBUG] POSIX watcher: Skipped non-directory entry: %s\n", dir_path);
        // Not a directory, skip. This can happen if a path added by user is a file.
        // Or if a non-directory is encountered during recursion (though current logic avoids this).
        return XO_SUCCESS; 
    }

    // Check if we need to grow the arrays
    if (data->watch_count >= data->watch_capacity) {
        size_t new_capacity = data->watch_capacity == 0 ? 16 : data->watch_capacity * 2;
        int *new_descriptors = realloc(data->watch_descriptors, new_capacity * sizeof(int));
        if (!new_descriptors) {
            xo_utils_console_error("Failed to realloc watch_descriptors");
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        data->watch_descriptors = new_descriptors;

        char **new_paths = realloc(data->watch_paths, new_capacity * sizeof(char *));
        if (!new_paths) {
            xo_utils_console_error("Failed to realloc watch_paths");
            // new_descriptors was already reallocated, assign NULL to avoid double free in caller's cleanup.
            // This is a simplification; robust error handling might require freeing new_descriptors here if it's not handled by caller.
            data->watch_descriptors = NULL; 
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        data->watch_paths = new_paths;
        data->watch_capacity = new_capacity;
    }

    // Add watch for the current directory
    // IN_ATTRIB is useful for catching changes to file permissions or ownership.
    // IN_ISDIR is implicit with IN_CREATE | IN_MOVED_TO on a watched directory; new subdirs will trigger events.
    int wd = inotify_add_watch(inotify_fd, dir_path, 
                             IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM | IN_CLOSE_WRITE | IN_ATTRIB);
    if (wd < 0) {
        printf("[XO DEBUG] POSIX watcher: Failed to add watch for %s: %s\n", dir_path, strerror(errno));
        xo_utils_console_error("Failed to add watch for directory: %s (errno: %d)", dir_path, errno);
        return XO_ERROR_FILE_NOT_FOUND; 
    }
    printf("[XO DEBUG] POSIX watcher: Adding watch for dir: %s (wd: %d)\n", dir_path, wd);

    data->watch_descriptors[data->watch_count] = wd;
    data->watch_paths[data->watch_count] = strdup(dir_path); 
    if (!data->watch_paths[data->watch_count]) {
        xo_utils_console_error("Failed to strdup dir_path: %s", dir_path);
        inotify_rm_watch(inotify_fd, wd); // Attempt to clean up the added watch
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    data->watch_count++;

    printf("[XO DEBUG] POSIX watcher: Recursively adding subdirectories of: %s\n", dir_path);
    DIR *dir = opendir(dir_path);
    if (!dir) {
        xo_utils_console_error("Failed to open directory for recursion: %s (errno: %d)", dir_path, errno);
        // The watch for dir_path itself was successful, so we don't want to report a failure for this level.
        // Log the error and continue. The directory itself will be watched.
        return XO_SUCCESS; 
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_entry_path[XO_MAX_PATH];
        snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", dir_path, entry->d_name);

        struct stat entry_stat;
        // Use lstat to handle symbolic links correctly if desired, but for watching, stat is usually fine.
        // If full_entry_path is a symlink to a directory, stat will give info about the target directory.
        if (stat(full_entry_path, &entry_stat) == -1) {
            xo_utils_console_error("Failed to stat: %s (errno: %d)", full_entry_path, errno);
            continue; // Skip this entry
        }

        if (S_ISDIR(entry_stat.st_mode)) {
            // Recursively add watches for subdirectories
            int result = add_watches_recursive(data, full_entry_path, inotify_fd);
            if (result != XO_SUCCESS) {
                xo_utils_console_error("Failed to add watches recursively for: %s. Continuing...", full_entry_path);
                // Continue adding other directories even if one branch fails.
                // Depending on requirements, one might choose to propagate the error up.
            }
        } else {
            printf("[XO DEBUG] POSIX watcher: Skipped non-directory entry: %s/%s\n", dir_path, entry->d_name);
        }
    }
    closedir(dir);
    return XO_SUCCESS;
}

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
                printf("[XO DEBUG] POSIX watcher: Event raw: wd=%d, mask=0x%X, cookie=0x%X, len=%u, name=%s\n", 
                       event->wd, event->mask, event->cookie, event->len, (event->len ? event->name : "N/A"));
                
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
                    printf("[XO DEBUG] POSIX watcher: Event processed: type=%d, path=%s\n", file_event.type, file_event.filepath);
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
    watch_data->watch_descriptors = NULL; // Will be allocated by add_watches_recursive
    watch_data->watch_paths = NULL;       // Will be allocated by add_watches_recursive
    watch_data->watch_count = 0;
    watch_data->watch_capacity = 0;       // Initial capacity, will be set by add_watches_recursive
    watch_data->running = false;
    
    // Initialize inotify
    watch_data->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (watch_data->inotify_fd < 0) {
        xo_utils_console_error("Failed to initialize inotify");
        free(watch_data);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Note: watcher->watch_paths contains the root paths added by xo_watcher_add_path.
    // These paths are duplicated by strdup in xo_watcher_add_path and owned by the watcher struct.
    // add_watches_recursive will further strdup these paths for its own storage in watch_data->watch_paths.

    printf("[XO DEBUG] POSIX watcher: Starting xo_posix_watcher_start. Number of root paths to process: %zu\n", watcher->path_count);
    for (size_t i = 0; i < watcher->path_count; i++) {
        printf("[XO DEBUG] POSIX watcher: Initializing recursive watch for root path: %s\n", watcher->watch_paths[i]);
        // xo_watcher_add_path ensures that watcher->watch_paths[i] exists and is a directory.
        // add_watches_recursive will also perform its own checks.
        int result = add_watches_recursive(watch_data, watcher->watch_paths[i], watch_data->inotify_fd);
        if (result != XO_SUCCESS) {
            // Log the error and continue to try and watch other specified root paths.
            // The watcher might operate on a subset of user-specified paths if some fail.
            xo_utils_console_error("Failed to set up recursive watch for root path: %s", watcher->watch_paths[i]);
        }
    }

    // Check if any watches were successfully added
    if (watch_data->watch_count == 0) {
        if (watcher->path_count > 0) {
             xo_utils_console_error("No directories could be watched. Watcher not started.");
        } else {
             xo_utils_console_warning("No paths configured for watcher. Watcher not started.");
        }
        close(watch_data->inotify_fd);
        // watch_descriptors and watch_paths might have been allocated if some capacity was set up
        // but no actual watches were successfully added or they were cleaned up.
        free(watch_data->watch_descriptors); 
        free(watch_data->watch_paths);       
        free(watch_data);
        return XO_ERROR_FILE_NOT_FOUND; // Or a more specific error like XO_ERROR_WATCHER_NO_PATHS
    }
    
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
    printf("[XO DEBUG] Watcher callback: Handling event type %d for path: %s\n", event->type, event->filepath);
    
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
    xo_config_t *config = (xo_config_t *)user_data; // Cast user_data to xo_config_t
    xo_server_t *server = (xo_server_t *)config->user_data; // Assuming config->user_data holds the server pointer
    
    // Check if we should rebuild
    if (ext && (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0)) {
        printf("[XO DEBUG] Watcher callback: Markdown file detected. Action: %s\n", (event->type == XO_FILE_DELETED ? "delete" : "build"));
        // This is a markdown file, rebuild it
        xo_utils_console_info("Rebuilding: %s", event->filepath);
        // config is already defined
        
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
    } else if (ext && (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)) {
        bool is_layout_file = false;
        size_t layouts_dir_len = strlen(config->layouts_dir);
        if (strncmp(event->filepath, config->layouts_dir, layouts_dir_len) == 0) {
            char char_after_prefix = event->filepath[layouts_dir_len];
            if (char_after_prefix == PATH_SEPARATOR || char_after_prefix == '\0') {
                is_layout_file = true;
            }
        }

        // Check for partials directory: config->content_dir + "_partials"
        // Example: "content/_partials"
        if (!is_layout_file) {
            char partials_path_check[XO_MAX_PATH];
            // Ensure content_dir ends with a path separator for clean concatenation, or handle if it doesn't.
            // For simplicity, assuming content_dir does not end with a separator for this construction.
            // A more robust solution would use xo_utils_join_path or similar.
            snprintf(partials_path_check, sizeof(partials_path_check), "%s%c_partials", config->content_dir, PATH_SEPARATOR);
            size_t partials_dir_len = strlen(partials_path_check);
            if (strncmp(event->filepath, partials_path_check, partials_dir_len) == 0) {
                 char char_after_prefix_partial = event->filepath[partials_dir_len];
                 if (char_after_prefix_partial == PATH_SEPARATOR || char_after_prefix_partial == '\0') {
                    is_layout_file = true; // Treat partials like layout files for rebuild purposes
                    printf("[XO DEBUG] Watcher callback: Partial HTML file '%s' changed. Treating as layout file.\n", event->filepath);
                 }
            }
        }

        if (is_layout_file) {
            printf("[XO DEBUG] Watcher callback: Layout file '%s' changed. Triggering full content rebuild.\n", event->filepath);
            // If a layout file (or partial) is deleted, we should still rebuild all content.
            // The xo_build_directory function should handle non-existent files gracefully during its traversal.
            // No special handling for XO_FILE_DELETED here for layouts, as the impact is on all files using it.

            xo_dependency_tracker_t tracker;
            xo_dependency_tracker_init(&tracker);
            printf("[XO DEBUG] Watcher callback: Calling xo_build_directory for content_dir: %s\n", config->content_dir);
            if (xo_build_directory(config, config->content_dir, &tracker) != XO_SUCCESS) {
                xo_utils_console_error("Error during full content rebuild triggered by layout change: %s", event->filepath);
            }
            xo_dependency_tracker_free(&tracker);

            printf("[XO DEBUG] Watcher callback: Triggering browser reload for layout change.\n");
            // server is already defined
            if (server) {
                xo_server_broadcast_ws(server, "reload", 6);
            }
        } else {
            // This is a static HTML file (not a layout or partial), reload the browser
            // If a static HTML file is deleted, the browser reload might show a 404, which is acceptable.
            printf("[XO DEBUG] Watcher callback: Static HTML file '%s' changed. Triggering browser reload only.\n", event->filepath);
            // server is already defined
            if (server) {
                xo_server_broadcast_ws(server, "reload", 6);
            }
        }
    } else if (ext && (strcmp(ext, "css") == 0 || strcmp(ext, "js") == 0)) {
        printf("[XO DEBUG] Watcher callback: Static asset (CSS/JS) '%s' changed. Triggering browser reload.\n", event->filepath);
        // This is a static asset (CSS, JS), we should reload the browser
        // server is already defined
        if (server) {
            xo_server_broadcast_ws(server, "reload", 6);
        }
    } else {
        printf("[XO DEBUG] Watcher callback: File type not specifically handled by rebuild/reload logic: %s (ext: %s)\n", event->filepath, ext ? ext : "N/A");
    }
} 