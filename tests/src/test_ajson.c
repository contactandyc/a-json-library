// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

// tests/test_main.c
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

/* Optional: richer error locations from your parser */
#define AJSON_DEBUG 1
#include "a-json-library/ajson.h"
#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

#include "the-macro-library/macro_test.h"

static ajson_t* P(aml_pool_t *pool, const char *s) { return ajson_parse_string(pool, s); }
static int is_ok(ajson_t *j) { return j && !ajson_is_error(j); }

/* ---------- 0) Predicates ---------- */

MACRO_TEST(type_predicates_nullptr) {
    /* All predicates should be safe on NULL and return false. */
    MACRO_ASSERT_FALSE(ajson_is_error(NULL));
    MACRO_ASSERT_FALSE(ajson_is_object(NULL));
    MACRO_ASSERT_FALSE(ajson_is_array(NULL));
    MACRO_ASSERT_FALSE(ajson_is_null(NULL));
    MACRO_ASSERT_FALSE(ajson_is_bool(NULL));
    MACRO_ASSERT_FALSE(ajson_is_string(NULL));
    MACRO_ASSERT_FALSE(ajson_is_number(NULL));
}

MACRO_TEST(type_predicates_values) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    MACRO_ASSERT_TRUE(ajson_is_null(ajson_null(pool)));
    MACRO_ASSERT_TRUE(ajson_is_bool(ajson_true(pool)));
    MACRO_ASSERT_TRUE(ajson_is_bool(ajson_false(pool)));
    MACRO_ASSERT_TRUE(ajson_is_string(ajson_str(pool, "x")));
    MACRO_ASSERT_TRUE(ajson_is_number(ajson_zero(pool)));
    MACRO_ASSERT_TRUE(ajson_is_number(ajson_number(pool, 123)));
    MACRO_ASSERT_TRUE(ajson_is_number(ajson_decimal_string(pool, "1.5")));

    ajson_t *o = ajsono(pool);
    ajson_t *a = ajsona(pool);
    MACRO_ASSERT_TRUE(ajson_is_object(o));
    MACRO_ASSERT_TRUE(ajson_is_array(a));
    MACRO_ASSERT_FALSE(ajson_is_number(o));
    MACRO_ASSERT_FALSE(ajson_is_string(a));

    aml_pool_destroy(pool);
}

/* ---------- 1) Basic parse/dump ---------- */

MACRO_TEST(parse_object_basic) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"a\":1,\"b\":true,\"c\":null}");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_TRUE(ajson_is_object(j));
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j,"a")), "1");
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j,"b")), "true");
    /* For null, ajsonv() intentionally returns NULL (not "null"). */
    MACRO_ASSERT_TRUE(ajson_is_null(ajsono_scan(j,"c")));

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"a\":1,\"b\":true,\"c\":null}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(parse_array_basic) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "[1,2,3]");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_TRUE(ajson_is_array(j));
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(j,0)), "1");
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(j,1)), "2");
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(j,2)), "3");
    aml_pool_destroy(pool);
}

/* ---------- 2) Numbers ---------- */

MACRO_TEST(numbers_valid) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *ok[] = {
        "0","-0","0.0","-0.0","1","-1","10","1234567890",
        "0e0","0E+5","0e-10","1e10","-1e-2","3.14159e+00","10E-2"
    };
    for (size_t i=0;i<sizeof(ok)/sizeof(ok[0]);++i) {
        char json[64];
        snprintf(json, sizeof(json), "{\"n\":%s}", ok[i]);
        ajson_t *j = P(pool, json);
        MACRO_ASSERT_TRUE(is_ok(j));
        ajson_t *n = ajsono_scan(j,"n");
        MACRO_ASSERT_TRUE(ajson_is_number(n));
        MACRO_ASSERT_STREQ(ajsonv(n), ok[i]);
    }
    aml_pool_destroy(pool);
}

MACRO_TEST(numbers_invalid) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *bad[] = {
        "{ \"n\": 01 }",
        "{ \"n\": -01 }",
        "{ \"n\": 1. }",
        "{ \"n\": .5 }",
        "{ \"n\": 1e }",
        "{ \"n\": -0e }"
    };
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        ajson_t *j = P(pool, bad[i]);
        MACRO_ASSERT_TRUE(j && ajson_is_error(j));
    }
    aml_pool_destroy(pool);
}

/* ---------- 3) UTF-8 filtering on dumps (values only) ---------- */

MACRO_TEST(utf8_filter_in_value_dumps) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* 0xC3 0x28 is invalid; output should drop 0xC3 and keep '(' */
    const char bad[] = { '{','"','s','"',':','"','\xC3','\x28','A','B','C','"', '}', 0};
    ajson_t *j = ajson_parse_string(pool, bad);
    MACRO_ASSERT_TRUE(is_ok(j));
    ajson_t *s = ajsono_scan(j, "s");
    MACRO_ASSERT_TRUE(s);

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"s\":\"(ABC\"}");

    char *pretty = ajson_stringify_pretty(pool, j, 2);
    MACRO_ASSERT_TRUE(strstr(pretty, "(ABC") != NULL);
    free(buf);
    aml_pool_destroy(pool);
}

/* ---------- 4) FILE* dump smoke (portable) ---------- */

MACRO_TEST(file_dump_smoke) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"k\":\"v\",\"n\":123}");
    MACRO_ASSERT_TRUE(is_ok(j));
    FILE *f = tmpfile();
    MACRO_ASSERT_TRUE(f != NULL);
    ajson_dump(f, j);
    fclose(f);
    aml_pool_destroy(pool);
}

/* ---------- 5) Objects: get vs find, set/remove ---------- */

MACRO_TEST(object_indexes_and_mutation) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *obj = ajsono(pool);
    ajsono_append(obj, "a", ajson_number(pool, 1), true);
    ajsono_append(obj, "b", ajson_true(pool), true);

    /* Build snapshot (sorted-array) used by _get */
    MACRO_ASSERT_STREQ(ajsonv(ajsono_get(obj, "a")), "1");
    MACRO_ASSERT_STREQ(ajsonv(ajsono_get(obj, "b")), "true");

    /* Append after index is built */
    ajsono_append(obj, "z", ajson_str(pool, "late"), true);

    MACRO_ASSERT_TRUE(ajsono_get(obj, "z") == NULL);          /* snapshot */
    MACRO_ASSERT_STREQ(ajsonv(ajsono_find(obj, "z")), "late");/* live */

    ajsono_set(obj, "a", ajson_str(pool, "one"), true);
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(obj, "a")), "one");

    MACRO_ASSERT_TRUE(ajsono_remove(obj, "b"));
    MACRO_ASSERT_TRUE(ajsono_scan(obj, "b") == NULL);

    aml_pool_destroy(pool);
}

