#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    
    // Windows threading
    typedef HANDLE thread_handle_t;
    typedef DWORD WINAPI thread_func_t(LPVOID);
    #define thread_create(handle, func, arg) (((*(handle)) = CreateThread(NULL, 0, (func), (arg), 0, NULL)) == NULL)
    #define thread_join(handle) WaitForSingleObject((handle), INFINITE); CloseHandle((handle))
    
    // Windows compatibility
    #define close closesocket
    typedef SOCKET socket_t;
    #define SOCKET_ERROR_VAL INVALID_SOCKET
    #define strcasecmp _stricmp
#else
    #include <pthread.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <strings.h>
    
    // POSIX threading
    typedef pthread_t thread_handle_t;
    typedef void *(*thread_func_t)(void *);
    #define thread_create(handle, func, arg) pthread_create((handle), NULL, (func), (arg))
    #define thread_join(handle) pthread_join((handle), NULL)
    
    typedef int socket_t;
    #define SOCKET_ERROR_VAL -1
#endif

#include "server.h"
#include "utils.h"

// Server structure including the socket
typedef struct {
    socket_t server_socket;
    thread_handle_t server_thread;
    bool running;
    xo_http_handler_t handler;
    void *user_data;
} xo_server_socket_t;

// WebSocket client structure
typedef struct {
    socket_t client_socket;
    char id[64];
    bool is_connected;
} xo_ws_client_socket_t;

// Thread function for accepting connections
#ifdef _WIN32
static DWORD WINAPI xo_server_thread_func(LPVOID arg) {
#else
static void *xo_server_thread_func(void *arg) {
#endif
    xo_server_t *server = (xo_server_t *)arg;
    xo_server_socket_t *socket_server = (xo_server_socket_t *)server->handle;
    
    while (socket_server->running) {
        // Accept a client connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        socket_t client_socket = accept(socket_server->server_socket, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_socket == SOCKET_ERROR_VAL) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) {
#else
            if (errno == EINTR) {
#endif
                // Interrupted, check if we should exit
                if (!socket_server->running) {
                    break;
                }
                continue;
            }
            
            xo_utils_console_error("Failed to accept client connection");
            continue;
        }
        
        // Handle the HTTP request
        xo_http_request_t request;
        xo_http_request_init(&request);
        
        // Parse the request (simplified)
        char buffer[8192] = {0};
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            // Set the request method
            if (strncmp(buffer, "GET", 3) == 0) {
                request.method = XO_HTTP_GET;
            } else if (strncmp(buffer, "POST", 4) == 0) {
                request.method = XO_HTTP_POST;
            } else if (strncmp(buffer, "PUT", 3) == 0) {
                request.method = XO_HTTP_PUT;
            } else if (strncmp(buffer, "DELETE", 6) == 0) {
                request.method = XO_HTTP_DELETE;
            }
            
            // Extract the path
            char *path_start = strchr(buffer, ' ');
            if (path_start) {
                path_start++; // Skip the space
                char *path_end = strchr(path_start, ' ');
                if (path_end) {
                    size_t path_len = path_end - path_start;
                    
                    // Check for query string
                    char *query_start = strchr(path_start, '?');
                    if (query_start && query_start < path_end) {
                        // We have a query string
                        size_t url_len = query_start - path_start;
                        request.path = malloc(url_len + 1);
                        if (request.path) {
                            strncpy(request.path, path_start, url_len);
                            request.path[url_len] = '\0';
                        }
                        
                        size_t query_len = path_end - query_start - 1;
                        request.query_string = malloc(query_len + 1);
                        if (request.query_string) {
                            strncpy(request.query_string, query_start + 1, query_len);
                            request.query_string[query_len] = '\0';
                        }
                    } else {
                        // No query string
                        request.path = malloc(path_len + 1);
                        if (request.path) {
                            strncpy(request.path, path_start, path_len);
                            request.path[path_len] = '\0';
                        }
                    }
                }
            }
            
            // Call the handler
            xo_http_response_t response;
            xo_http_response_init(&response);
            
            // Call the callback
            if (socket_server->handler) {
                socket_server->handler(&request, &response, socket_server->user_data);
            } else {
                // Default 404 response
                response.status_code = 404;
                const char *not_found = "<html><body><h1>404 Not Found</h1></body></html>";
                xo_http_response_set_body(&response, not_found, strlen(not_found));
            }
            
            // Send the response
            char status_line[128];
            const char *status_text = "OK";
            
            switch (response.status_code) {
                case 200: status_text = "OK"; break;
                case 201: status_text = "Created"; break;
                case 204: status_text = "No Content"; break;
                case 400: status_text = "Bad Request"; break;
                case 404: status_text = "Not Found"; break;
                case 500: status_text = "Internal Server Error"; break;
                default: status_text = "Unknown"; break;
            }
            
            snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", response.status_code, status_text);
            send(client_socket, status_line, strlen(status_line), 0);
            
            // Send headers
            const char *content_type = "text/html";
            char content_length[64];
            snprintf(content_length, sizeof(content_length), "Content-Length: %zu\r\n", response.body_length);
            
            // Content type header
            for (size_t i = 0; i < response.header_count; i++) {
                if (strcasecmp(response.header_keys[i], "Content-Type") == 0) {
                    content_type = response.header_values[i];
                    break;
                }
            }
            
            char content_type_header[128];
            snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s\r\n", content_type);
            send(client_socket, content_type_header, strlen(content_type_header), 0);
            
            // Send content length
            send(client_socket, content_length, strlen(content_length), 0);
            
            // Send other headers
            for (size_t i = 0; i < response.header_count; i++) {
                if (strcasecmp(response.header_keys[i], "Content-Type") != 0) {
                    char header[1024];
                    snprintf(header, sizeof(header), "%s: %s\r\n", response.header_keys[i], response.header_values[i]);
                    send(client_socket, header, strlen(header), 0);
                }
            }
            
            // End of headers
            send(client_socket, "\r\n", 2, 0);
            
            // Send body
            if (response.body_length > 0 && response.body) {
                send(client_socket, response.body, response.body_length, 0);
            }
            
            // Free the response resources
            xo_http_response_free(&response);
        }
        
        // Free the request resources
        xo_http_request_free(&request);
        
        // Close the client socket
        close(client_socket);
    }
    
    return 0;
}

