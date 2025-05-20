#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xo.h"
#include "utils.h"

// Parse command-line arguments and fill the configuration
static int parse_arguments(int argc, char *argv[], xo_config_t *config) {
    // Set default values
    config->command = XO_CMD_DEV;
    strcpy(config->content_dir, "content");
    strcpy(config->layouts_dir, "layouts");
    strcpy(config->output_dir, "dist");
    config->server_port = 3000;
    config->clean_build = false;
    config->running = false;  // Initialize running flag
    config->user_data = NULL; // Initialize user data

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "dev") == 0) {
            config->command = XO_CMD_DEV;
        } else if (strcmp(argv[i], "build") == 0) {
            config->command = XO_CMD_BUILD;
        } else if (strcmp(argv[i], "init") == 0) {
            config->command = XO_CMD_INIT;
        } else if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0) {
            config->command = XO_CMD_HELP;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config->server_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--clean") == 0) {
            config->clean_build = true;
        }
    }

    return XO_SUCCESS;
}

// Print help information
void xo_print_help(void) {
    printf("XO Static Site Generator v%s\n\n", XO_VERSION);
    printf("Usage:\n");
    printf("  xo-c [command] [options]\n\n");
    printf("Commands:\n");
    printf("  dev       Start development server (default)\n");
    printf("  build     Production build\n");
    printf("  init      Create sample site structure\n");
    printf("  help      Show this help\n\n");
    printf("Options:\n");
    printf("  --port    Set development server port\n");
    printf("  --clean   Remove build directory before build\n");
}

// Main entry point
int main(int argc, char *argv[]) {
    xo_config_t config;
    int result;

    // Parse command-line arguments
    result = parse_arguments(argc, argv, &config);
    if (result != XO_SUCCESS) {
        xo_utils_console_error("Failed to parse arguments");
        return 1;
    }

    // Execute the appropriate command
    switch (config.command) {
        case XO_CMD_HELP:
            xo_print_help();
            break;

        case XO_CMD_INIT:
            xo_utils_console_info("Initializing sample project...");
            result = xo_init_project(&config);
            if (result == XO_SUCCESS) {
                xo_utils_console_success("Sample project created successfully!");
            } else {
                xo_utils_console_error("Failed to create sample project");
                return 1;
            }
            break;

        case XO_CMD_BUILD:
            xo_utils_console_info("Building project...");
            result = xo_build(&config);
            if (result == XO_SUCCESS) {
                xo_utils_console_success("Build completed successfully!");
            } else {
                xo_utils_console_error("Build failed");
                return 1;
            }
            break;

        case XO_CMD_DEV:
            xo_utils_console_info("Starting development server on port %d...", config.server_port);
            result = xo_dev_server(&config);
            if (result != XO_SUCCESS) {
                xo_utils_console_error("Development server failed");
                return 1;
            }
            break;

        default:
            xo_utils_console_error("Unknown command");
            xo_print_help();
            return 1;
    }

    return 0;
} 