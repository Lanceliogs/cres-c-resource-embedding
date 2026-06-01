/*
 * cres demo — shows all access patterns:
 *   1. Direct symbol access (compile-time known, O(1))
 *   2. Runtime lookup by name (O(n))
 *   3. Lazy load/unload for compressed resources
 *   4. Prefix-based bulk loading
 */
#include <stdio.h>
#include "resources.h"

int main(void)
{
    /* --- List all resources before loading --- */
    printf("=== All embedded resources ===\n");
    for (size_t i = 0; i < cres_table_count; i++) {
        const CResEntry *e = &cres_table[i];
        printf("  [%zu] %-24s  %6zu bytes  compressed=%s  loaded=%s  %s\n",
               i, e->name, e->size,
               e->cdata ? "yes" : "no ",
               e->data  ? "yes" : "no ",
               cres_mime(e));
    }

    /* --- Direct access to a raw entry (no load needed) --- */
    printf("\n=== Direct access: cres_www_index_html (raw, always available) ===\n");
    printf("Name: %s\n", cres_www_index_html->name);
    printf("Size: %zu bytes\n", CRES_SIZE(cres_www_index_html));
    printf("MIME: %s\n", cres_mime(cres_www_index_html));
    printf("First 80 chars:\n%.80s\n", CRES_DATA(char, cres_www_index_html));

    /* --- Lazy load a compressed entry --- */
    printf("\n=== Lazy load: cres_www_js_app_js (compressed) ===\n");
    printf("Before load: data=%s\n", cres_www_js_app_js->data ? "available" : "NULL");

    if (cres_load(cres_www_js_app_js) == 0) {
        printf("After load:  data=%s, %zu bytes\n",
               cres_www_js_app_js->data ? "available" : "NULL",
               cres_www_js_app_js->size);
        printf("Content:\n%.*s\n", (int)cres_www_js_app_js->size,
               CRES_DATA(char, cres_www_js_app_js));
        cres_unload(cres_www_js_app_js);
        printf("After unload: data=%s\n",
               cres_www_js_app_js->data ? "available" : "NULL");
    } else {
        printf("Failed to decompress!\n");
    }

    /* --- Prefix-based bulk loading --- */
    printf("\n=== Prefix load: \"www/css/\" ===\n");
    cres_load_prefix(cres_table, cres_table_count, "www/css/");
    for (size_t i = 0; i < cres_table_count; i++) {
        const CResEntry *e = &cres_table[i];
        if (e->data)
            printf("  %-24s  loaded, %zu bytes\n", e->name, e->size);
    }
    cres_free_prefix(cres_table, cres_table_count, "www/css/");

    /* --- Runtime lookup --- */
    printf("\n=== Runtime lookup (cres_find) ===\n");
    const char *paths[] = { "www/index.html", "www/js/app.js", "nonexistent.txt" };
    for (int i = 0; i < 3; i++) {
        CResEntry *e = cres_find(cres_table, cres_table_count, paths[i]);
        if (e) {
            printf("  %-24s  found, %zu bytes, %s\n",
                   e->name, e->size, cres_mime(e));
        } else {
            printf("  %-24s  NOT FOUND\n", paths[i]);
        }
    }

    /* --- FILE* access --- */
    printf("\n=== FILE* access (cres_fopen) ===\n");
    FILE *f = cres_fopen(cres_table, cres_table_count, "www/index.html");
    if (f) {
        char buf[128];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        printf("Read %zu bytes via fread:\n%s...\n", n, buf);
        fclose(f);
    }

    /* --- Clean up all --- */
    cres_free_all(cres_table, cres_table_count);

    return 0;
}