/* ---------- 6) Arrays: append, nth, clear ---------- */

MACRO_TEST(array_append_nth_clear) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *arr = ajsona(pool);
    ajsona_append(arr, ajson_number(pool, 10));
    ajsona_append(arr, ajson_number(pool, 20));
    ajsona_append(arr, ajson_number(pool, 30));
    MACRO_ASSERT_EQ_INT(ajsona_count(arr), 3);
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(arr, 1)), "20");
    ajsona_clear(arr);
    MACRO_ASSERT_EQ_INT(ajsona_count(arr), 0);
    MACRO_ASSERT_TRUE(ajsona_nth(arr, 0) == NULL);
    aml_pool_destroy(pool);
}

/* ---------- 7) Path helpers ---------- */

MACRO_TEST(path_helpers) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *doc = P(pool, "{\"users\":[{\"id\":\"1\",\"name\":\"x\"},{\"id\":\"2\",\"name\":\"y\"}]}");
    MACRO_ASSERT_TRUE(is_ok(doc));
    MACRO_ASSERT_STREQ(ajsono_pathv(pool, doc, "users.1.name"), "y");

    ajson_t *u2 = ajsono_path(pool, doc, "users.id=2");
    MACRO_ASSERT_TRUE(u2 && ajson_is_object(u2));
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(u2, "name")), "y");
    aml_pool_destroy(pool);
}

/* ---------- 8) Encode/Decode helpers ---------- */

MACRO_TEST(encode_decode) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *raw = "Hello\t\"World\"\n";
    ajson_t *s = ajson_encode_str(pool, raw);
    MACRO_ASSERT_TRUE(s);
    size_t out_len = 0;
    char *decoded = ajson_decode2(&out_len, pool, ajsonv(s), s->length);
    MACRO_ASSERT_EQ_SZ(out_len, strlen(raw));
    MACRO_ASSERT_TRUE(memcmp(decoded, raw, out_len) == 0);
    aml_pool_destroy(pool);
}

/* ---------- 9) Error reporting (row/col smoke) ---------- */

MACRO_TEST(error_row_col) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *err = P(pool, "{\n  \"x\": 1,\n  \"y\": [1,2,,3]\n}\n");
    MACRO_ASSERT_TRUE(err && ajson_is_error(err));
    aml_buffer_t *bh = aml_buffer_init(256);
    ajson_dump_error_to_buffer(bh, err);
    MACRO_ASSERT_TRUE(strstr(aml_buffer_data(bh), "row 3, column: 14"));
    aml_buffer_destroy(bh);
    // ajson_dump_error(stdout, err); /* smoke */
    aml_pool_destroy(pool);
}

/* ---------- New tests ---------- */

MACRO_TEST(dump_estimate_matches_output) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"a\":1,\"b\":[true,null,\"hi\"]}");
    MACRO_ASSERT_TRUE(is_ok(j));

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    char *end = ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_EQ_SZ((size_t)(end - buf) + 1, need);
    MACRO_ASSERT_STREQ(buf, "{\"a\":1,\"b\":[true,null,\"hi\"]}");

    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(pretty_estimate_matches_output) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"a\":1,\"b\":[true,null,\"hi\"]}");
    MACRO_ASSERT_TRUE(is_ok(j));

    size_t need = ajson_dump_pretty_estimate(j, 2);
    char *pretty = ajson_stringify_pretty(pool, j, 2);
    MACRO_ASSERT_EQ_SZ(strlen(pretty) + 1, need);

    /* Spot-check structure */
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"a\": 1") != NULL);
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"b\": [") != NULL);

    aml_pool_destroy(pool);
}

MACRO_TEST(parse_string_is_nondestructive) {
    const char *src = "{\"k\":\"x\\\"y\",\"n\":123}";
    char *copy = strdup(src);
    aml_pool_t *pool = aml_pool_init(1 << 12);

    ajson_t *j = ajson_parse_string(pool, src);
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_STREQ(copy, src); /* original must be unchanged */

    free(copy);
    aml_pool_destroy(pool);
}

MACRO_TEST(keys_escaped_quote_and_path_escape) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* key1 = a\"b, key2 = c.d (dot in key) */
    ajson_t *j = P(pool, "{ \"a\\\"b\": 1, \"c.d\": 2, \"obj\": {\"x.y\": 3} }");
    MACRO_ASSERT_TRUE(is_ok(j));

    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j, "a\\\"b")), "1");
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j, "c.d")), "2");

    /* path dot-escape: use backslash to treat '.' as literal */
    MACRO_ASSERT_STREQ(ajsono_pathv(pool, j, "obj.x\\.y"), "3");
    aml_pool_destroy(pool);
}

MACRO_TEST(duplicate_keys_scan_vs_scanr) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{ \"x\": 1, \"x\": 2, \"x\": 3 }");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j,  "x")), "1");
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scanr(j, "x")), "3");
    aml_pool_destroy(pool);
}

MACRO_TEST(remove_invalidate_get_index) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *obj = ajsono(pool);
    ajsono_append(obj, "k", ajson_number(pool, 7), true);

    /* Build snapshot used by _get */
    MACRO_ASSERT_STREQ(ajsonv(ajsono_get(obj, "k")), "7");

    /* Remove and ensure _get reflects it (sorted-array invalidated/rebuilt) */
    MACRO_ASSERT_TRUE(ajsono_remove(obj, "k"));
    MACRO_ASSERT_TRUE(ajsono_get(obj, "k") == NULL);
    aml_pool_destroy(pool);
}

MACRO_TEST(array_erase_middle) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *arr = ajsona(pool);
    ajsona_append(arr, ajson_str(pool, "a"));
    ajsona_append(arr, ajson_str(pool, "b"));
    ajsona_append(arr, ajson_str(pool, "c"));
    MACRO_ASSERT_EQ_INT(ajsona_count(arr), 3);

    ajsona_t *mid = ajsona_nth_node(arr, 1);
    MACRO_ASSERT_TRUE(mid != NULL);
    ajsona_erase(mid);

    MACRO_ASSERT_EQ_INT(ajsona_count(arr), 2);
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(arr, 0)), "a");
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(arr, 1)), "c");
    aml_pool_destroy(pool);
}

