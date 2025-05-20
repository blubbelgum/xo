#ifndef XO_MARKDOWN_H
#define XO_MARKDOWN_H

#include "xo.h"

// Frontmatter key-value pair
typedef struct {
    char *key;
    char *value;
} xo_frontmatter_item_t;

// Frontmatter structure
typedef struct {
    xo_frontmatter_item_t *items;
    size_t count;
    size_t capacity;
} xo_frontmatter_t;

// Markdown document structure
typedef struct {
    xo_frontmatter_t frontmatter;
    char *content;
} xo_markdown_t;

// Function declarations
int xo_markdown_init(xo_markdown_t *md);
void xo_markdown_free(xo_markdown_t *md);
int xo_markdown_parse_file(const char *filepath, xo_markdown_t *md);
int xo_markdown_to_html(const xo_markdown_t *md, char **html_output);
const char *xo_markdown_get_frontmatter(const xo_markdown_t *md, const char *key);

#endif /* XO_MARKDOWN_H */ 