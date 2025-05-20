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

// Helper function to check if a line starts with a string
static bool starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Helper function to calculate required buffer size for a line
static size_t html_line_size(const char *line) {
    size_t line_len = strlen(line);
    // Add extra space for potential HTML tags (rough estimate)
    return line_len * 2 + 100;
}

// Simple markdown to HTML conversion
// This implements a basic subset of markdown (headers, paragraphs, bold, italic, lists)
int xo_markdown_to_html(const xo_markdown_t *md, char **html_output) {
    if (!md || !html_output) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    const char *content = md->content ? md->content : "";
    
    // Initial allocation - we'll resize as needed
    size_t content_len = strlen(content);
    size_t buffer_size = content_len * 2; // Rough estimation
    char *html = (char *)malloc(buffer_size);
    if (!html) {
        return XO_ERROR_MEMORY_ALLOCATION;
    }
    
    size_t html_len = 0;
    html[0] = '\0';
    
    // Process the markdown line by line
    char *line_start = (char *)content;
    char *next_line;
    bool in_list = false;
    
    while (line_start && *line_start) {
        // Find the end of the line
        next_line = strchr(line_start, '\n');
        if (next_line) {
            *next_line = '\0';  // Temporarily terminate the line
        }
        
        // Process the line
        char line_buffer[1024] = {0};
        
        // Trim trailing whitespace
        char *line_end = line_start + strlen(line_start) - 1;
        while (line_end >= line_start && isspace(*line_end)) {
            *line_end = '\0';
            line_end--;
        }
        
        // Skip empty lines
        if (strlen(line_start) == 0) {
            if (in_list) {
                // End the list if we were in one
                strcpy(line_buffer, "</ul>\n");
                in_list = false;
            }
        }
        // Headers
        else if (starts_with(line_start, "# ")) {
            sprintf(line_buffer, "<h1>%s</h1>\n", line_start + 2);
        }
        else if (starts_with(line_start, "## ")) {
            sprintf(line_buffer, "<h2>%s</h2>\n", line_start + 3);
        }
        else if (starts_with(line_start, "### ")) {
            sprintf(line_buffer, "<h3>%s</h3>\n", line_start + 4);
        }
        else if (starts_with(line_start, "#### ")) {
            sprintf(line_buffer, "<h4>%s</h4>\n", line_start + 5);
        }
        else if (starts_with(line_start, "##### ")) {
            sprintf(line_buffer, "<h5>%s</h5>\n", line_start + 6);
        }
        else if (starts_with(line_start, "###### ")) {
            sprintf(line_buffer, "<h6>%s</h6>\n", line_start + 7);
        }
        // List items
        else if (starts_with(line_start, "- ") || starts_with(line_start, "* ")) {
            if (!in_list) {
                // Start a new list
                strcpy(line_buffer, "<ul>\n");
                strcat(line_buffer, "<li>");
                strcat(line_buffer, line_start + 2);
                strcat(line_buffer, "</li>\n");
                in_list = true;
            } else {
                // Continue the list
                strcpy(line_buffer, "<li>");
                strcat(line_buffer, line_start + 2);
                strcat(line_buffer, "</li>\n");
            }
        }
        // Code blocks
        else if (starts_with(line_start, "```")) {
            // Simple code block support
            strcpy(line_buffer, "<pre><code>\n");
            
            // Find the ending ```
            if (next_line) {
                *next_line = '\n';  // Restore newline
                char *code_end = strstr(next_line, "```");
                if (code_end) {
                    // Extract code content
                    char *code_content = next_line + 1;
                    *code_end = '\0';  // Terminate before closing ```
                    
                    // Append code content
                    strcat(line_buffer, code_content);
                    strcat(line_buffer, "</code></pre>\n");
                    
                    // Skip to after the closing ```
                    line_start = code_end + 3;
                    next_line = strchr(line_start, '\n');
                    if (next_line) {
                        *next_line = '\0';
                    }
                    
                    // Skip to next iteration
                    if (next_line) {
                        *next_line = '\n';
                        line_start = next_line + 1;
                    } else {
                        line_start = NULL;
                    }
                    continue;
                }
            }
        }
        // Default to paragraph
        else {
            if (in_list) {
                // End the list if it doesn't continue
                strcpy(line_buffer, "</ul>\n<p>");
                strcat(line_buffer, line_start);
                strcat(line_buffer, "</p>\n");
                in_list = false;
            } else {
                strcpy(line_buffer, "<p>");
                strcat(line_buffer, line_start);
                strcat(line_buffer, "</p>\n");
            }
        }
        
        // Append the processed line to the HTML
        size_t line_buffer_len = strlen(line_buffer);
        
        // Check if we need to resize the buffer
        if (html_len + line_buffer_len >= buffer_size) {
            buffer_size = (html_len + line_buffer_len) * 2;
            char *new_html = (char *)realloc(html, buffer_size);
            if (!new_html) {
                free(html);
                return XO_ERROR_MEMORY_ALLOCATION;
            }
            html = new_html;
        }
        
        // Append the line
        strcat(html, line_buffer);
        html_len += line_buffer_len;
        
        // Restore the newline and move to the next line
        if (next_line) {
            *next_line = '\n';
            line_start = next_line + 1;
        } else {
            line_start = NULL;
        }
    }
    
    // Close any open list
    if (in_list) {
        size_t list_end_len = 6; // Length of "</ul>\n"
        
        // Check if we need to resize the buffer
        if (html_len + list_end_len >= buffer_size) {
            buffer_size = (html_len + list_end_len) * 2;
            char *new_html = (char *)realloc(html, buffer_size);
            if (!new_html) {
                free(html);
                return XO_ERROR_MEMORY_ALLOCATION;
            }
            html = new_html;
        }
        
        strcat(html, "</ul>\n");
        html_len += list_end_len;
    }
    
    *html_output = html;
    return XO_SUCCESS;
} 