MACRO_TEST(conversion_helpers_defaults_and_values) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *n = ajson_number(pool, 42);
    ajson_t *d = ajson_decimal_string(pool, "3.5");
    ajson_t *t = ajson_true(pool);
    ajson_t *f = ajson_false(pool);

    MACRO_ASSERT_EQ_INT(ajson_to_int(n, 0), 42);
    MACRO_ASSERT_EQ_INT(ajson_to_int(NULL, 7), 7);
    MACRO_ASSERT_TRUE(ajson_to_bool(t, false));
    MACRO_ASSERT_FALSE(ajson_to_bool(f, true));
    MACRO_ASSERT_TRUE(ajson_to_double(d, 0.0) > 3.49 && ajson_to_double(d, 0.0) < 3.51);

    /* via object scan helpers */
    ajson_t *obj = ajsono(pool);
    ajsono_append(obj, "n", n, false);
    ajsono_append(obj, "d", d, false);
    ajsono_append(obj, "t", t, false);

    MACRO_ASSERT_EQ_INT(ajsono_scan_int(obj, "n", -1), 42);
    MACRO_ASSERT_TRUE(ajsono_scan_double(obj, "d", 0.0) > 3.49);
    MACRO_ASSERT_TRUE(ajsono_scan_bool(obj, "t", false));

    aml_pool_destroy(pool);
}

MACRO_TEST(decode_unicode_surrogate_pair_and_invalid) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Valid pair: U+1D11E (MUSICAL SYMBOL G CLEF) -> F0 9D 84 9E */
    const char *enc_pair = "\\uD834\\uDD1E";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, (char*)enc_pair, strlen(enc_pair));
    MACRO_ASSERT_EQ_SZ(out_len, 4);
    unsigned char expect[4] = {0xF0,0x9D,0x84,0x9E};
    MACRO_ASSERT_TRUE(memcmp(dec, expect, 4) == 0);

    /* Invalid lone high surrogate -> copied literally */
    const char *enc_bad = "\\uD800";
    out_len = 0;
    dec = ajson_decode2(&out_len, pool, (char*)enc_bad, strlen(enc_bad));
    MACRO_ASSERT_EQ_SZ(out_len, 6);
    MACRO_ASSERT_TRUE(memcmp(dec, "\\uD800", 6) == 0);

    aml_pool_destroy(pool);
}

MACRO_TEST(pretty_indentation_contains_expected_spaces) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"k\":\"v\",\"n\":123}");
    MACRO_ASSERT_TRUE(is_ok(j));
    char *pretty = ajson_stringify_pretty(pool, j, 2);
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"k\": \"v\"") != NULL);
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"n\": 123") != NULL);
    aml_pool_destroy(pool);
}

/* ---------- More tests ---------- */

MACRO_TEST(empty_values_and_whitespace) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    ajson_t *o = P(pool, " {  } ");
    MACRO_ASSERT_TRUE(is_ok(o));
    MACRO_ASSERT_TRUE(ajson_is_object(o));
    size_t need = ajson_dump_estimate(o);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, o);
    MACRO_ASSERT_STREQ(buf, "{}");
    free(buf);

    ajson_t *a = P(pool, "\n\t [ \r\n ] \t");
    MACRO_ASSERT_TRUE(is_ok(a));
    MACRO_ASSERT_TRUE(ajson_is_array(a));
    need = ajson_dump_estimate(a);
    buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, a);
    MACRO_ASSERT_STREQ(buf, "[]");
    free(buf);

    aml_pool_destroy(pool);
}

MACRO_TEST(encode_embedded_nul_and_controls) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* raw: A \0 B \n */
    const char raw[] = { 'A', '\0', 'B', '\n' };
    char *enc = ajson_encode(pool, (char*)raw, sizeof(raw));
    /* Expect: A\u0000B\n  (newline becomes \n) */
    MACRO_ASSERT_STREQ(enc, "A\\u0000B\\n");

    /* No-escape case should return the same pointer */
    char ok[] = "simple";
    char *enc2 = ajson_encode(pool, ok, strlen(ok));
    MACRO_ASSERT_TRUE(enc2 == ok);

    aml_pool_destroy(pool);
}

MACRO_TEST(invalid_utf8_truncated_sequence_end) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* "XY" + truncated 3-byte sequence start (E2 82 missing final AC) */
    const char bad[] = { '{','"','s','"',':','"','X','Y','\xE2','\x82','"', '}', 0 };
    ajson_t *j = ajson_parse_string(pool, bad);
    MACRO_ASSERT_TRUE(is_ok(j));

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"s\":\"XY\"}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(pretty_to_buffer_equals_pretty_string) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"obj\":{\"k\":\"v\"},\"arr\":[1,2,3]}");
    MACRO_ASSERT_TRUE(is_ok(j));

    char *pretty = ajson_stringify_pretty(pool, j, 2);

    aml_buffer_t *bh = aml_buffer_init(64);
    ajson_dump_pretty_to_buffer(bh, j, 2);

    MACRO_ASSERT_EQ_SZ(strlen(pretty), aml_buffer_length(bh));
    MACRO_ASSERT_TRUE(memcmp(pretty, aml_buffer_data(bh), aml_buffer_length(bh)) == 0);

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

MACRO_TEST(object_remove_head_tail_middle) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "a", ajson_number(pool, 1), true);  /* head */
    ajsono_append(o, "b", ajson_number(pool, 2), true);  /* middle */
    ajsono_append(o, "c", ajson_number(pool, 3), true);  /* tail */
    MACRO_ASSERT_EQ_INT(ajsono_count(o), 3);

    /* remove head */
    MACRO_ASSERT_TRUE(ajsono_remove(o, "a"));
    MACRO_ASSERT_EQ_INT(ajsono_count(o), 2);
    MACRO_ASSERT_TRUE(ajsono_scan(o, "a") == NULL);
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(o, "b")), "2");

    /* remove tail */
    MACRO_ASSERT_TRUE(ajsono_remove(o, "c"));
    MACRO_ASSERT_EQ_INT(ajsono_count(o), 1);
    MACRO_ASSERT_TRUE(ajsono_scan(o, "c") == NULL);

    /* remove last (middle-now-head) */
    MACRO_ASSERT_TRUE(ajsono_remove(o, "b"));
    MACRO_ASSERT_EQ_INT(ajsono_count(o), 0);
    MACRO_ASSERT_TRUE(ajsono_scan(o, "b") == NULL);

    aml_pool_destroy(pool);
}

MACRO_TEST(path_filter_then_field) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *doc = P(pool, "{\"users\":[{\"id\":\"1\",\"name\":\"x\"},{\"id\":\"2\",\"name\":\"y\"}]}");
    MACRO_ASSERT_TRUE(is_ok(doc));

    /* Filter -> then field in the same path */
    MACRO_ASSERT_STREQ(ajsono_pathv(pool, doc, "users.id=2.name"), "y");
    aml_pool_destroy(pool);
}

