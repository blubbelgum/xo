#ifndef XO_SERVER_H
#define XO_SERVER_H

#include "xo.h"

// HTTP Method enum
typedef enum {
    XO_HTTP_GET,
    XO_HTTP_POST,
    XO_HTTP_PUT,
    XO_HTTP_DELETE,
    XO_HTTP_OPTIONS
} xo_http_method_t;

// HTTP Request structure
typedef struct {
    xo_http_method_t method;
    char *path;
    char *query_string;
    char **header_keys;
    char **header_values;
    size_t header_count;
    char *body;
    size_t body_length;
} xo_http_request_t;

// HTTP Response structure
typedef struct {
    int status_code;
    char **header_keys;
    char **header_values;
    size_t header_count;
    char *body;
    size_t body_length;
} xo_http_response_t;

// WebSocket Client structure
typedef struct {
    void *handle;
    char *id;
    bool is_connected;
} xo_ws_client_t;

// WebSocket Manager structure
typedef struct {
    xo_ws_client_t *clients;
    size_t client_count;
    size_t client_capacity;
} xo_ws_manager_t;

// Server context
typedef struct {
    void *handle;
    int port;
    bool running;
    xo_ws_manager_t ws_manager;
    const xo_config_t *config;
} xo_server_t;

// Handler function types
typedef int (*xo_http_handler_t)(const xo_http_request_t *request, xo_http_response_t *response, void *user_data);
typedef int (*xo_ws_handler_t)(xo_ws_client_t *client, const char *message, size_t length, void *user_data);

// Function declarations
int xo_server_init(xo_server_t *server, const xo_config_t *config);
void xo_server_free(xo_server_t *server);
int xo_server_start(xo_server_t *server, xo_http_handler_t handler, void *user_data);
int xo_server_stop(xo_server_t *server);
int xo_server_broadcast_ws(xo_server_t *server, const char *message, size_t length);

int xo_http_request_init(xo_http_request_t *request);
void xo_http_request_free(xo_http_request_t *request);
int xo_http_response_init(xo_http_response_t *response);
void xo_http_response_free(xo_http_response_t *response);
int xo_http_response_set_status(xo_http_response_t *response, int status_code);
int xo_http_response_add_header(xo_http_response_t *response, const char *key, const char *value);
int xo_http_response_set_body(xo_http_response_t *response, const char *body, size_t length);

int xo_http_handler(const xo_http_request_t *request, xo_http_response_t *response, void *user_data);
int xo_ws_handler(xo_ws_client_t *client, const char *message, size_t length, void *user_data);

#endif /* XO_SERVER_H */ 