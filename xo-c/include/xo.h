#ifndef XO_H
#define XO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Platform-specific definitions
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #define PATH_SEPARATOR '\\'
#else
    #include <unistd.h>
    #define PATH_SEPARATOR '/'
#endif

// Version information
#define XO_VERSION "0.1.0"

// Error codes
#define XO_SUCCESS 0
#define XO_ERROR_FILE_NOT_FOUND 1
#define XO_ERROR_MEMORY_ALLOCATION 2
#define XO_ERROR_INVALID_FORMAT 3
#define XO_ERROR_SERVER 4

// Maximum path length
#define XO_MAX_PATH 1024

// Command types
typedef enum {
    XO_CMD_DEV,
    XO_CMD_BUILD,
    XO_CMD_INIT,
    XO_CMD_HELP
} xo_command_t;

// Configuration structure
typedef struct {
    xo_command_t command;
    char content_dir[XO_MAX_PATH];
    char layouts_dir[XO_MAX_PATH];
    char output_dir[XO_MAX_PATH];
    int server_port;
    bool clean_build;
    bool running;         // Flag for controlling the dev server
    void *user_data;      // User data for callbacks
} xo_config_t;

// Function declarations
void xo_print_help(void);
int xo_init_project(const xo_config_t *config);
int xo_build(const xo_config_t *config);
int xo_dev_server(const xo_config_t *config);

#endif /* XO_H */ 