MACRO_TEST(extract_string_array_variants) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Non-array -> singleton array */
    size_t n = 0;
    char **out = ajson_extract_string_array(&n, pool, ajson_str(pool, "solo"));
    MACRO_ASSERT_EQ_SZ(n, 1);
    MACRO_ASSERT_STREQ(out[0], "solo");
    MACRO_ASSERT_TRUE(out[1] == NULL);

    /* Real array of strings */
    ajson_t *arr = ajsona(pool);
    ajsona_append(arr, ajson_str(pool, "a"));
    ajsona_append(arr, ajson_str(pool, "b"));
    ajsona_append(arr, ajson_str(pool, "c"));
    n = 0;
    out = ajson_extract_string_array(&n, pool, arr);
    MACRO_ASSERT_EQ_SZ(n, 3);
    MACRO_ASSERT_STREQ(out[0], "a");
    MACRO_ASSERT_STREQ(out[1], "b");
    MACRO_ASSERT_STREQ(out[2], "c");
    MACRO_ASSERT_TRUE(out[3] == NULL);

    aml_pool_destroy(pool);
}

MACRO_TEST(conversion_defaults_on_string) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *s = ajson_str(pool, "abc"); /* not a number */
    MACRO_ASSERT_EQ_INT(ajson_to_int(s, 42), 42);
    MACRO_ASSERT_TRUE(ajson_to_double(s, 3.14) == 3.14);
    MACRO_ASSERT_FALSE(ajson_to_bool(s, false)); /* macro_to_bool("abc", false) should keep default */

    ajson_t *obj = ajsono(pool);
    ajsono_append(obj, "x", s, false);
    MACRO_ASSERT_EQ_INT(ajsono_scan_int(obj, "x", -9), -9);

    aml_pool_destroy(pool);
}

MACRO_TEST(keys_preserve_escapes_on_dump) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Key contains a quote (encoded as \") and a backslash (encoded as \\) */
    ajson_t *j = P(pool, "{ \"a\\\"b\\\\c\": 1 }");
    MACRO_ASSERT_TRUE(is_ok(j));

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"a\\\"b\\\\c\":1}");

    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(trailing_comma_invalid) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{ \"a\":1, }")));
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "[1,2,]")));
    aml_pool_destroy(pool);
}

MACRO_TEST(pretty_step_zero_defaults_to_two) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"x\":1,\"y\":[2]}");
    MACRO_ASSERT_TRUE(is_ok(j));
    char *pretty = ajson_stringify_pretty(pool, j, 0);   /* step=0 => 2 spaces */
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"x\": 1") != NULL);
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"y\": [") != NULL);
    aml_pool_destroy(pool);
}

MACRO_TEST(pretty_step_negative_defaults_to_two) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"x\":1}");
    MACRO_ASSERT_TRUE(is_ok(j));
    char *pretty = ajson_stringify_pretty(pool, j, -4);  /* negative => 2 spaces */
    MACRO_ASSERT_TRUE(strstr(pretty, "\n  \"x\": 1") != NULL);
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: decode/encode escapes ---------- */

MACRO_TEST(decode_simple_no_escapes_zerocopy) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *enc = "no_escapes_here";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, (char*)enc, strlen(enc));
    MACRO_ASSERT_TRUE(dec == enc);                /* zero-copy */
    MACRO_ASSERT_EQ_SZ(out_len, strlen(enc));
    aml_pool_destroy(pool);
}

MACRO_TEST(decode_all_simple_escapes) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *enc = "\\n\\t\\r\\b\\f\\/\\\\\\\"";
    const char expected[8] = { '\n','\t','\r','\b','\f','/','\\','"' };
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, (char*)enc, strlen(enc));
    MACRO_ASSERT_EQ_SZ(out_len, sizeof(expected));
    MACRO_ASSERT_TRUE(memcmp(dec, expected, sizeof(expected)) == 0);
    aml_pool_destroy(pool);
}

MACRO_TEST(encode_slash_quote_backslash) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char raw[] = { '/', '\\', '\"' };
    char *enc = ajson_encode(pool, (char*)raw, sizeof(raw));
    MACRO_ASSERT_STREQ(enc, "\\/\\\\\\\"");
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: 64-bit helpers & vararg builders ---------- */

MACRO_TEST(uint64_roundtrip_max) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *u = ajson_uint64(pool, UINT64_MAX);
    MACRO_ASSERT_TRUE(ajson_is_number(u));
    MACRO_ASSERT_STREQ(ajsonv(u), "18446744073709551615");
    MACRO_ASSERT_TRUE(ajson_to_uint64(u, 0) == UINT64_MAX);
    aml_pool_destroy(pool);
}

MACRO_TEST(number_stringf_variants) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *n = ajson_number_stringf(pool, "%d%s", 12, "34");
    ajson_t *d = ajson_decimal_stringf(pool, "%.3f", 1.25);
    MACRO_ASSERT_TRUE(ajson_is_number(n));
    MACRO_ASSERT_TRUE(ajson_is_number(d));
    MACRO_ASSERT_STREQ(ajsonv(n), "1234");
    MACRO_ASSERT_STREQ(ajsonv(d), "1.250");
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: path & error edges ---------- */

MACRO_TEST(path_index_out_of_range) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *doc = P(pool, "{\"users\":[{\"id\":\"1\"},{\"id\":\"2\"}]}");
    MACRO_ASSERT_TRUE(is_ok(doc));
    MACRO_ASSERT_TRUE(ajsono_path(pool, doc, "users.999") == NULL);
    MACRO_ASSERT_TRUE(ajsono_path(pool, doc, "users.x") == NULL); /* non-numeric index */
    aml_pool_destroy(pool);
}

MACRO_TEST(syntax_errors_basic) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{\"a\" 1}")));         /* missing ':' */
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{\"a\":\"abc}")));     /* unclosed string */
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "[1,2")));              /* missing ']' */
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "trux")));              /* not 'true' */
    /* whitespace in number body */
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{ \"n\": - 1 }")));
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: insertion order & snapshot rebuild ---------- */

MACRO_TEST(object_insertion_order_preserved) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "z", ajson_number(pool, 1), true);
    ajsono_append(o, "a", ajson_number(pool, 2), true);
    ajsono_append(o, "m", ajson_number(pool, 3), true);

    size_t need = ajson_dump_estimate(o);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, o);
    MACRO_ASSERT_STREQ(buf, "{\"z\":1,\"a\":2,\"m\":3}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(get_after_set_rebuilds_snapshot) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "a", ajson_number(pool, 1), true);
    ajsono_append(o, "b", ajson_number(pool, 2), true);

    /* build snapshot for _get */
    MACRO_ASSERT_STREQ(ajsonv(ajsono_get(o, "a")), "1");

    /* set a new key; _get should see it after internal rebuild */
    ajsono_set(o, "c", ajson_number(pool, 3), true);
    MACRO_ASSERT_STREQ(ajsonv(ajsono_get(o, "c")), "3");
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: defaults and NULL behaviors ---------- */

MACRO_TEST(to_strd_defaults_on_null_and_error) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    MACRO_ASSERT_STREQ(ajson_to_strd(pool, ajson_null(pool), "DEF"), "DEF");
    ajson_t *err = P(pool, "{,}");  /* force an error node */
    MACRO_ASSERT_TRUE(ajson_is_error(err));
    MACRO_ASSERT_STREQ(ajson_to_strd(pool, err, "DEF2"), "DEF2");
    aml_pool_destroy(pool);
}

