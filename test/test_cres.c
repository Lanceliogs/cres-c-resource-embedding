/*
 * test_cres.c — quick assert-based tests for cres runtime
 *
 * Build:
 *   1. Generate test resources:
 *      ./cres -m test/resources.txt -o test/resources
 *   2. Compile & run:
 *      gcc -std=c11 -DCRES_COMPRESSION -I. -Itest -o test/test_cres \
 *          test/test_cres.c test/resources.c cres.c miniz.c && ./test/test_cres
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "resources.h"

/* ---- Test harness ----------------------------------------------------- */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-50s ", #name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL  (%s)\n", msg); \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while (0)

#define ASSERT_EQ(a, b, msg)    ASSERT((a) == (b), msg)
#define ASSERT_NE(a, b, msg)    ASSERT((a) != (b), msg)
#define ASSERT_NULL(p, msg)     ASSERT((p) == NULL, msg)
#define ASSERT_NOT_NULL(p, msg) ASSERT((p) != NULL, msg)
#define ASSERT_STR_EQ(a, b, msg) ASSERT(strcmp((a), (b)) == 0, msg)

#define SECTION(name) printf("\n--- %s ---\n", name)
#define SUMMARY() \
    printf("\n=== %d tests: %d passed, %d failed ===\n", \
           tests_run, tests_passed, tests_failed)

/* ---- Tests ------------------------------------------------------------ */

static void test_table_count(void)
{
    TEST(table_count);
    ASSERT_EQ(cres_table_count, 3, "expected 3 resources");
    PASS();
}

static void test_find_existing(void)
{
    TEST(find_existing);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/hello.txt");
    ASSERT_NOT_NULL(e, "hello.txt not found");
    ASSERT_STR_EQ(e->name, "www/hello.txt", "wrong name");
    PASS();
}

static void test_find_nonexistent(void)
{
    TEST(find_nonexistent);
    CResEntry *e = cres_find(cres_table, cres_table_count, "nope.bin");
    ASSERT_NULL(e, "should return NULL for missing resource");
    PASS();
}

static void test_find_null_args(void)
{
    TEST(find_null_args);
    ASSERT_NULL(cres_find(NULL, 0, "x"), "NULL table");
    ASSERT_NULL(cres_find(cres_table, cres_table_count, NULL), "NULL name");
    PASS();
}

static void test_raw_entry_data_available(void)
{
    TEST(raw_entry_data_available);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/hello.txt");
    ASSERT_NOT_NULL(e, "not found");
    ASSERT_NOT_NULL(e->data, "raw entry data should be non-NULL");
    ASSERT_NULL(e->cdata, "raw entry cdata should be NULL");
    ASSERT_EQ(e->csize, 0, "raw entry csize should be 0");
    PASS();
}

static void test_raw_entry_content(void)
{
    TEST(raw_entry_content);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/hello.txt");
    ASSERT_NOT_NULL(e, "not found");
    ASSERT_EQ(e->size, 12, "expected 12 bytes");
    ASSERT(memcmp(e->data, "Hello, cres!", 12) == 0, "content mismatch");
    PASS();
}

static void test_compressed_entry_null_before_load(void)
{
    TEST(compressed_entry_null_before_load);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/css/style.css");
    ASSERT_NOT_NULL(e, "not found");
    ASSERT_NULL(e->data, "compressed entry data should be NULL before load");
    ASSERT_NOT_NULL(e->cdata, "compressed entry cdata should be set");
    ASSERT_NE(e->csize, 0, "compressed entry csize should be non-zero");
    PASS();
}

static void test_load_compressed(void)
{
    TEST(load_compressed);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/css/style.css");
    ASSERT_NOT_NULL(e, "not found");
    ASSERT_EQ(cres_load(e), 0, "cres_load failed");
    ASSERT_NOT_NULL(e->data, "data should be available after load");
    ASSERT(memcmp(e->data, "body { color: red; }", 20) == 0, "content mismatch");
    cres_unload(e);
    ASSERT_NULL(e->data, "data should be NULL after unload");
    PASS();
}

