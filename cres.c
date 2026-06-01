/*
 * cres.c — C Resource Embedding runtime implementation
 */
#include "cres.h"
#include <stdlib.h>
#include <string.h>

#ifdef CRES_COMPRESSION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "miniz.h"
#pragma GCC diagnostic pop
#endif

/* --- Lookup ------------------------------------------------------------- */

CResEntry *cres_find(CResEntry *table, size_t count, const char *name)
{
    if (!table || !name) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, name) == 0)
            return &table[i];
    }
    return NULL;
}

/* --- Load / Unload ------------------------------------------------------ */

int cres_load(CResEntry *entry)
{
    if (!entry) return -1;

    /* raw entry — data already points to static array */
    if (!entry->cdata) return 0;

    /* already loaded */
    if (entry->data) return 0;

#ifdef CRES_COMPRESSION
    uint8_t *buf = (uint8_t *)malloc(entry->size);
    if (!buf) return -1;

    mz_ulong out_size = (mz_ulong)entry->size;
    int status = mz_uncompress(buf, &out_size, entry->cdata, (mz_ulong)entry->csize);
    if (status != MZ_OK) {
        free(buf);
        return -1;
    }

    entry->data = buf;
    return 0;
#else
    (void)entry;
    return -1;
#endif
}

void cres_unload(CResEntry *entry)
{
    if (!entry) return;

    /* only free if this was a compressed entry that got loaded */
    if (entry->cdata && entry->data) {
        free((void *)entry->data);
        entry->data = NULL;
    }
}

/* --- Bulk --------------------------------------------------------------- */

int cres_load_all(CResEntry *table, size_t count)
{
    int result = 0;
    for (size_t i = 0; i < count; i++) {
        if (cres_load(&table[i]) != 0)
            result = -1;
    }
    return result;
}

void cres_free_all(CResEntry *table, size_t count)
{
    for (size_t i = 0; i < count; i++)
        cres_unload(&table[i]);
}

/* --- Prefix-based ------------------------------------------------------- */

static int has_prefix(const char *str, const char *prefix)
{
    size_t plen = strlen(prefix);
    return strncmp(str, prefix, plen) == 0;
}

int cres_load_prefix(CResEntry *table, size_t count, const char *prefix)
{
    int result = 0;
    for (size_t i = 0; i < count; i++) {
        if (has_prefix(table[i].name, prefix)) {
            if (cres_load(&table[i]) != 0)
                result = -1;
        }
    }
    return result;
}

void cres_free_prefix(CResEntry *table, size_t count, const char *prefix)
{
    for (size_t i = 0; i < count; i++) {
        if (has_prefix(table[i].name, prefix))
            cres_unload(&table[i]);
    }
}

/* --- FILE* access ------------------------------------------------------- */

FILE *cres_fopen(CResEntry *table, size_t count, const char *name)
{
    CResEntry *e = cres_find(table, count, name);
    if (!e) return NULL;
    if (cres_load(e) != 0) return NULL;

#ifdef _WIN32
    FILE *f = tmpfile();
    if (!f) return NULL;
    if (fwrite(e->data, 1, e->size, f) != e->size) { fclose(f); return NULL; }
    rewind(f);
    return f;
#else
    return fmemopen((void *)e->data, e->size, "r");
#endif
}

/* --- MIME --------------------------------------------------------------- */

static const char *ext_of(const char *name)
{
    const char *dot = NULL;
    for (const char *p = name; *p; p++) {
        if (*p == '.') dot = p;
        if (*p == '/' || *p == '\\') dot = NULL;
    }
    return dot ? dot + 1 : "";
}

static int casecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int d = ((unsigned char)(*a | 0x20)) - ((unsigned char)(*b | 0x20));
        if (d != 0 || !*a) return d;
    }
}

const char *cres_mime(const CResEntry *entry)
{
    static const struct { const char *ext; const char *mime; } map[] = {
        { "html",  "text/html" },
        { "htm",   "text/html" },
        { "css",   "text/css" },
        { "js",    "application/javascript" },
        { "mjs",   "application/javascript" },
        { "json",  "application/json" },
        { "xml",   "application/xml" },
        { "txt",   "text/plain" },
        { "csv",   "text/csv" },
        { "svg",   "image/svg+xml" },
        { "png",   "image/png" },
        { "jpg",   "image/jpeg" },
        { "jpeg",  "image/jpeg" },
        { "gif",   "image/gif" },
        { "webp",  "image/webp" },
        { "ico",   "image/x-icon" },
        { "woff",  "font/woff" },
        { "woff2", "font/woff2" },
        { "ttf",   "font/ttf" },
        { "otf",   "font/otf" },
        { "eot",   "application/vnd.ms-fontobject" },
        { "wasm",  "application/wasm" },
        { "pdf",   "application/pdf" },
        { "zip",   "application/zip" },
        { "mp3",   "audio/mpeg" },
        { "mp4",   "video/mp4" },
        { "webm",  "video/webm" },
    };

    if (!entry || !entry->name) return "application/octet-stream";

    const char *ext = ext_of(entry->name);
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (casecmp(ext, map[i].ext) == 0)
            return map[i].mime;
    }
    return "application/octet-stream";
}