MACRO_TEST(ajsona_count_null_safe) {
    MACRO_ASSERT_EQ_INT(ajsona_count(NULL), 0);
}

/* ---------- Extra coverage: roundtrip stability ---------- */

MACRO_TEST(roundtrip_numbers_stability) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j1 = P(pool, "{\"n\":-0.0e+00,\"m\":123,\"d\":3.25e-5}");
    MACRO_ASSERT_TRUE(is_ok(j1));

    size_t need1 = ajson_dump_estimate(j1);
    char *s1 = (char*)malloc(need1);
    ajson_dump_to_memory(s1, j1);

    ajson_t *j2 = P(pool, s1);
    MACRO_ASSERT_TRUE(is_ok(j2));

    size_t need2 = ajson_dump_estimate(j2);
    char *s2 = (char*)malloc(need2);
    ajson_dump_to_memory(s2, j2);

    MACRO_ASSERT_STREQ(s1, s2);
    free(s1); free(s2);
    aml_pool_destroy(pool);
}

/* ---------- Extra coverage: insert API ---------- */

MACRO_TEST(object_insert_updates_existing_and_adds_new) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "x", ajson_str(pool, "one"), true);

    /* replace existing via insert */
    ajsono_insert(o, "x", ajson_str(pool, "two"), true);
    MACRO_ASSERT_STREQ(ajsonv(ajsono_find(o, "x")), "two");

    /* add new via insert */
    ajsono_insert(o, "y", ajson_number(pool, 3), true);
    MACRO_ASSERT_STREQ(ajsonv(ajsono_find(o, "y")), "3");

    aml_pool_destroy(pool);
}

MACRO_TEST(nocopy_encode_alias_when_clean) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    char *s = strdup("cleanASCII");
    ajson_t *j = ajson_encode_str_nocopy(pool, s);
    MACRO_ASSERT_TRUE(j && ajson_is_string(j));
    MACRO_ASSERT_TRUE(ajsonv(j) == s);                 /* alias confirmed */
    MACRO_ASSERT_EQ_SZ(j->length, strlen(s));

    s[5] = '_';                                  /* replace, not insert */

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);

    /* Expect replacement result */
    MACRO_ASSERT_TRUE(strstr(buf, "clean_SCII") != NULL);

    free(s);
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(nocopy_encode_length_is_captured) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    char s[32] = "abc";                          /* NUL-terminated */
    ajson_t *j = ajson_encode_str_nocopy(pool, s);
    MACRO_ASSERT_TRUE(ajsonv(j) == s);
    MACRO_ASSERT_EQ_SZ(j->length, 3);

    /* Try to grow the source after node creation */
    s[3] = '!'; s[4] = 0;                        /* now "abc!" in the buffer */

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);

    MACRO_ASSERT_TRUE(strstr(buf, "\"abc\"") != NULL); /* still original length */
    MACRO_ASSERT_TRUE(strstr(buf, "\"abc!\"") == NULL);

    free(buf);
    aml_pool_destroy(pool);
}


MACRO_TEST(nocopy_encode_string_len_zerocopy_safe) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Non-NUL-terminated slice, no escapes needed */
    char raw[3] = { 'A','B','C' }; /* no trailing NUL */
    ajson_t *j = ajson_encode_string_nocopy(pool, raw, 3);
    MACRO_ASSERT_TRUE(j && ajson_is_string(j));
    MACRO_ASSERT_TRUE(ajsonv(j) == raw);         /* aliased */
    MACRO_ASSERT_EQ_SZ(j->length, 3);            /* uses original length, not strlen */

    /* Dump length should be 2 quotes + 3 bytes */
    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    char *end = ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_EQ_SZ((size_t)(end - buf), 5);  /* "ABC" -> 5 bytes */
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(nocopy_encode_alloc_when_escapes) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    char raw[] = { 'A','\n','B', 0 }; /* needs escaping */
    ajson_t *j = ajson_encode_string_nocopy(pool, raw, 3);
    MACRO_ASSERT_TRUE(j && ajson_is_string(j));
    MACRO_ASSERT_TRUE(ajsonv(j) != raw);         /* encoded into new buffer */
    MACRO_ASSERT_STREQ(ajsonv(j), "A\\nB");      /* j->length matches strlen */

    aml_pool_destroy(pool);
}

MACRO_TEST(copy_vs_nocopy_string_builders) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    char *s = strdup("hello");
    ajson_t *j_copy   = ajson_str(pool, s);          /* duplicates */
    ajson_t *j_nocopy = ajson_str_nocopy(pool, s);   /* aliases */

    MACRO_ASSERT_TRUE(j_copy && j_nocopy);
    MACRO_ASSERT_TRUE(ajsonv(j_copy)   != s);
    MACRO_ASSERT_TRUE(ajsonv(j_nocopy) == s);

    /* Mutate source; only nocopy reflects change */
    s[0] = 'J';
    MACRO_ASSERT_STREQ(ajsonv(j_nocopy), "Jello");
    MACRO_ASSERT_STREQ(ajsonv(j_copy),   "hello");

    free(s);
    aml_pool_destroy(pool);
}

MACRO_TEST(string_nocopy_slice_with_nul_dump_length) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    const char bytes[7] = { 'a','b','c','\0','d','e','f' };
    ajson_t *j = ajson_string_nocopy(pool, bytes, sizeof(bytes));
    MACRO_ASSERT_TRUE(j && ajson_is_string(j));
    MACRO_ASSERT_EQ_SZ(j->length, sizeof(bytes));

    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    char *end = ajson_dump_to_memory(buf, j);
    /* 2 quotes + 7 bytes -> 9 total; cannot strcmp due to embedded NUL */
    MACRO_ASSERT_EQ_SZ((size_t)(end - buf), 9);

    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(buffer_dump_filters_invalid_utf8_too) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char bad[] = { '{','"','s','"',':','"','\xC3','\x28','"', '}', 0 }; // invalid 2-byte start
    ajson_t *j = ajson_parse_string(pool, bad);
    MACRO_ASSERT_TRUE(j && !ajson_is_error(j));

    aml_buffer_t *bh = aml_buffer_init(32);
    ajson_dump_to_buffer(bh, j);
    MACRO_ASSERT_STREQ(aml_buffer_data(bh), "{\"s\":\"(\"}");

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

/* ---------- Additional JSON edge tests ---------- */

MACRO_TEST(bom_is_rejected) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *s = "\xEF\xBB\xBF{}";              /* UTF-8 BOM */
    ajson_t *j = ajson_parse_string(pool, s);
    MACRO_ASSERT_TRUE(ajson_is_error(j));
    aml_pool_destroy(pool);
}