// Initialize the server context
int xo_server_init(xo_server_t *server, const xo_config_t *config) {
    if (!server || !config) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize Windows sockets if needed
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        xo_utils_console_error("Failed to initialize Winsock");
        return XO_ERROR_SERVER;
    }
#endif
    
    // Allocate and initialize server socket structure
    xo_server_socket_t *socket_server = malloc(sizeof(xo_server_socket_t));
    if (!socket_server) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    socket_server->server_socket = SOCKET_ERROR_VAL;
    socket_server->running = false;
    socket_server->handler = NULL;
    socket_server->user_data = NULL;
    
    server->handle = socket_server;
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
    
    // Free the server socket structure
    free(server->handle);
    
    // Free WebSocket clients
    for (size_t i = 0; i < server->ws_manager.client_count; i++) {
        free(server->ws_manager.clients[i].id);
    }
    
    free(server->ws_manager.clients);
    
    // Cleanup Windows sockets if needed
#ifdef _WIN32
    WSACleanup();
#endif
    
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
    
    xo_server_socket_t *socket_server = (xo_server_socket_t *)server->handle;
    
    if (socket_server->running) {
        xo_utils_console_warning("Server is already running");
        return XO_SUCCESS;
    }
    
    // Create socket
    socket_server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_server->server_socket == SOCKET_ERROR_VAL) {
        xo_utils_console_error("Failed to create server socket");
        return XO_ERROR_SERVER;
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(socket_server->server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0) {
        xo_utils_console_error("Failed to set socket options");
        close(socket_server->server_socket);
        socket_server->server_socket = SOCKET_ERROR_VAL;
        return XO_ERROR_SERVER;
    }
    
    // Bind socket to address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server->port);
    
    if (bind(socket_server->server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        xo_utils_console_error("Failed to bind server socket to port %d", server->port);
        close(socket_server->server_socket);
        socket_server->server_socket = SOCKET_ERROR_VAL;
        return XO_ERROR_SERVER;
    }
    
    // Listen for connections
    if (listen(socket_server->server_socket, 10) < 0) {
        xo_utils_console_error("Failed to listen on server socket");
        close(socket_server->server_socket);
        socket_server->server_socket = SOCKET_ERROR_VAL;
        return XO_ERROR_SERVER;
    }
    
    // Set the handler and user data
    socket_server->handler = handler;
    socket_server->user_data = user_data;
    socket_server->running = true;
    
    // Start the server thread
    if (thread_create(&socket_server->server_thread, xo_server_thread_func, server) != 0) {
        xo_utils_console_error("Failed to start server thread");
        close(socket_server->server_socket);
        socket_server->server_socket = SOCKET_ERROR_VAL;
        socket_server->running = false;
        return XO_ERROR_SERVER;
    }
    
    server->running = true;
    xo_utils_console_success("Server started on http://localhost:%d", server->port);
    
    return XO_SUCCESS;
}

// Stop the server
int xo_server_stop(xo_server_t *server) {
    if (!server) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    xo_server_socket_t *socket_server = (xo_server_socket_t *)server->handle;
    
    if (!socket_server->running) {
        xo_utils_console_warning("Server is not running");
        return XO_SUCCESS;
    }
    
    // Set running flag to false
    socket_server->running = false;
    
    // Close the server socket
#ifdef _WIN32
    // Shutdown socket
    shutdown(socket_server->server_socket, SD_BOTH);
#else
    // Shutdown socket
    shutdown(socket_server->server_socket, SHUT_RDWR);
#endif
    
    // Close socket
    close(socket_server->server_socket);
    socket_server->server_socket = SOCKET_ERROR_VAL;
    
    // Wait for server thread to exit
    thread_join(socket_server->server_thread);
    
    server->running = false;
    xo_utils_console_info("Server stopped");
    
    return XO_SUCCESS;
}

// Broadcast a message to all WebSocket clients
int xo_server_broadcast_ws(xo_server_t *server, const char *message, size_t length) {
    if (!server || !message) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Log the message but don't actually send it since we're not implementing WebSockets
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

// HTTP handler for the development server
int xo_http_handler(const xo_http_request_t *request, xo_http_response_t *response, void *user_data) {
    if (!request || !response || !user_data) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    xo_server_t *server = (xo_server_t *)user_data;
    const xo_config_t *config = server->config;
    
    // Default path is index.html
    char *path = strdup(request->path ? request->path : "/");
    if (!path) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Remove trailing slash for directories
    if (path[strlen(path) - 1] == '/') {
        path[strlen(path) - 1] = '\0';
    }
    
    // Default to index.html for empty path
    if (path[0] == '\0' || strcmp(path, "/") == 0) {
        free(path);
        path = strdup("/index.html");
        if (!path) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
    }
    
    // Construct the full file path
    char full_path[XO_MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", config->output_dir, path);
    
    // Try to read the file
    char *content = xo_utils_read_file(full_path);
    
    if (!content) {
        // File not found, return 404
        xo_http_response_set_status(response, 404);
        const char *not_found = "<html><body><h1>404 Not Found</h1></body></html>";
        xo_http_response_set_body(response, not_found, strlen(not_found));
        free(path);
        return XO_SUCCESS;
    }
    
    // Set Content-Type based on file extension
    const char *ext = xo_utils_get_extension(path);
    if (ext) {
        if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
            xo_http_response_add_header(response, "Content-Type", "text/html");
        } else if (strcmp(ext, "css") == 0) {
            xo_http_response_add_header(response, "Content-Type", "text/css");
        } else if (strcmp(ext, "js") == 0) {
            xo_http_response_add_header(response, "Content-Type", "application/javascript");
        } else if (strcmp(ext, "json") == 0) {
            xo_http_response_add_header(response, "Content-Type", "application/json");
        } else if (strcmp(ext, "png") == 0) {
            xo_http_response_add_header(response, "Content-Type", "image/png");
        } else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
            xo_http_response_add_header(response, "Content-Type", "image/jpeg");
        } else if (strcmp(ext, "gif") == 0) {
            xo_http_response_add_header(response, "Content-Type", "image/gif");
        } else if (strcmp(ext, "svg") == 0) {
            xo_http_response_add_header(response, "Content-Type", "image/svg+xml");
        } else if (strcmp(ext, "ico") == 0) {
            xo_http_response_add_header(response, "Content-Type", "image/x-icon");
        } else {
            xo_http_response_add_header(response, "Content-Type", "application/octet-stream");
        }
    } else {
        xo_http_response_add_header(response, "Content-Type", "text/plain");
    }
    
    // Send the file content
    xo_http_response_set_body(response, content, strlen(content));
    
    // Clean up
    free(content);
    free(path);
    
    return XO_SUCCESS;
}

// WebSocket handler (placeholder, not implemented)
int xo_ws_handler(xo_ws_client_t *client, const char *message, size_t length, void *user_data) {
    if (!client || !message || !user_data) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Not implemented - we're just using a simple HTTP server
    
    return XO_SUCCESS;
}