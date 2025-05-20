#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "markdown.h"
#include "utils.h"

// Initialize a markdown structure
int xo_markdown_init(xo_markdown_t *md) {
    if (!md) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize frontmatter
    md->frontmatter.items = NULL;
    md->frontmatter.count = 0;
    md->frontmatter.capacity = 0;
    
    // Initialize content
    md->content = NULL;
    
    return XO_SUCCESS;
}

// Free resources used by a markdown structure
void xo_markdown_free(xo_markdown_t *md) {
    if (!md) {
        return;
    }
    
    // Free frontmatter items
    for (size_t i = 0; i < md->frontmatter.count; i++) {
        free(md->frontmatter.items[i].key);
        free(md->frontmatter.items[i].value);
    }
    
    // Free frontmatter array
    free(md->frontmatter.items);
    
    // Free content
    free(md->content);
    
    // Reset structure
    md->frontmatter.items = NULL;
    md->frontmatter.count = 0;
    md->frontmatter.capacity = 0;
    md->content = NULL;
}

// Add an item to the frontmatter
static int xo_frontmatter_add(xo_frontmatter_t *frontmatter, const char *key, const char *value) {
    if (!frontmatter || !key || !value) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Check if we need to resize the array
    if (frontmatter->count >= frontmatter->capacity) {
        size_t new_capacity = frontmatter->capacity == 0 ? 8 : frontmatter->capacity * 2;
        xo_frontmatter_item_t *new_items = realloc(frontmatter->items, new_capacity * sizeof(xo_frontmatter_item_t));
        
        if (!new_items) {
            return XO_ERROR_MEMORY_ALLOCATION;
        }
        
        frontmatter->items = new_items;
        frontmatter->capacity = new_capacity;
    }
    
    // Add the new item
    frontmatter->items[frontmatter->count].key = xo_utils_strdup(key);
    frontmatter->items[frontmatter->count].value = xo_utils_strdup(value);
    
    if (!frontmatter->items[frontmatter->count].key || !frontmatter->items[frontmatter->count].value) {
        free(frontmatter->items[frontmatter->count].key);
        free(frontmatter->items[frontmatter->count].value);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    frontmatter->count++;
    
    return XO_SUCCESS;
}

// Basic implementation of frontmatter parsing
// This is a simplified version for now
int xo_markdown_parse_file(const char *filepath, xo_markdown_t *md) {
    if (!filepath || !md) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // Read the file content
    char *file_content = xo_utils_read_file(filepath);
    if (!file_content) {
        return XO_ERROR_FILE_NOT_FOUND;
    }
    
    // Initialize the markdown structure
    xo_markdown_init(md);
    
    // Check for frontmatter (delimited by ---)
    char *content_start = file_content;
    if (strncmp(content_start, "---", 3) == 0) {
        // Found frontmatter start
        char *frontmatter_end = strstr(content_start + 3, "---");
        if (frontmatter_end) {
            // Extract and parse frontmatter
            *frontmatter_end = '\0';  // Temporarily terminate frontmatter
            
            // Simple line-by-line parsing
            char *line = content_start + 4;  // Skip "---\n"
            char *next_line;
            
            while (line && *line) {
                // Find the end of the line
                next_line = strchr(line, '\n');
                if (next_line) {
                    *next_line = '\0';  // Temporarily terminate the line
                }
                
                // Process the line if it's not empty
                if (*line) {
                    // Find the colon separator
                    char *colon = strchr(line, ':');
                    if (colon) {
                        *colon = '\0';  // Temporarily terminate the key
                        char *key = line;
                        char *value = colon + 1;
                        
                        // Trim whitespace
                        xo_utils_str_trim(key);
                        xo_utils_str_trim(value);
                        
                        // Add to frontmatter
                        xo_frontmatter_add(&md->frontmatter, key, value);
                        
                        // Restore the colon
                        *colon = ':';
                    }
                }
                
                // Restore the newline and move to the next line
                if (next_line) {
                    *next_line = '\n';
                    line = next_line + 1;
                } else {
                    line = NULL;
                }
            }
            
            // Restore the frontmatter end marker
            *frontmatter_end = '-';
            
            // Set content to start after frontmatter
            content_start = frontmatter_end + 4;  // Skip "---\n"
        }
    }
    
    // Set the content
    md->content = xo_utils_strdup(content_start);
    if (!md->content) {
        xo_markdown_free(md);
        free(file_content);
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    free(file_content);
    return XO_SUCCESS;
}

// Get a frontmatter value by key
const char *xo_markdown_get_frontmatter(const xo_markdown_t *md, const char *key) {
    if (!md || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < md->frontmatter.count; i++) {
        if (strcmp(md->frontmatter.items[i].key, key) == 0) {
            return md->frontmatter.items[i].value;
        }
    }
    
    return NULL;
}

// Placeholder for markdown to HTML conversion
// This would ideally use a proper markdown library like cmark
int xo_markdown_to_html(const xo_markdown_t *md, char **html_output) {
    if (!md || !html_output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    // This is a very basic placeholder that doesn't actually parse markdown
    // In a real implementation, you would use a proper library or implement parsing
    
    // For now, just wrap content in basic HTML
    const char *content = md->content ? md->content : "";
    size_t content_len = strlen(content);
    size_t output_len = content_len + 1;  // + 1 for null terminator
    
    *html_output = (char *)malloc(output_len);
    if (!*html_output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    strcpy(*html_output, content);
    
    return XO_SUCCESS;
} 