MACRO_TEST(trailing_garbage_ignored) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = ajson_parse_string(pool, "{} 42");
    MACRO_ASSERT_TRUE(is_ok(j));
    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(non_json_literals_rejected) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{ \"n\": NaN }")));
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{ \"n\": Infinity }")));
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, "{ \"t\": True }")));
    aml_pool_destroy(pool);
}

MACRO_TEST(number_type_classification) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"z\":0,\"m\":-0,\"d\":0.0,\"e\":1e2}");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_TRUE(ajson_type(ajsono_scan(j,"z")) == zero);
    MACRO_ASSERT_TRUE(ajson_type(ajsono_scan(j,"m")) == number);
    MACRO_ASSERT_TRUE(ajson_type(ajsono_scan(j,"d")) == decimal);
    MACRO_ASSERT_TRUE(ajson_type(ajsono_scan(j,"e")) == number);
    aml_pool_destroy(pool);
}

MACRO_TEST(keys_unicode_escapes_are_not_decoded) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{ \"\\u0041\": 1 }");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j, "\\u0041")), "1");
    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"\\u0041\":1}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(utf8_4byte_roundtrip) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"s\":\"\xF0\x9F\x98\x80\"}"); /* 😀 */
    MACRO_ASSERT_TRUE(is_ok(j));
    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"s\":\"\xF0\x9F\x98\x80\"}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(decode_invalid_unicode_escape_copied) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    const char *enc = "\\u12G4";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, (char*)enc, strlen(enc));
    MACRO_ASSERT_EQ_SZ(out_len, 6);
    MACRO_ASSERT_TRUE(memcmp(dec, "\\u12G4", 6) == 0);
    aml_pool_destroy(pool);
}

MACRO_TEST(empty_key_allowed) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"\":1}");
    MACRO_ASSERT_TRUE(is_ok(j));
    MACRO_ASSERT_STREQ(ajsonv(ajsono_scan(j, "")), "1");
    aml_pool_destroy(pool);
}

MACRO_TEST(solidus_preserved_in_dump) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *j = P(pool, "{\"s\":\"\\/path\"}");
    size_t need = ajson_dump_estimate(j);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, j);
    MACRO_ASSERT_STREQ(buf, "{\"s\":\"\\/path\"}");
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(raw_string_builder_can_emit_invalid_json) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "s", ajson_str(pool, "a\"b"), true);
    size_t need = ajson_dump_estimate(o);
    char *buf = (char*)malloc(need);
    ajson_dump_to_memory(buf, o);
    MACRO_ASSERT_TRUE(ajson_is_error(P(pool, buf)));
    free(buf);
    aml_pool_destroy(pool);
}

MACRO_TEST(extract_float_array_mixed) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *arr = ajsona(pool);
    ajsona_append(arr, ajson_decimal_string(pool, "1.25"));
    ajsona_append(arr, ajson_number(pool, 2));
    ajsona_append(arr, ajson_str(pool, "x"));
    size_t n = 0;
    float *f = ajson_extract_float_array(&n, pool, arr);
    MACRO_ASSERT_EQ_SZ(n, 3);
    MACRO_ASSERT_TRUE(f[0] > 1.24f && f[0] < 1.26f);
    MACRO_ASSERT_TRUE(f[1] == 2.0f);
    MACRO_ASSERT_TRUE(f[2] == 0.0f);
    aml_pool_destroy(pool);
}

MACRO_TEST(deep_nesting_arrays) {
    aml_pool_t *pool = aml_pool_init(1 << 20);
    aml_buffer_t *bh = aml_buffer_init(1 << 16);
    for (int i = 0; i < 64; ++i) aml_buffer_appendc(bh, '[');
    aml_buffer_appends(bh, "0");
    for (int i = 0; i < 64; ++i) aml_buffer_appendc(bh, ']');
    ajson_t *j = ajson_parse_string(pool, aml_buffer_data(bh));
    MACRO_ASSERT_TRUE(is_ok(j) && ajson_is_array(j));
    ajson_t *cur = j;
    for (int i = 0; i < 63; ++i) { cur = ajsona_nth(cur, 0); MACRO_ASSERT_TRUE(cur); }
    MACRO_ASSERT_STREQ(ajsonv(ajsona_nth(cur, 0)), "0");
    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}


/* ---------- Conversions: string/number/bool ---------- */

MACRO_TEST(conv_string_numeric_to_int_double_bool) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* String nodes that look numeric should parse */
    ajson_t *s_int = ajson_str(pool, "123");
    ajson_t *s_neg = ajson_str(pool, "-7");
    ajson_t *s_dec = ajson_str(pool, "3.5");
    ajson_t *s_bad = ajson_str(pool, "abc");

    MACRO_ASSERT_EQ_INT(ajson_to_int(s_int, 0), 123);
    MACRO_ASSERT_EQ_INT(ajson_to_int(s_neg, 0), -7);

    /* Double should parse decimals; int on decimal may be impl-defined.
       Keep it to double to avoid relying on macro_to_int specifics. */
    MACRO_ASSERT_TRUE(ajson_to_double(s_dec, 0.0) > 3.49 && ajson_to_double(s_dec, 0.0) < 3.51);

    /* Non-numeric -> defaults */
    MACRO_ASSERT_EQ_INT(ajson_to_int(s_bad, 42), 42);
    MACRO_ASSERT_TRUE(ajson_to_double(s_bad, 1.25) == 1.25);

    /* Bool nodes convert naturally */
    MACRO_ASSERT_TRUE(ajson_to_bool(ajson_true(pool), false));
    MACRO_ASSERT_FALSE(ajson_to_bool(ajson_false(pool), true));

    /* Non-bool string leaves default */
    MACRO_ASSERT_FALSE(ajson_to_bool(s_bad, false));

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_number_nodes_all_paths) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    ajson_t *z  = ajson_zero(pool);                    /* "0" */
    ajson_t *n  = ajson_number(pool, -42);             /* "-42" */
    ajson_t *dp = ajson_decimal_string(pool, "10.25"); /* "10.25" */

    MACRO_ASSERT_TRUE(ajson_is_number(z));
    MACRO_ASSERT_TRUE(ajson_is_number(n));
    MACRO_ASSERT_TRUE(ajson_is_number(dp));

    MACRO_ASSERT_EQ_INT(ajson_to_int(z, 99), 0);
    MACRO_ASSERT_EQ_INT(ajson_to_int(n, 99), -42);
    MACRO_ASSERT_TRUE(ajson_to_double(dp, 0.0) > 10.24 && ajson_to_double(dp, 0.0) < 10.26);

    /* Bool semantics:
       - "0" => false (recognized)
       - nonzero numeric strings are NOT recognized as true; default is returned. */
    MACRO_ASSERT_FALSE(ajson_to_bool(z, true));      /* "0" -> false even if default=true */
    MACRO_ASSERT_TRUE(ajson_to_bool(n, false) == false);
    MACRO_ASSERT_TRUE(ajson_to_bool(n, true)  == true);

    aml_pool_destroy(pool);
}


