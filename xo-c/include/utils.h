#ifndef XO_UTILS_H
#define XO_UTILS_H

#include "xo.h"

// Path utilities
char *xo_utils_join_path(const char *base, const char *path);
char *xo_utils_dirname(const char *path);
char *xo_utils_basename(const char *path);
char *xo_utils_get_extension(const char *path);
int xo_utils_mkdir_p(const char *path);
bool xo_utils_file_exists(const char *path);
bool xo_utils_dir_exists(const char *path);

// String utilities
char *xo_utils_strdup(const char *str);
char *xo_utils_strndup(const char *str, size_t n);
char *xo_utils_str_replace(const char *str, const char *search, const char *replace);
void xo_utils_str_trim(char *str);
char **xo_utils_str_split(const char *str, const char *delim, size_t *count);
void xo_utils_str_split_free(char **parts, size_t count);

// File utilities
char *xo_utils_read_file(const char *path);
int xo_utils_write_file(const char *path, const char *content);
int xo_utils_copy_file(const char *src, const char *dest);
char **xo_utils_list_files(const char *path, const char *ext, size_t *count);
void xo_utils_list_files_free(char **files, size_t count);

// Hash utilities
char *xo_utils_hash_string(const char *str);
char *xo_utils_hash_file(const char *path);

// Console utilities
void xo_utils_console_info(const char *fmt, ...);
void xo_utils_console_success(const char *fmt, ...);
void xo_utils_console_warning(const char *fmt, ...);
void xo_utils_console_error(const char *fmt, ...);

#endif /* XO_UTILS_H */ 