static void test_load_raw_is_noop(void)
{
    TEST(load_raw_is_noop);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/hello.txt");
    ASSERT_NOT_NULL(e, "not found");
    const uint8_t *orig = e->data;
    ASSERT_EQ(cres_load(e), 0, "load should succeed");
    ASSERT_EQ(e->data, orig, "data pointer should not change for raw entry");
    cres_unload(e);
    ASSERT_EQ(e->data, orig, "unload should be no-op for raw entry");
    PASS();
}

static void test_load_null(void)
{
    TEST(load_null);
    ASSERT_EQ(cres_load(NULL), -1, "load(NULL) should return -1");
    PASS();
}

static void test_double_load(void)
{
    TEST(double_load);
    CResEntry *e = cres_find(cres_table, cres_table_count, "www/js/app.js");
    ASSERT_NOT_NULL(e, "not found");
    ASSERT_EQ(cres_load(e), 0, "first load");
    const uint8_t *first = e->data;
    ASSERT_EQ(cres_load(e), 0, "second load");
    ASSERT_EQ(e->data, first, "should return same pointer on double load");
    cres_unload(e);
    PASS();
}

static void test_load_all(void)
{
    TEST(load_all);
    ASSERT_EQ(cres_load_all(cres_table, cres_table_count), 0, "load_all failed");
    for (size_t i = 0; i < cres_table_count; i++)
        ASSERT_NOT_NULL(cres_table[i].data, "entry data NULL after load_all");
    cres_free_all(cres_table, cres_table_count);
    PASS();
}

static void test_load_prefix(void)
{
    TEST(load_prefix);
    cres_load_prefix(cres_table, cres_table_count, "www/css/");

    CResEntry *css = cres_find(cres_table, cres_table_count, "www/css/style.css");
    ASSERT_NOT_NULL(css, "css not found");
    ASSERT_NOT_NULL(css->data, "css should be loaded");

    CResEntry *js = cres_find(cres_table, cres_table_count, "www/js/app.js");
    ASSERT_NOT_NULL(js, "js not found");
    ASSERT_NULL(js->data, "js should NOT be loaded");

    cres_free_prefix(cres_table, cres_table_count, "www/css/");
    ASSERT_NULL(css->data, "css should be freed");
    PASS();
}

static void test_free_prefix_no_match(void)
{
    TEST(free_prefix_no_match);
    cres_free_prefix(cres_table, cres_table_count, "nonexistent/");
    PASS();
}

static void test_direct_symbol_raw(void)
{
    TEST(direct_symbol_raw);
    ASSERT_NOT_NULL(cres_www_hello_txt, "direct symbol should exist");
    ASSERT_STR_EQ(cres_www_hello_txt->name, "www/hello.txt", "wrong name");
    ASSERT_NOT_NULL(cres_www_hello_txt->data, "raw should have data");
    ASSERT_EQ(cres_www_hello_txt->size, 12, "wrong size");
    PASS();
}

static void test_direct_symbol_compressed(void)
{
    TEST(direct_symbol_compressed);
    ASSERT_NOT_NULL(cres_www_css_style_css, "direct symbol should exist");
    ASSERT_STR_EQ(cres_www_css_style_css->name, "www/css/style.css", "wrong name");
    ASSERT_NULL(cres_www_css_style_css->data, "compressed should have NULL data");
    ASSERT_EQ(cres_load(cres_www_css_style_css), 0, "load failed");
    ASSERT_NOT_NULL(cres_www_css_style_css->data, "should have data after load");
    cres_unload(cres_www_css_style_css);
    PASS();
}

static void test_direct_symbol_points_into_table(void)
{
    TEST(direct_symbol_points_into_table);
    int found = 0;
    for (size_t i = 0; i < cres_table_count; i++) {
        if (cres_www_hello_txt == &cres_table[i]) { found = 1; break; }
    }
    ASSERT(found, "direct symbol should be a pointer into cres_table");
    PASS();
}

static void test_macros(void)
{
    TEST(CRES_DATA_and_CRES_SIZE);
    ASSERT_EQ(CRES_SIZE(cres_www_hello_txt), 12, "wrong size via macro");
    const char *s = CRES_DATA(char, cres_www_hello_txt);
    ASSERT(memcmp(s, "Hello, cres!", 12) == 0, "wrong data via macro");
    PASS();
}