MACRO_TEST(conv_uint64_boundaries_and_overflow) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    ajson_t *u_ok = ajson_str(pool, "18446744073709551615");  /* UINT64_MAX */
    ajson_t *u_ov = ajson_str(pool, "18446744073709551616");  /* overflow */
    ajson_t *u_neg = ajson_str(pool, "-1");                   /* negative to unsigned */

    MACRO_ASSERT_TRUE(ajson_to_uint64(u_ok, 0) == UINT64_MAX);

    /* Expect defaults on overflow/negative */
    MACRO_ASSERT_TRUE(ajson_to_uint64(u_ov, 7) == 7);
    MACRO_ASSERT_TRUE(ajson_to_uint64(u_neg, 9) == 9);

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_int64_boundaries) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    char minbuf[32], maxbuf[32];
    snprintf(minbuf, sizeof(minbuf), "%lld", (long long)INT64_MIN);
    snprintf(maxbuf, sizeof(maxbuf), "%lld", (long long)INT64_MAX);

    ajson_t *smin = ajson_str(pool, minbuf);
    ajson_t *smax = ajson_str(pool, maxbuf);

    MACRO_ASSERT_TRUE(ajson_to_int64(smin, 1) == INT64_MIN);
    MACRO_ASSERT_TRUE(ajson_to_int64(smax, 1) == INT64_MAX);

    /* One below/above -> default */
    ajson_t *below = ajson_str(pool, "-9223372036854775809");
    ajson_t *above = ajson_str(pool,  "9223372036854775808");
    MACRO_ASSERT_TRUE(ajson_to_int64(below, 13) == 13);
    MACRO_ASSERT_TRUE(ajson_to_int64(above, 17) == 17);

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_non_value_types_return_defaults) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *obj = ajsono(pool);
    ajson_t *arr = ajsona(pool);

    MACRO_ASSERT_EQ_INT(ajson_to_int(obj, 5), 5);
    MACRO_ASSERT_EQ_INT(ajson_to_int(arr, 6), 6);
    MACRO_ASSERT_TRUE(ajson_to_double(obj, 1.0) == 1.0);
    MACRO_ASSERT_TRUE(ajson_to_bool(arr, true) == true);

    /* NULL node -> defaults as well */
    MACRO_ASSERT_EQ_INT(ajson_to_int(ajson_null(pool), 11), 11);

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_scan_get_find_defaults_when_missing) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "n", ajson_number(pool, 123), true);

    /* missing key */
    MACRO_ASSERT_EQ_INT(ajsono_scan_int(o, "missing", -1), -1);
    MACRO_ASSERT_EQ_INT(ajsono_get_int(o,  "missing", -2), -2);
    MACRO_ASSERT_EQ_INT(ajsono_find_int(o, "missing", -3), -3);

    /* present key */
    MACRO_ASSERT_EQ_INT(ajsono_scan_int(o, "n", 0), 123);
    MACRO_ASSERT_EQ_INT(ajsono_get_int(o,  "n", 0), 123);
    MACRO_ASSERT_EQ_INT(ajsono_find_int(o, "n", 0), 123);

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_string_to_uint32_and_float) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    ajson_t *su = ajson_str(pool, "4294967295"); /* UINT32_MAX */
    ajson_t *fu = ajson_str(pool, "3.14159");
    ajson_t *bad= ajson_str(pool, "nan-ish");

    MACRO_ASSERT_TRUE(ajson_to_uint32(su, 0) == 4294967295u);
    MACRO_ASSERT_TRUE(ajson_to_float(fu, 0.0f) > 3.14f && ajson_to_float(fu, 0.0f) < 3.15f);

    /* non-numeric -> default */
    MACRO_ASSERT_TRUE(ajson_to_uint32(bad, 77) == 77u);
    MACRO_ASSERT_TRUE(ajson_to_float(bad, 1.0f) == 1.0f);

    aml_pool_destroy(pool);
}

MACRO_TEST(conv_bool_from_string_literals_case) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* If macro_to_bool is strict, these will keep defaults; if it’s permissive,
       they may parse. We assert conservatively on defaults for non-exact matches. */
    ajson_t *s_true  = ajson_str(pool, "true");
    ajson_t *s_FALSE = ajson_str(pool, "FALSE");
    ajson_t *s_yes   = ajson_str(pool, "yes");

    /* exact lower-case "true"/"false" should behave; others keep defaults */
    MACRO_ASSERT_TRUE(ajson_to_bool(s_true,  false) == true);
    MACRO_ASSERT_TRUE(ajson_to_bool(s_FALSE, true)  == false);  /* default not retained as test is case-insensitive */
    MACRO_ASSERT_TRUE(ajson_to_bool(s_yes,   false) == true); /* default not retained as yes is valid */

    aml_pool_destroy(pool);
}

/* ---------- NEW: try() converters ---------- */

MACRO_TEST(node_try_converters) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    int vi = 0;
    double vd = 0.0;
    uint64_t vu = 0;
    bool vb = false;

    MACRO_ASSERT_TRUE( ajson_try_to_int   (ajson_str(pool, "123"), &vi) && vi == 123 );
    MACRO_ASSERT_FALSE(ajson_try_to_int   (ajson_str(pool, "12x"), &vi));

    MACRO_ASSERT_TRUE( ajson_try_to_double(ajson_str(pool, "3.5e1"), &vd) && fabs(vd - 35.0) < 1e-9 );
    MACRO_ASSERT_TRUE( ajson_try_to_uint64(ajson_str(pool, "18446744073709551615"), &vu) && vu == UINT64_MAX );
    MACRO_ASSERT_FALSE(ajson_try_to_uint64(ajson_str(pool, "18446744073709551616"), &vu));

    MACRO_ASSERT_TRUE( ajson_try_to_bool  (ajson_str(pool, "true"), &vb) && vb == true );
    MACRO_ASSERT_TRUE( ajson_try_to_bool  (ajson_str(pool, "0"),    &vb) && vb == false );
    MACRO_ASSERT_FALSE(ajson_try_to_bool  (ajson_str(pool, "maybe"), &vb));

    aml_pool_destroy(pool);
}

