#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "template.h"

// Initialize a template context
int xo_template_context_init(xo_template_context_t *ctx) {
    if (!ctx) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->keys = NULL;
    ctx->values = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
    
    return XO_SUCCESS;
}

// Free resources used by a template context
void xo_template_context_free(xo_template_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    // Free keys
    for (size_t i = 0; i < ctx->count; i++) {
        free(ctx->keys[i]);
        
        // Free string values
        if (ctx->values[i].type == XO_TPLVAL_STRING) {
            free(ctx->values[i].value.string_val);
        }
    }
    
    // Free arrays
    free(ctx->keys);
    free(ctx->values);
    
    // Reset structure
    ctx->keys = NULL;
    ctx->values = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}

// Add a string value to the context
int xo_template_context_add_string(xo_template_context_t *ctx, const char *key, const char *value) {
    if (!ctx || !key || !value) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if we need to resize the arrays
    if (ctx->count >= ctx->capacity) {
        size_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;
        
        char **new_keys = realloc(ctx->keys, new_capacity * sizeof(char *));
        if (!new_keys) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        xo_template_value_t *new_values = realloc(ctx->values, new_capacity * sizeof(xo_template_value_t));
        if (!new_values) {
            free(new_keys);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        ctx->keys = new_keys;
        ctx->values = new_values;
        ctx->capacity = new_capacity;
    }
    
    // Add the new item
    ctx->keys[ctx->count] = strdup(key);
    if (!ctx->keys[ctx->count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->values[ctx->count].type = XO_TPLVAL_STRING;
    ctx->values[ctx->count].value.string_val = strdup(value);
    
    if (!ctx->values[ctx->count].value.string_val) {
        free(ctx->keys[ctx->count]);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->count++;
    
    return XO_SUCCESS;
}

// Add an integer value to the context
int xo_template_context_add_int(xo_template_context_t *ctx, const char *key, int value) {
    if (!ctx || !key) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if we need to resize the arrays
    if (ctx->count >= ctx->capacity) {
        size_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;
        
        char **new_keys = realloc(ctx->keys, new_capacity * sizeof(char *));
        if (!new_keys) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        xo_template_value_t *new_values = realloc(ctx->values, new_capacity * sizeof(xo_template_value_t));
        if (!new_values) {
            free(new_keys);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        ctx->keys = new_keys;
        ctx->values = new_values;
        ctx->capacity = new_capacity;
    }
    
    // Add the new item
    ctx->keys[ctx->count] = strdup(key);
    if (!ctx->keys[ctx->count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->values[ctx->count].type = XO_TPLVAL_INT;
    ctx->values[ctx->count].value.int_val = value;
    
    ctx->count++;
    
    return XO_SUCCESS;
}

// Add a boolean value to the context
int xo_template_context_add_bool(xo_template_context_t *ctx, const char *key, bool value) {
    if (!ctx || !key) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if we need to resize the arrays
    if (ctx->count >= ctx->capacity) {
        size_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;
        
        char **new_keys = realloc(ctx->keys, new_capacity * sizeof(char *));
        if (!new_keys) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        xo_template_value_t *new_values = realloc(ctx->values, new_capacity * sizeof(xo_template_value_t));
        if (!new_values) {
            free(new_keys);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        ctx->keys = new_keys;
        ctx->values = new_values;
        ctx->capacity = new_capacity;
    }
    
    // Add the new item
    ctx->keys[ctx->count] = strdup(key);
    if (!ctx->keys[ctx->count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    ctx->values[ctx->count].type = XO_TPLVAL_BOOL;
    ctx->values[ctx->count].value.bool_val = value;
    
    ctx->count++;
    
    return XO_SUCCESS;
}

// Create a context from a frontmatter structure
int xo_template_context_from_frontmatter(xo_template_context_t *ctx, const xo_frontmatter_t *frontmatter) {
    if (!ctx || !frontmatter) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize the context
    xo_template_context_init(ctx);
    
    // Copy frontmatter items to context
    for (size_t i = 0; i < frontmatter->count; i++) {
        xo_template_context_add_string(ctx, frontmatter->items[i].key, frontmatter->items[i].value);
    }
    
    return XO_SUCCESS;
}

// Initialize template partials
int xo_template_partials_init(xo_template_partials_t *partials) {
    if (!partials) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    partials->names = NULL;
    partials->contents = NULL;
    partials->count = 0;
    partials->capacity = 0;
    
    return XO_SUCCESS;
}

// Free resources used by template partials
void xo_template_partials_free(xo_template_partials_t *partials) {
    if (!partials) {
        return;
    }
    
    for (size_t i = 0; i < partials->count; i++) {
        free(partials->names[i]);
        free(partials->contents[i]);
    }
    
    free(partials->names);
    free(partials->contents);
    
    partials->names = NULL;
    partials->contents = NULL;
    partials->count = 0;
    partials->capacity = 0;
}

// Add a partial to the collection
int xo_template_partials_add(xo_template_partials_t *partials, const char *name, const char *content) {
    if (!partials || !name || !content) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if we need to resize the arrays
    if (partials->count >= partials->capacity) {
        size_t new_capacity = partials->capacity == 0 ? 8 : partials->capacity * 2;
        
        char **new_names = realloc(partials->names, new_capacity * sizeof(char *));
        if (!new_names) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        char **new_contents = realloc(partials->contents, new_capacity * sizeof(char *));
        if (!new_contents) {
            free(new_names);
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        partials->names = new_names;
        partials->contents = new_contents;
        partials->capacity = new_capacity;
    }
    
    // Add the new partial
    partials->names[partials->count] = strdup(name);
    if (!partials->names[partials->count]) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    partials->contents[partials->count] = strdup(content);
    if (!partials->contents[partials->count]) {
        free(partials->names[partials->count]);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    partials->count++;
    
    return XO_SUCCESS;
}

// Find a value in the context by key
static const char *get_context_value(const xo_template_context_t *ctx, const char *key) {
    if (!ctx || !key) {
        return NULL;
    }
    
    // Search for the key
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->keys[i], key) == 0) {
            // Convert the value to string based on its type
            switch (ctx->values[i].type) {
                case XO_TPLVAL_STRING:
                    return ctx->values[i].value.string_val;
                
                case XO_TPLVAL_INT: {
                    // Convert int to string using a static buffer
                    // This is not thread-safe but sufficient for our needs
                    static char int_buffer[32];
                    sprintf(int_buffer, "%d", ctx->values[i].value.int_val);
                    return int_buffer;
                }
                
                case XO_TPLVAL_BOOL:
                    return ctx->values[i].value.bool_val ? "true" : "false";
                
                default:
                    return NULL;
            }
        }
    }
    
    return NULL;
}

// Find a partial by name
static const char *get_partial(const xo_template_partials_t *partials, const char *name) {
    if (!partials || !name) {
        return NULL;
    }
    
    // Search for the partial
    for (size_t i = 0; i < partials->count; i++) {
        if (strcmp(partials->names[i], name) == 0) {
            return partials->contents[i];
        }
    }
    
    return NULL;
}

// Simple template rendering implementation with variable substitution and partials
int xo_template_render(const char *template_str, const xo_template_context_t *ctx, 
                      const xo_template_partials_t *partials, char **output) {
    if (!template_str || !ctx || !output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t template_len = strlen(template_str);
    
    // Initial allocation - we'll resize as needed
    size_t buffer_size = template_len * 2; // Start with double the template size
    char *result = (char *)malloc(buffer_size);
    if (!result) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t result_len = 0;
    result[0] = '\0';
    
    // Process the template
    const char *p = template_str;
    while (*p) {
        // Check for variable or partial tag
        if (*p == '{' && *(p + 1) == '{') {
            // Found the start of a tag
            const char *tag_start = p + 2;
            
            // Check if it's a partial
            bool is_partial = false;
            if (*tag_start == '>') {
                is_partial = true;
                tag_start++;
                // Skip whitespace
                while (isspace((unsigned char)*tag_start)) {
                    tag_start++;
                }
            }
            
            // Look for the end of the tag
            const char *tag_end = strstr(tag_start, "}}");
            if (tag_end) {
                // Extract the tag name
                size_t tag_len = tag_end - tag_start;
                char *tag_name = (char *)malloc(tag_len + 1);
                if (!tag_name) {
                    free(result);
                    return XO_ERROR_MEMORY_ALLOCATION;
                }
                
                strncpy(tag_name, tag_start, tag_len);
                tag_name[tag_len] = '\0';
                
                // Trim whitespace
                char *tag_ptr = tag_name;
                while (isspace((unsigned char)*tag_ptr)) {
                    tag_ptr++;
                }
                
                char *tag_end_ptr = tag_name + strlen(tag_ptr) - 1;
                while (tag_end_ptr > tag_ptr && isspace((unsigned char)*tag_end_ptr)) {
                    *tag_end_ptr = '\0';
                    tag_end_ptr--;
                }
                
                // Get the value or partial
                const char *value = NULL;
                if (is_partial) {
                    value = get_partial(partials, tag_ptr);
                } else {
                    value = get_context_value(ctx, tag_ptr);
                }
                
                if (value) {
                    // Check if we need to resize the result buffer
                    size_t value_len = strlen(value);
                    if (result_len + value_len >= buffer_size) {
                        buffer_size = (result_len + value_len) * 2;
                        char *new_result = (char *)realloc(result, buffer_size);
                        if (!new_result) {
                            free(tag_name);
                            free(result);
                            return XO_ERROR_MEMORY_ALLOCATION;
                        }
                        result = new_result;
                    }
                    
                    // Append the value
                    strcat(result, value);
                    result_len += value_len;
                }
                
                free(tag_name);
                
                // Move past the end of the tag
                p = tag_end + 2;
            } else {
                // No end tag found, just output the character
                if (result_len + 1 >= buffer_size) {
                    buffer_size *= 2;
                    char *new_result = (char *)realloc(result, buffer_size);
                    if (!new_result) {
                        free(result);
                        return XO_ERROR_MEMORY_ALLOCATION;
                    }
                    result = new_result;
                }
                
                result[result_len++] = *p;
                result[result_len] = '\0';
                p++;
            }
        } else {
            // Regular character, just copy it
            if (result_len + 1 >= buffer_size) {
                buffer_size *= 2;
                char *new_result = (char *)realloc(result, buffer_size);
                if (!new_result) {
                    free(result);
                    return XO_ERROR_MEMORY_ALLOCATION;
                }
                result = new_result;
            }
            
            result[result_len++] = *p;
            result[result_len] = '\0';
            p++;
        }
    }
    
    *output = result;
    return XO_SUCCESS;
}

// Render a template file
int xo_template_render_file(const char *template_path, const xo_template_context_t *ctx, 
                           const xo_template_partials_t *partials, char **output) {
    if (!template_path || !ctx || !output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Read the template file
    FILE *file = fopen(template_path, "rb");
    if (!file) {
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    
    // Allocate buffer for template content
    char *template_str = malloc(file_size + 1);
    if (!template_str) {
        fclose(file);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Read the file
    size_t read_size = fread(template_str, 1, file_size, file);
    template_str[read_size] = '\0';
    
    fclose(file);
    
    // Check if we read the whole file
    if (read_size != file_size) {
        free(template_str);
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Render the template
    int result = xo_template_render(template_str, ctx, partials, output);
    
    free(template_str);
    
    return result;
} 