# cres — C Resource Embedding

Embed files and entire directories into C executables at compile time. Access them at runtime by symbol or path lookup, with optional zlib compression and lazy decompression.

Pure C11. No dependencies beyond libc. Vendorable as a handful of files.

It's really just an experiment to build tiny pieces of software with Embedded resources. The first thing I'll do with it is some kind of self-extracting installer for some other projects I have where I don't want to use Inno Setup (which I use and like a lot by the way. I just want to try something else).

## Features

- Embed individual files or entire directory trees
- Optional per-file compression (deflate via miniz, levels 1–9)
- Lazy decompression — only decompress when accessed, free when done
- Bulk load/free by path prefix
- Direct O(1) symbol access and O(n) runtime path lookup
- `FILE*` access via `cres_fopen()` (fmemopen on POSIX, tmpfile fallback on Windows)
- Built-in MIME type detection for HTTP serving
- Works with GCC, Clang, MinGW — any C11 compiler

## Quick start

```bash
# 1. Build the tool
gcc -std=c11 -DCRES_COMPRESSION -o cres cres_tool.c miniz.c

# 2. Write a manifest
cat > resources.txt <<EOF
config.json
z @www/
EOF

# 3. Generate the resource files
./cres -m resources.txt -o resources

# 4. Compile your project with the generated code
gcc -std=c11 -DCRES_COMPRESSION -o myapp main.c resources.c cres.c miniz.c
```

## Manifest format

One entry per line. Comments (`#`) and blank lines are ignored.

```
# Individual file (stored raw)
config.json

# Compressed file (default level 6)
z data/big-payload.json

# Compressed file with explicit level (1=fast, 9=best)
z9 data/huge-bundle.js
z1 data/quick-compress.txt

# Recursive directory (raw)
@assets/images/

# Recursive directory (compressed)
z @www/css/
z @www/js/

# Paths with spaces
"path/to/my file.txt"
z @"www/my site/"
```

**Path rules:**
- Paths are relative to the manifest file's directory
- For individual files, the lookup key is the path as written
- For `@dir/` entries, the lookup key is the full path from the manifest (e.g. `@www/` → `www/css/style.css`)
- C symbols are auto-derived from paths (non-alphanumeric → `_`). Collisions are detected at generation time

## Tool CLI

```
cres -m <manifest> -o <output_base> [--guard GUARD_NAME] [--prefix prefix_]
```

| Flag | Default | Description |
|------|---------|-------------|
| `-m, --manifest` | required | Manifest file path |
| `-o, --output` | required | Output base name (generates `<base>.h` and `<base>.c`) |
| `--guard` | auto from output name | Custom `#ifndef` include guard |
| `--prefix` | `cres_` | Symbol prefix for all generated names |

## Runtime API

### Types

```c
typedef struct {
    const char    *name;   // lookup key (path)
    const uint8_t *data;   // usable data (NULL for compressed entries until cres_load)
    size_t         size;   // decompressed size
    const uint8_t *cdata;  // compressed data (NULL if raw)
    size_t         csize;  // compressed size (0 if raw)
} CResEntry;
```

### Lookup

```c
// Find by name/path — O(n) linear scan
CResEntry *cres_find(CResEntry *table, size_t count, const char *name);
```

### Load / Unload

```c
// Decompress on demand (no-op for raw entries), returns 0 on success
int  cres_load(CResEntry *entry);
void cres_unload(CResEntry *entry);

// Bulk operations
int  cres_load_all(CResEntry *table, size_t count);
void cres_free_all(CResEntry *table, size_t count);

// Load/free by path prefix
int  cres_load_prefix(CResEntry *table, size_t count, const char *prefix);
void cres_free_prefix(CResEntry *table, size_t count, const char *prefix);
```

### FILE\* access

```c
// Opens resource as a FILE* stream. Caller must fclose().
// Decompresses automatically if needed.
FILE *cres_fopen(CResEntry *table, size_t count, const char *name);
```

### MIME type

```c
// Guess content type from file extension
const char *cres_mime(const CResEntry *entry);
```

### Convenience macros

```c
#define CRES_DATA(type, entry_ptr)  // cast data to const type*
#define CRES_SIZE(entry_ptr)        // get size
```

## Access patterns

### Direct symbol access (O(1), compile-time known)

The generated header exports a pointer per resource:

```c
#include "resources.h"

// Raw entry — data is always available
const char *html = CRES_DATA(char, cres_www_index_html);

// Compressed entry — load first
cres_load(cres_www_js_app_js);
const char *js = CRES_DATA(char, cres_www_js_app_js);
cres_unload(cres_www_js_app_js);
```

### Runtime lookup (O(n), dynamic path)

```c
CResEntry *e = cres_find(cres_table, cres_table_count, "www/css/style.css");
if (e) {
    cres_load(e);
    // use e->data, e->size
    cres_unload(e);
}
```

### Bulk prefix loading

```c
// Load all CSS and JS at once
cres_load_prefix(cres_table, cres_table_count, "www/css/");
cres_load_prefix(cres_table, cres_table_count, "www/js/");

// ... serve requests ...

// Free when done
cres_free_all(cres_table, cres_table_count);
```

## HTTP serving with Mongoose

```c
#include "mongoose.h"
#include "resources.h"

static void handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = ev_data;

        // Map URI to resource path: "/css/style.css" → "www/css/style.css"
        char path[512];
        snprintf(path, sizeof(path), "www%.*s", (int)hm->uri.len, hm->uri.buf);

        CResEntry *res = cres_find(cres_table, cres_table_count, path);
        if (res && cres_load(res) == 0) {
            mg_printf(c,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n\r\n",
                cres_mime(res), res->size);
            mg_send(c, res->data, res->size);
        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }
    }
}
```

## Vendoring into your project

### Files to copy

```
cres.h          — runtime API header
cres.c          — runtime implementation
cres_tool.c     — code generator source
miniz.h         — compression library header
miniz.c         — compression library
```

All five files are required. Drop them into a `vendor/cres/` directory (or wherever you keep vendored code).

### Integrating into your Makefile

```makefile
CRES_DIR = vendor/cres
CRES     = cres

# Build the tool from source (first time / when sources change)
$(CRES): $(CRES_DIR)/cres_tool.c $(CRES_DIR)/miniz.c
	$(CC) -std=c11 -w -DCRES_COMPRESSION -o $@ $^

# Generate resources from manifest
resources.h resources.c: $(CRES) resources.txt
	./$(CRES) -m resources.txt -o resources

# Compile miniz once with warnings suppressed
miniz.o: $(CRES_DIR)/miniz.c
	$(CC) -std=c11 -w -DCRES_COMPRESSION -c -o $@ $<

# Your application
myapp: main.c resources.c $(CRES_DIR)/cres.c miniz.o
	$(CC) -std=c11 -Wall -Wextra -DCRES_COMPRESSION -I$(CRES_DIR) -o $@ $^
```

The tool is built from source automatically as a build dependency. Works on Linux, macOS, and Windows (MinGW/Strawberry GCC).

## License

cres is public domain. miniz is MIT licensed.