static void test_mime_html(void)
{
    TEST(mime_html);
    CResEntry e = { .name = "index.html" };
    ASSERT_STR_EQ(cres_mime(&e), "text/html", "wrong mime");
    PASS();
}

static void test_mime_css(void)
{
    TEST(mime_css);
    CResEntry e = { .name = "css/style.css" };
    ASSERT_STR_EQ(cres_mime(&e), "text/css", "wrong mime");
    PASS();
}

static void test_mime_js(void)
{
    TEST(mime_js);
    CResEntry e = { .name = "app.js" };
    ASSERT_STR_EQ(cres_mime(&e), "application/javascript", "wrong mime");
    PASS();
}

static void test_mime_png(void)
{
    TEST(mime_png);
    CResEntry e = { .name = "images/logo.png" };
    ASSERT_STR_EQ(cres_mime(&e), "image/png", "wrong mime");
    PASS();
}

static void test_mime_unknown(void)
{
    TEST(mime_unknown);
    CResEntry e = { .name = "file.xyz123" };
    ASSERT_STR_EQ(cres_mime(&e), "application/octet-stream", "wrong fallback");
    PASS();
}

static void test_mime_no_extension(void)
{
    TEST(mime_no_extension);
    CResEntry e = { .name = "Makefile" };
    ASSERT_STR_EQ(cres_mime(&e), "application/octet-stream", "wrong fallback");
    PASS();
}

static void test_mime_null(void)
{
    TEST(mime_null);
    ASSERT_STR_EQ(cres_mime(NULL), "application/octet-stream", "NULL entry");
    CResEntry e = { .name = NULL };
    ASSERT_STR_EQ(cres_mime(&e), "application/octet-stream", "NULL name");
    PASS();
}

static void test_fopen_raw(void)
{
    TEST(fopen_raw);
    FILE *f = cres_fopen(cres_table, cres_table_count, "www/hello.txt");
    ASSERT_NOT_NULL(f, "fopen returned NULL");
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    ASSERT_EQ(n, 12, "wrong read size");
    ASSERT(memcmp(buf, "Hello, cres!", 12) == 0, "content mismatch");
    PASS();
}

static void test_fopen_compressed(void)
{
    TEST(fopen_compressed);
    FILE *f = cres_fopen(cres_table, cres_table_count, "www/css/style.css");
    ASSERT_NOT_NULL(f, "fopen returned NULL");
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    ASSERT_EQ(n, 20, "wrong read size");
    ASSERT(memcmp(buf, "body { color: red; }", 20) == 0, "content mismatch");
    cres_unload(cres_find(cres_table, cres_table_count, "www/css/style.css"));
    PASS();
}

static void test_fopen_nonexistent(void)
{
    TEST(fopen_nonexistent);
    FILE *f = cres_fopen(cres_table, cres_table_count, "nope.bin");
    ASSERT_NULL(f, "should return NULL for missing");
    PASS();
}

/* ---- Main ------------------------------------------------------------- */

int main(void)
{
    printf("cres test suite\n");

    SECTION("table");
    test_table_count();

    SECTION("cres_find");
    test_find_existing();
    test_find_nonexistent();
    test_find_null_args();

    SECTION("raw entries");
    test_raw_entry_data_available();
    test_raw_entry_content();

    SECTION("compressed entries");
    test_compressed_entry_null_before_load();
    test_load_compressed();
    test_load_raw_is_noop();
    test_load_null();
    test_double_load();

    SECTION("bulk load");
    test_load_all();
    test_load_prefix();
    test_free_prefix_no_match();

    SECTION("direct symbols");
    test_direct_symbol_raw();
    test_direct_symbol_compressed();
    test_direct_symbol_points_into_table();
    test_macros();

    SECTION("cres_mime");
    test_mime_html();
    test_mime_css();
    test_mime_js();
    test_mime_png();
    test_mime_unknown();
    test_mime_no_extension();
    test_mime_null();

    SECTION("cres_fopen");
    test_fopen_raw();
    test_fopen_compressed();
    test_fopen_nonexistent();

    SUMMARY();
    return tests_failed ? 1 : 0;
}
