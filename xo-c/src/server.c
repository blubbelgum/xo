#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"
#include "utils.h"

// Initialize the server context
int xo_server_init(xo_server_t *server, const xo_config_t *config) {
    if (!server || !config) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    server->handle = NULL;
    server->port = config->server_port;
    server->running = false;
    server->config = config;
    
    // Initialize WebSocket manager
    server->ws_manager.clients = NULL;
    server->ws_manager.client_count = 0;
    server->ws_manager.client_capacity = 0;
    
    return XO_SUCCESS;
}

// Free resources used by the server
void xo_server_free(xo_server_t *server) {
    if (!server) {
        return;
    }
    
    // Stop the server if it's running
    if (server->running) {
        xo_server_stop(server);
    }
    
    // Free WebSocket clients
    for (size_t i = 0; i < server->ws_manager.client_count; i++) {
        free(server->ws_manager.clients[i].id);
    }
    
    free(server->ws_manager.clients);
    
    // Reset structure
    server->handle = NULL;
    server->port = 0;
    server->running = false;
    server->config = NULL;
    server->ws_manager.clients = NULL;
    server->ws_manager.client_count = 0;
    server->ws_manager.client_capacity = 0;
}

// Start the server with the given handler
int xo_server_start(xo_server_t *server, xo_http_handler_t handler, void *user_data) {
    if (!server || !handler) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a placeholder that doesn't actually start a server
    xo_utils_console_info("Server would start on port %d", server->port);
    server->running = true;
    
    return XO_SUCCESS;
}

// Stop the server
int xo_server_stop(xo_server_t *server) {
    if (!server) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a placeholder that doesn't actually stop a server
    xo_utils_console_info("Server would stop");
    server->running = false;
    
    return XO_SUCCESS;
}

// Broadcast a message to all WebSocket clients
int xo_server_broadcast_ws(xo_server_t *server, const char *message, size_t length) {
    if (!server || !message) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a placeholder that doesn't actually broadcast anything
    xo_utils_console_info("Would broadcast: %.*s", (int)length, message);
    
    return XO_SUCCESS;
}

// Initialize an HTTP request structure
int xo_http_request_init(xo_http_request_t *request) {
    if (!request) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    request->method = XO_HTTP_GET;
    request->path = NULL;
    request->query_string = NULL;
    request->header_keys = NULL;
    request->header_values = NULL;
    request->header_count = 0;
    request->body = NULL;
    request->body_length = 0;
    
    return XO_SUCCESS;
}

// Free resources used by an HTTP request
void xo_http_request_free(xo_http_request_t *request) {
    if (!request) {
        return;
    }
    
    free(request->path);
    free(request->query_string);
    
    for (size_t i = 0; i < request->header_count; i++) {
        free(request->header_keys[i]);
        free(request->header_values[i]);
    }
    
    free(request->header_keys);
    free(request->header_values);
    free(request->body);
    
    // Reset structure
    request->method = XO_HTTP_GET;
    request->path = NULL;
    request->query_string = NULL;
    request->header_keys = NULL;
    request->header_values = NULL;
    request->header_count = 0;
    request->body = NULL;
    request->body_length = 0;
}

// Initialize an HTTP response structure
int xo_http_response_init(xo_http_response_t *response) {
    if (!response) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    response->status_code = 200;
    response->header_keys = NULL;
    response->header_values = NULL;
    response->header_count = 0;
    response->body = NULL;
    response->body_length = 0;
    
    return XO_SUCCESS;
}

// Free resources used by an HTTP response
void xo_http_response_free(xo_http_response_t *response) {
    if (!response) {
        return;
    }
    
    for (size_t i = 0; i < response->header_count; i++) {
        free(response->header_keys[i]);
        free(response->header_values[i]);
    }
    
    free(response->header_keys);
    free(response->header_values);
    free(response->body);
    
    // Reset structure
    response->status_code = 200;
    response->header_keys = NULL;
    response->header_values = NULL;
    response->header_count = 0;
    response->body = NULL;
    response->body_length = 0;
}

// Set the response status code
int xo_http_response_set_status(xo_http_response_t *response, int status_code) {
    if (!response) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    response->status_code = status_code;
    
    return XO_SUCCESS;
}

// Add a header to the response
int xo_http_response_add_header(xo_http_response_t *response, const char *key, const char *value) {
    if (!response || !key || !value) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Resize header arrays if needed
    size_t new_count = response->header_count + 1;
    char **new_keys = realloc(response->header_keys, new_count * sizeof(char *));
    if (!new_keys) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    char **new_values = realloc(response->header_values, new_count * sizeof(char *));
    if (!new_values) {
        free(new_keys);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    response->header_keys = new_keys;
    response->header_values = new_values;
    
    // Add the new header
    response->header_keys[response->header_count] = strdup(key);
    if (!response->header_keys[response->header_count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    response->header_values[response->header_count] = strdup(value);
    if (!response->header_values[response->header_count]) {
        free(response->header_keys[response->header_count]);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    response->header_count = new_count;
    
    return XO_SUCCESS;
}

// Set the response body
int xo_http_response_set_body(xo_http_response_t *response, const char *body, size_t length) {
    if (!response || !body) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Free existing body
    free(response->body);
    
    // Allocate and copy new body
    response->body = malloc(length);
    if (!response->body) {
        response->body_length = 0;
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    memcpy(response->body, body, length);
    response->body_length = length;
    
    return XO_SUCCESS;
}

// Example HTTP handler
int xo_http_handler(const xo_http_request_t *request, xo_http_response_t *response, void *user_data) {
    if (!request || !response) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Set response status
    xo_http_response_set_status(response, 200);
    
    // Add headers
    xo_http_response_add_header(response, "Content-Type", "text/html");
    
    // Set body
    const char *body = "<html><body><h1>Hello from XO-C</h1></body></html>";
    xo_http_response_set_body(response, body, strlen(body));
    
    return XO_SUCCESS;
}

// Example WebSocket handler
int xo_ws_handler(xo_ws_client_t *client, const char *message, size_t length, void *user_data) {
    if (!client || !message) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a placeholder that doesn't actually handle WebSocket messages
    xo_utils_console_info("WebSocket message from client %s: %.*s", client->id, (int)length, message);
    
    return XO_SUCCESS;
}