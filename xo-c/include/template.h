#ifndef XO_TEMPLATE_H
#define XO_TEMPLATE_H

#include "xo.h"
#include "markdown.h"

// Template context value types
typedef enum {
    XO_TPLVAL_STRING,
    XO_TPLVAL_INT,
    XO_TPLVAL_BOOL
} xo_template_value_type_t;

// Template context value
typedef struct {
    xo_template_value_type_t type;
    union {
        char *string_val;
        int int_val;
        bool bool_val;
    } value;
} xo_template_value_t;

// Template context
typedef struct {
    char **keys;
    xo_template_value_t *values;
    size_t count;
    size_t capacity;
} xo_template_context_t;

// Template partials
typedef struct {
    char **names;
    char **contents;
    size_t count;
    size_t capacity;
} xo_template_partials_t;

// Function declarations
int xo_template_context_init(xo_template_context_t *ctx);
void xo_template_context_free(xo_template_context_t *ctx);
int xo_template_context_add_string(xo_template_context_t *ctx, const char *key, const char *value);
int xo_template_context_add_int(xo_template_context_t *ctx, const char *key, int value);
int xo_template_context_add_bool(xo_template_context_t *ctx, const char *key, bool value);
int xo_template_context_from_frontmatter(xo_template_context_t *ctx, const xo_frontmatter_t *frontmatter);

int xo_template_partials_init(xo_template_partials_t *partials);
void xo_template_partials_free(xo_template_partials_t *partials);
int xo_template_partials_add(xo_template_partials_t *partials, const char *name, const char *content);
int xo_template_partials_load_dir(xo_template_partials_t *partials, const char *dir_path);

int xo_template_render(const char *template_str, const xo_template_context_t *ctx, 
                      const xo_template_partials_t *partials, char **output);
int xo_template_render_file(const char *template_path, const xo_template_context_t *ctx, 
                           const xo_template_partials_t *partials, char **output);

#endif /* XO_TEMPLATE_H */ 