MACRO_TEST(object_try_helpers_scan_get_find) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "i", ajson_str(pool, "42"),   true);
    ajsono_append(o, "f", ajson_str(pool, "2.5"),  true);
    ajsono_append(o, "b", ajson_str(pool, "true"), true);

    int vi=0; double vf=0.0; bool vb=false;

    MACRO_ASSERT_TRUE( ajsono_scan_try_int   (o, "i", &vi) && vi == 42 );
    MACRO_ASSERT_TRUE( ajsono_get_try_double (o, "f", &vf) && fabs(vf - 2.5) < 1e-6 );
    MACRO_ASSERT_TRUE( ajsono_find_try_bool  (o, "b", &vb) && vb == true );

    MACRO_ASSERT_FALSE(ajsono_find_try_int   (o, "missing", &vi));

    aml_pool_destroy(pool);
}

/* Regression for the get<->find snapshot/tree crossover bug:
   Calling find() first builds the RB-tree; subsequent get() must rebuild the snapshot. */
MACRO_TEST(get_after_find_rebuilds_snapshot) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    ajson_t *o = ajsono(pool);
    ajsono_append(o, "n", ajson_number(pool, 123), true);

    int dummy = 0;
    MACRO_ASSERT_FALSE(ajsono_find_try_int(o, "missing", &dummy)); /* force tree build */

    /* This used to fail before the fix; now it should pass. */
    MACRO_ASSERT_EQ_INT(ajsono_get_int(o, "n", 0), 123);

    aml_pool_destroy(pool);
}

/* -------- register & run -------- */

int main(void) {
    macro_test_case tests[256];
    size_t test_count = 0;

    MACRO_ADD(tests, type_predicates_nullptr);
    MACRO_ADD(tests, type_predicates_values);

    MACRO_ADD(tests, parse_array_basic);
    MACRO_ADD(tests, numbers_invalid);
    MACRO_ADD(tests, utf8_filter_in_value_dumps);
    MACRO_ADD(tests, file_dump_smoke);
    MACRO_ADD(tests, object_indexes_and_mutation);
    MACRO_ADD(tests, array_append_nth_clear);
    MACRO_ADD(tests, encode_decode);
    MACRO_ADD(tests, error_row_col);

    MACRO_ADD(tests, parse_object_basic);
    MACRO_ADD(tests, numbers_valid);
    MACRO_ADD(tests, path_helpers);

    MACRO_ADD(tests, dump_estimate_matches_output);
    MACRO_ADD(tests, pretty_estimate_matches_output);
    MACRO_ADD(tests, parse_string_is_nondestructive);
    MACRO_ADD(tests, keys_escaped_quote_and_path_escape);
    MACRO_ADD(tests, duplicate_keys_scan_vs_scanr);
    MACRO_ADD(tests, remove_invalidate_get_index);
    MACRO_ADD(tests, array_erase_middle);
    MACRO_ADD(tests, conversion_helpers_defaults_and_values);
    MACRO_ADD(tests, decode_unicode_surrogate_pair_and_invalid);
    MACRO_ADD(tests, pretty_indentation_contains_expected_spaces);

    MACRO_ADD(tests, empty_values_and_whitespace);
    MACRO_ADD(tests, encode_embedded_nul_and_controls);
    MACRO_ADD(tests, invalid_utf8_truncated_sequence_end);
    MACRO_ADD(tests, pretty_to_buffer_equals_pretty_string);
    MACRO_ADD(tests, object_remove_head_tail_middle);
    MACRO_ADD(tests, path_filter_then_field);
    MACRO_ADD(tests, extract_string_array_variants);
    MACRO_ADD(tests, conversion_defaults_on_string);
    MACRO_ADD(tests, keys_preserve_escapes_on_dump);

    MACRO_ADD(tests, trailing_comma_invalid);

    MACRO_ADD(tests, pretty_step_zero_defaults_to_two);
    MACRO_ADD(tests, pretty_step_negative_defaults_to_two);

    MACRO_ADD(tests, decode_simple_no_escapes_zerocopy);
    MACRO_ADD(tests, decode_all_simple_escapes);
    MACRO_ADD(tests, encode_slash_quote_backslash);

    MACRO_ADD(tests, uint64_roundtrip_max);
    MACRO_ADD(tests, number_stringf_variants);

    MACRO_ADD(tests, path_index_out_of_range);
    MACRO_ADD(tests, syntax_errors_basic);

    MACRO_ADD(tests, object_insertion_order_preserved);
    MACRO_ADD(tests, get_after_set_rebuilds_snapshot);

    MACRO_ADD(tests, to_strd_defaults_on_null_and_error);
    MACRO_ADD(tests, ajsona_count_null_safe);

    MACRO_ADD(tests, roundtrip_numbers_stability);
    MACRO_ADD(tests, object_insert_updates_existing_and_adds_new);

    MACRO_ADD(tests, nocopy_encode_string_len_zerocopy_safe);
    MACRO_ADD(tests, nocopy_encode_alloc_when_escapes);
    MACRO_ADD(tests, copy_vs_nocopy_string_builders);
    MACRO_ADD(tests, string_nocopy_slice_with_nul_dump_length);
    MACRO_ADD(tests, buffer_dump_filters_invalid_utf8_too);
    MACRO_ADD(tests, nocopy_encode_alias_when_clean);
    MACRO_ADD(tests, nocopy_encode_length_is_captured);

    MACRO_ADD(tests, bom_is_rejected);
    MACRO_ADD(tests, trailing_garbage_ignored);
    MACRO_ADD(tests, non_json_literals_rejected);
    MACRO_ADD(tests, number_type_classification);
    MACRO_ADD(tests, keys_unicode_escapes_are_not_decoded);
    MACRO_ADD(tests, utf8_4byte_roundtrip);
    MACRO_ADD(tests, decode_invalid_unicode_escape_copied);
    MACRO_ADD(tests, empty_key_allowed);
    MACRO_ADD(tests, solidus_preserved_in_dump);
    MACRO_ADD(tests, raw_string_builder_can_emit_invalid_json);
    MACRO_ADD(tests, extract_float_array_mixed);
    MACRO_ADD(tests, deep_nesting_arrays);

    MACRO_ADD(tests, conv_string_numeric_to_int_double_bool);
    MACRO_ADD(tests, conv_number_nodes_all_paths);
    MACRO_ADD(tests, conv_uint64_boundaries_and_overflow);
    MACRO_ADD(tests, conv_int64_boundaries);
    MACRO_ADD(tests, conv_non_value_types_return_defaults);
    MACRO_ADD(tests, conv_scan_get_find_defaults_when_missing);
    MACRO_ADD(tests, conv_string_to_uint32_and_float);
    MACRO_ADD(tests, conv_bool_from_string_literals_case);

    /* New functionality tests */
    MACRO_ADD(tests, node_try_converters);
    MACRO_ADD(tests, object_try_helpers_scan_get_find);
    MACRO_ADD(tests, get_after_find_rebuilds_snapshot);

    macro_run_all("ajson", tests, test_count);
    return 0;
}
