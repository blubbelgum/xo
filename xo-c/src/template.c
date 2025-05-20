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

// Placeholder for template rendering
// This is a very basic implementation that doesn't support most Mustache features
int xo_template_render(const char *template_str, const xo_template_context_t *ctx, 
                      const xo_template_partials_t *partials, char **output) {
    if (!template_str || !ctx || !output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a simplified placeholder that doesn't actually parse templates properly
    // In a real implementation, you would use a proper library or implement parsing
    
    // For now, just return a copy of the template
    *output = strdup(template_str);
    if (!*output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
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