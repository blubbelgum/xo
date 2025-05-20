#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include "utils.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#include <dirent.h>
#endif

// ANSI color codes
#define ANSI_RESET   "\x1b[0m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"

// ===============================
// Path utilities
// ===============================

char *xo_utils_join_path(const char *base, const char *path) {
    if (!base || !path) {
        return NULL;
    }
    
    size_t base_len = strlen(base);
    size_t path_len = strlen(path);
    size_t needs_separator = (base_len > 0 && base[base_len - 1] != PATH_SEPARATOR && 
                             path_len > 0 && path[0] != PATH_SEPARATOR) ? 1 : 0;
    
    // Allocate memory for the joined path
    char *result = (char *)malloc(base_len + path_len + needs_separator + 1);
    if (!result) {
        return NULL;
    }
    
    // Copy base path
    strcpy(result, base);
    
    // Add separator if needed
    if (needs_separator) {
        result[base_len] = PATH_SEPARATOR;
        strcpy(result + base_len + 1, path);
    } else {
        strcpy(result + base_len, path);
    }
    
    return result;
}

char *xo_utils_dirname(const char *path) {
    if (!path) {
        return NULL;
    }
    
    // Find the last path separator
    const char *last_sep = strrchr(path, PATH_SEPARATOR);
    if (!last_sep) {
        // No separator found, return "."
        char *result = (char *)malloc(2);
        if (!result) {
            return NULL;
        }
        result[0] = '.';
        result[1] = '\0';
        return result;
    }
    
    // Calculate length up to the separator
    size_t len = last_sep - path;
    
    // Special case for root directory
    if (len == 0) {
        char *result = (char *)malloc(2);
        if (!result) {
            return NULL;
        }
        result[0] = PATH_SEPARATOR;
        result[1] = '\0';
        return result;
    }
    
    // Allocate memory and copy the dirname
    char *result = (char *)malloc(len + 1);
    if (!result) {
        return NULL;
    }
    
    strncpy(result, path, len);
    result[len] = '\0';
    
    return result;
}

char *xo_utils_basename(const char *path) {
    if (!path) {
        return NULL;
    }
    
    // Find the last path separator
    const char *last_sep = strrchr(path, PATH_SEPARATOR);
    const char *filename = last_sep ? last_sep + 1 : path;
    
    // Duplicate the filename
    return xo_utils_strdup(filename);
}

char *xo_utils_get_extension(const char *path) {
    if (!path) {
        return NULL;
    }
    
    // Get the basename first
    char *basename = xo_utils_basename(path);
    if (!basename) {
        return NULL;
    }
    
    // Find the last dot
    char *last_dot = strrchr(basename, '.');
    if (!last_dot) {
        free(basename);
        return xo_utils_strdup("");
    }
    
    char *extension = xo_utils_strdup(last_dot + 1);
    free(basename);
    
    return extension;
}

int xo_utils_mkdir_p(const char *path) {
    if (!path || strlen(path) == 0) {
        return -1;
    }
    
    // Copy the path so we can modify it
    char *temp_path = xo_utils_strdup(path);
    if (!temp_path) {
        return -1;
    }
    
    char *p = temp_path;
    
    // Skip leading slashes
    while (*p == PATH_SEPARATOR) {
        p++;
    }
    
    // Traverse the path
    while (*p) {
        // Find the next path separator
        char *next_sep = strchr(p, PATH_SEPARATOR);
        if (next_sep) {
            *next_sep = '\0';  // Temporarily terminate the path
        }
        
        // Create the directory if it doesn't exist
        if (strlen(temp_path) > 0 && !xo_utils_dir_exists(temp_path)) {
            if (mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
                free(temp_path);
                return -1;
            }
        }
        
        // Restore the path separator and continue
        if (next_sep) {
            *next_sep = PATH_SEPARATOR;
            p = next_sep + 1;
        } else {
            break;
        }
    }
    
    free(temp_path);
    return 0;
}

bool xo_utils_file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

bool xo_utils_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

// ===============================
// String utilities
// ===============================

char *xo_utils_strdup(const char *str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char *result = (char *)malloc(len);
    if (!result) {
        return NULL;
    }
    
    return memcpy(result, str, len);
}

char *xo_utils_strndup(const char *str, size_t n) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strnlen(str, n);
    char *result = (char *)malloc(len + 1);
    if (!result) {
        return NULL;
    }
    
    memcpy(result, str, len);
    result[len] = '\0';
    
    return result;
}

void xo_utils_str_trim(char *str) {
    if (!str) {
        return;
    }
    
    // Trim leading whitespace
    char *start = str;
    while (*start && isspace(*start)) {
        start++;
    }
    
    // Shift the string to remove leading whitespace
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    // Trim trailing whitespace
    char *end = str + strlen(str);
    while (end > str && isspace(*(end - 1))) {
        end--;
    }
    *end = '\0';
}

// ===============================
// File utilities
// ===============================

char *xo_utils_read_file(const char *path) {
    if (!path) {
        return NULL;
    }
    
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    
    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    
    // Allocate buffer for file content
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    // Read the file
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    
    fclose(file);
    
    // Check if we read the whole file
    if (read_size != file_size) {
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

int xo_utils_write_file(const char *path, const char *content) {
    if (!path || !content) {
        return -1;
    }
    
    // Ensure the directory exists
    char *dir = xo_utils_dirname(path);
    if (!dir) {
        return -1;
    }
    
    int result = xo_utils_mkdir_p(dir);
    free(dir);
    
    if (result != 0) {
        return -1;
    }
    
    // Open the file for writing
    FILE *file = fopen(path, "wb");
    if (!file) {
        return -1;
    }
    
    // Write the content
    size_t content_len = strlen(content);
    size_t written = fwrite(content, 1, content_len, file);
    
    fclose(file);
    
    return (written == content_len) ? 0 : -1;
}

int xo_utils_copy_file(const char *src, const char *dest) {
    if (!src || !dest) {
        return -1;
    }
    
    // Read the source file
    char *content = xo_utils_read_file(src);
    if (!content) {
        return -1;
    }
    
    // Write to the destination file
    int result = xo_utils_write_file(dest, content);
    
    free(content);
    
    return result;
}

// ===============================
// Console utilities
// ===============================

void xo_utils_console_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    printf("[xo] %s", ANSI_CYAN);
    vprintf(fmt, args);
    printf("%s\n", ANSI_RESET);
    
    va_end(args);
}

void xo_utils_console_success(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    printf("[xo] %s", ANSI_GREEN);
    vprintf(fmt, args);
    printf("%s\n", ANSI_RESET);
    
    va_end(args);
}

void xo_utils_console_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    printf("[xo] %s", ANSI_YELLOW);
    vprintf(fmt, args);
    printf("%s\n", ANSI_RESET);
    
    va_end(args);
}

void xo_utils_console_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    printf("[xo] %s", ANSI_RED);
    vprintf(fmt, args);
    printf("%s\n", ANSI_RESET);
    
    va_end(args);
} 