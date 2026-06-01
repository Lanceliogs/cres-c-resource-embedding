/*
 * cres.h — C Resource Embedding runtime API
 *
 * Vendorable. Drop cres.h, cres.c, and cres_tool.c into any project.
 * Optionally vendor miniz.c/miniz.h for compression support.
 *
 * Pure C11. No dependencies beyond libc (+ miniz for compressed resources).
 */
#ifndef CRES_H
#define CRES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char    *name;   /* lookup key: path or auto-derived symbol */
    const uint8_t *data;   /* usable data (NULL for compressed entries until cres_load) */
    size_t         size;   /* usable (decompressed) size */
    const uint8_t *cdata;  /* compressed data (NULL if stored raw) */
    size_t         csize;  /* compressed size (0 if stored raw) */
} CResEntry;

/*
 * Lookup by name/path. Linear scan O(n).
 * Returns NULL if not found.
 */
CResEntry *cres_find(CResEntry *table, size_t count, const char *name);

/*
 * Lazy decompression — decompress on demand, no-op for raw entries.
 * cres_load returns 0 on success, -1 on error.
 * cres_unload frees the decompressed buffer, no-op for raw entries.
 */
int  cres_load(CResEntry *entry);
void cres_unload(CResEntry *entry);

/*
 * Bulk load/free — convenience wrappers.
 * cres_load_all returns 0 on success, -1 if any entry failed.
 */
int  cres_load_all(CResEntry *table, size_t count);
void cres_free_all(CResEntry *table, size_t count);

/*
 * Prefix-based load/free — matches against the name/path field.
 * e.g. cres_load_prefix(table, count, "js/") loads all entries
 * whose name starts with "js/".
 */
int  cres_load_prefix(CResEntry *table, size_t count, const char *prefix);
void cres_free_prefix(CResEntry *table, size_t count, const char *prefix);

/*
 * Open an embedded resource as a read-only FILE* stream.
 * Calls cres_load internally if needed. Caller must fclose() the result.
 * Uses fmemopen on POSIX, tmpfile fallback on Windows.
 * Returns NULL if the resource is not found or decompression fails.
 */
FILE *cres_fopen(CResEntry *table, size_t count, const char *name);

/*
 * Guess MIME type from a resource entry's name (by file extension).
 * Returns "application/octet-stream" for unknown extensions.
 */
const char *cres_mime(const CResEntry *entry);

/*
 * Convenience macros for accessing entry data.
 */
#define CRES_DATA(type, entry_ptr) ((const type *)(entry_ptr)->data)
#define CRES_SIZE(entry_ptr)       ((entry_ptr)->size)

#ifdef __cplusplus
}
#endif

#endif /* CRES_H */
