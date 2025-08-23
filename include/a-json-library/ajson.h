// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _ajson_H
#define _ajson_H

/* Optional feature flags (leave commented in production):
   - AJSON_DEBUG: richer error locations on parse failures.
   - AJSON_FILL_TEST / AJSON_SORT_TEST: dev-only helpers. */
//#define AJSON_DEBUG
//#define AJSON_FILL_TEST
//#define AJSON_SORT_TEST

#include "a-memory-library/aml_buffer.h"
#include "a-memory-library/aml_pool.h"

#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>  /* ssize_t in ajson_number */
#include <stdio.h>      /* sscanf/snprintf used from inline impl */
#include <string.h>     /* strlen/strcmp/strcpy/memcpy from inline impl */
#include <stdarg.h>     /* va_list for *_stringf in inline impl */

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================
 *  Design notes / contracts
 * ===========================
 *
 * - Speed first. Containers keep insertion order. Lookups use either a
 *   one-time sorted-array snapshot ("get") or an RB-tree map ("find").
 *
 * - Strings (values & keys) are assumed to be **already JSON-encoded**
 *   (quotes, backslashes, control bytes escaped). If you have raw text,
 *   use ajson_encode[_str|_string] or encode yourself. Dumpers do not
 *   escape again; they only add surrounding quotes for string nodes.
 *
 * - UTF-8 is *not* validated in the parser. Dump-to-memory/buffer/file
 *   will **drop invalid UTF-8 bytes inside strings** (they copy only valid
 *   sequences). If you need strict validation, do it ahead of time.
 *
 * - Parser stores string contents and object keys *verbatim and encoded*.
 *   No unescaping/normalization at parse time (unless you compile tests).
 *   Use ajson_decode/ajson_decode2 to unescape on demand.
 *
 * - ajson_parse is destructive to the input buffer (in-place NULs).
 *   Use ajson_parse_string to get a pool-backed copy.
 *
 * - Object key indexes are snapshots:
 *     * ajsono_get / ajsono_get_node build a sorted-array index **once**.
 *       Appends after that are invisible to this index until it rebuilds.
 *     * ajsono_find uses a tree map. Appends are **not** auto-inserted
 *       unless you used ajsono_insert/ajsono_set; plain ajsono_append
 *       will leave the tree stale.
 *
 * - Numbers are stored as strings. Converters parse on demand and return
 *   your default on failure. Special type ZERO distinguishes literal "0".
 *
 * - Dumpers write by length (no printf("%s")). FILE* dumpers can emit
 *   embedded NULs if you put them there (not valid JSON); buffer/memory
 *   dumpers also preserve NULs in the raw bytes they copy.
 */

/* Forward decls */
struct ajson_s;  typedef struct ajson_s  ajson_t;
struct ajsona_s; typedef struct ajsona_s ajsona_t;
struct ajsono_s; typedef struct ajsono_s ajsono_t;

/* Public type tag.
   NOTE: Values map to implementation tags:
     error=0, object=1, array=2, null=4, string=5,
     bool_false=6, zero=7, number=8, decimal=9, bool_true=10
*/
typedef enum {
  error      = 0,
  object     = 1,
  array      = 2,
  null       = 4,
  string     = 5,
  bool_false = 6,
  zero       = 7,  /* literal "0" */
  number     = 8,  /* integer-like, incl. "-0" */
  decimal    = 9,  /* has '.' or exponent */
  bool_true  = 10
} ajson_type_t;

/* =========
 * Parsing
 * ========= */

/** Parse JSON from [p,ep). Destructive: inserts NULs in-place.
 *  - Stores strings/keys in encoded form (no unescape).
 *  - No UTF-8 validation; trailing commas are rejected; leading zeros
 *    are rejected (except "0"); numbers support exponents.
 *  - Returns error node on failure (check with ajson_is_error()).
 *  Lifetime: returned node points into the caller's buffer; keep it alive.
 *  NOTE: The caller must ensure **one writable byte past ep** so the parser
 *  can temporarily write a '\0' at token boundaries. If you cannot guarantee
 *  that, use ajson_parse_string() instead. */
ajson_t *ajson_parse(aml_pool_t *pool, char *p, char *ep);

/** Convenience: non-destructive parse of a NUL-terminated string.
 *  Duplicates 's' into the pool, then calls ajson_parse. */
static inline ajson_t *ajson_parse_string(aml_pool_t *pool, const char *s);

/** True iff j is an error node. */
static inline bool ajson_is_error(ajson_t *j);

/** Human-readable parse error → FILE*. Row/column are byte-based; escaped
 *  backslashes are roughly skipped during counting. */
void ajson_dump_error(FILE *out, ajson_t *j);

/** Human-readable parse error → aml_buffer. See ajson_dump_error notes. */
void ajson_dump_error_to_buffer(aml_buffer_t *bh, ajson_t *j);

/** Return the tag of a node. O(1). */
static inline ajson_type_t ajson_type(ajson_t *j);

/* =================
 * Dump / stringify
 * ================= */

/** Dump compact JSON to FILE*.
 *  - Strings are *assumed encoded* already; only quotes are added.
 *  - Invalid UTF-8 bytes inside strings are dropped.
 *  - Writes raw bytes by length; embedded NULs are written as-is. */
void ajson_dump(FILE *out, ajson_t *a);

/** Dump pretty JSON to FILE* with 'indent_step' spaces per level.
 *  If step <= 0, defaults to 2. Same encoding/UTF-8 caveats as ajson_dump. */
void ajson_dump_pretty(FILE *out, ajson_t *a, int indent_step);

/** Pretty-print to aml_buffer. Pre-sizes then fills; same caveats as dump. */
void ajson_dump_pretty_to_buffer(aml_buffer_t *bh, ajson_t *a, int indent_step);

/** Dump compact JSON to aml_buffer (non-pretty).
 *  Drops invalid UTF-8 bytes in strings; does not escape. */
void ajson_dump_to_buffer(aml_buffer_t *bh, ajson_t *a);

/** Estimate bytes for compact dump (no NUL). Assumes no escaping occurs. */
size_t ajson_dump_estimate(ajson_t *a);

/** Estimate bytes for pretty dump **including** trailing NUL.
 *  Assumes no escaping occurs at dump time. */
size_t ajson_dump_pretty_estimate(ajson_t *a, int indent_step);

/** Dump compact JSON into caller-provided buffer 's' (must have at least
 *  ajson_dump_estimate(a)+1 bytes). Returns end pointer (points at '\0'). */
char *ajson_dump_to_memory(char *s, ajson_t *a);

/** Deprecated: use ajson_stringify. Compact JSON as a single pool alloc. */
char *ajson_dump_using_pool(aml_pool_t *pool, ajson_t *a);

/** Compact JSON as a single pool allocation. */
char *ajson_stringify(aml_pool_t *pool, ajson_t *a);

/** Pretty JSON as a single pool allocation. */
char *ajson_stringify_pretty(aml_pool_t *pool, ajson_t *a, int indent_step);

/* ========================
 * Encode / decode strings
 * ======================== */

/** Decode JSON escape sequences in [s, s+length) into UTF-8.
 *  May return 's' unchanged if no escapes are present (aliasing!).
 *  Otherwise returns a pool-allocated buffer. */
char *ajson_decode(aml_pool_t *pool, char *s, size_t length);

/** Decode like ajson_decode, but also returns decoded length via *rlen.
 *  May alias input if nothing to decode. */
char *ajson_decode2(size_t *rlen, aml_pool_t *pool, char *s, size_t length);

/** Escape JSON specials/control bytes in [s, s+length).
 *  May return 's' unchanged if nothing needs escaping (aliasing!),
 *  otherwise returns a pool-allocated buffer. */
char *ajson_encode(aml_pool_t *pool, char *s, size_t length);

/* ==========================
 * Access raw storage safely
 * ========================== */

/** For non-container nodes:
 *  - string  → decoded copy (pool) if needed; else may alias original.
 *  - number/bool/null/zero/decimal → internal string view.
 *  Returns NULL for object/array/error. */
static inline char *ajsond(aml_pool_t *pool, ajson_t *j);

/** Return internal string view for string-or-scalar nodes (encoded for
 *  strings). NULL for object/array/error. */
static inline char *ajsonv(ajson_t *j);

/* ==========================
 * Node construction helpers
 * ========================== */

/** Make a STRING node referencing [s,s+length).
 *  Caller must ensure 's' is already JSON-escaped if needed. */
static inline ajson_t *ajson_string(aml_pool_t *pool, const char *s, size_t length);

/** Make a STRING node referencing s (strlen).
 *  Same escaping caveats as ajson_string. */
static inline ajson_t *ajson_str(aml_pool_t *pool, const char *s);

/** Make a STRING node from raw text by escaping. Internally calls
 *  ajson_encode. */
static inline ajson_t *ajson_encode_string(aml_pool_t *pool, const char *s, size_t length);

/** Like ajson_encode_string for NUL-terminated input. */
static inline ajson_t *ajson_encode_str(aml_pool_t *pool, const char *s);

/** Make a STRING node referencing [s,s+length). **No copy.**
 *  Caller must ensure 's' outlives the JSON tree and is already
 *  JSON-escaped if needed. */
static inline ajson_t *ajson_string_nocopy(aml_pool_t *pool, const char *s, size_t length);

/** Make a STRING node referencing s (strlen). **No copy.**
 *  Same ownership/escaping caveats as ajson_string. */
static inline ajson_t *ajson_str_nocopy(aml_pool_t *pool, const char *s);

/** Make a STRING node from raw text by escaping. Internally calls
 *  ajson_encode; if no escaping is needed it may **alias the input**.
 * Because it may alias, caller must ensure 's' outlives the JSON tree.
 */
static inline ajson_t *ajson_encode_string_nocopy(aml_pool_t *pool, const char *s, size_t length);

/** Like ajson_encode_string for NUL-terminated input. May alias input.
 * Because it may alias, caller must ensure 's' outlives the JSON tree.
 */
static inline ajson_t *ajson_encode_str_nocopy(aml_pool_t *pool, const char *s);

/** Literal nodes (small, fast). Values point to static string constants. */
static inline ajson_t *ajson_true(aml_pool_t *pool);
static inline ajson_t *ajson_false(aml_pool_t *pool);
static inline ajson_t *ajson_bool(aml_pool_t *pool, bool v);
static inline ajson_t *ajson_null(aml_pool_t *pool);
static inline ajson_t *ajson_zero(aml_pool_t *pool);

/** Numeric helpers. Stored as decimal strings.
 *  ajson_number/uint64 embed the printed number in the node allocation. */
static inline ajson_t *ajson_uint64(aml_pool_t *pool, uint64_t n);
static inline ajson_t *ajson_number(aml_pool_t *pool, ssize_t n);

/** Use an existing numeric string. Copies into the node allocation. */
static inline ajson_t *ajson_number_string(aml_pool_t *pool, char *s);

/** Format + store numeric string (note: creates a temp pool string and then
 *  copies again into the node; double alloc/copy). */
static inline ajson_t *ajson_number_stringf(aml_pool_t *pool, char *s, ...);

/** Decimal variants (same storage semantics as number_*). */
static inline ajson_t *ajson_decimal_string(aml_pool_t *pool, char *s);
static inline ajson_t *ajson_decimal_stringf(aml_pool_t *pool, char *s, ...);

/* ======================
 * Type test convenience
 * ====================== */

/** True iff j is an object. */
static inline bool ajson_is_object(ajson_t *j);
/** True iff j is an array. */
static inline bool ajson_is_array(ajson_t *j);
/** True iff j is a JSON null. */
static inline bool ajson_is_null(ajson_t *j);
/** True iff j is bool_true or bool_false. */
static inline bool ajson_is_bool(ajson_t *j);
/** True iff j is a string. */
static inline bool ajson_is_string(ajson_t *j);
/** True iff j is ZERO, NUMBER, or DECIMAL. */
static inline bool ajson_is_number(ajson_t *j);

/* ======================
 * Value conversions
 * ======================
 * All functions:
 *   - Accept string/number literal nodes.
 *   - Return 'default_value' if j is NULL or parsing fails.
 *   - Fast, no locale, typical C parsing rules.
 */
static inline int      ajson_to_int(ajson_t *j, int default_value);
static inline int32_t  ajson_to_int32(ajson_t *j, int32_t default_value);
static inline uint32_t ajson_to_uint32(ajson_t *j, uint32_t default_value);
static inline int64_t  ajson_to_int64(ajson_t *j, int64_t default_value);
static inline uint64_t ajson_to_uint64(ajson_t *j, uint64_t default_value);
static inline float    ajson_to_float(ajson_t *j, float default_value);
static inline double   ajson_to_double(ajson_t *j, double default_value);
static inline bool     ajson_to_bool(ajson_t *j, bool default_value);

/* “Try” variants (no defaults). Return true on success and fill *out. */
static inline bool ajson_try_to_int    (ajson_t *j, int      *out);
static inline bool ajson_try_to_long   (ajson_t *j, long     *out);
static inline bool ajson_try_to_int32  (ajson_t *j, int32_t  *out);
static inline bool ajson_try_to_uint32 (ajson_t *j, uint32_t *out);
static inline bool ajson_try_to_int64  (ajson_t *j, int64_t  *out);
static inline bool ajson_try_to_uint64 (ajson_t *j, uint64_t *out);
static inline bool ajson_try_to_float  (ajson_t *j, float    *out);
static inline bool ajson_try_to_double (ajson_t *j, double   *out);
static inline bool ajson_try_to_bool   (ajson_t *j, bool     *out);

/** Return decoded string (pool) or default_value if missing, else empty. */
static inline char *ajson_to_strd(aml_pool_t *pool, ajson_t *j, const char *default_value);

/** Return internal string view or default_value if missing. */
static inline char *ajson_to_str(ajson_t *j, const char *default_value);

/* ===========================
 * Small extraction helpers
 * =========================== */

/** Decode and return string ("" if not present). */
static inline char *ajson_extract_string(aml_pool_t *pool, ajson_t *node);
/** Return int (0 if missing). */
static inline int ajson_extract_int(ajson_t *node);
/** Return bool (false if missing). */
static inline bool ajson_extract_bool(ajson_t *node);
/** Return uint32 (0 if missing). */
static inline uint32_t ajson_extract_uint32(ajson_t *node);

/** Extract array of strings:
 *  - If node is not an array → returns a 1-element array.
 *  - Decodes each element. Result is pool-allocated; last element is NULL. */
static inline char **ajson_extract_string_array(size_t *count, aml_pool_t *pool, ajson_t *node);

/** Extract array of floats. Returns NULL if node not an array. */
static inline float *ajson_extract_float_array(size_t *num, aml_pool_t *pool, ajson_t *node);

/* =========
 * Arrays
 * ========= */

/** Create an empty array (pool-owned). */
static inline ajson_t *ajsona(aml_pool_t *pool);

/** Count elements. O(1). */
static inline int ajsona_count(ajson_t *j);

/** Linear scan to nth (0-based). O(n/2). */
static inline ajson_t *ajsona_scan(ajson_t *j, int nth);

/** Direct-access (builds/uses an internal snapshot table). O(1) after build. */
static inline ajson_t  *ajsona_nth(ajson_t *j, int nth);
static inline ajsona_t *ajsona_nth_node(ajson_t *j, int nth);

/** Doubly-linked iteration helpers (preserve insertion order). */
static inline ajsona_t *ajsona_first(ajson_t *j);
static inline ajsona_t *ajsona_last(ajson_t *j);
static inline ajsona_t *ajsona_next(ajsona_t *j);
static inline ajsona_t *ajsona_previous(ajsona_t *j);

/** Append item (sets item->parent). Invalidates any direct-access table. */
static inline void ajsona_append(ajson_t *j, ajson_t *item);

/** Erase a node from its array. Fixes links, decrements count, invalidates
 *  direct-access cache. Does not free pool memory. */
static inline void ajsona_erase(ajsona_t *n);

/** Clear an array by unlinking all items. Keeps pool allocations. */
static inline void ajsona_clear(ajson_t *j);

/* ==========
 * Objects
 * ========== */

/** Create an empty object (pool-owned). */
static inline ajson_t *ajsono(aml_pool_t *pool);

/** Count key-value pairs. O(1). */
static inline int ajsono_count(ajson_t *j);

/** Ordered iteration (insertion order). */
static inline ajsono_t *ajsono_first(ajson_t *j);
static inline ajsono_t *ajsono_last(ajson_t *j);
static inline ajsono_t *ajsono_next(ajsono_t *j);
static inline ajsono_t *ajsono_previous(ajsono_t *j);

/** Append without checking for existing key (fast). Keeps insertion order.
 *  If copy_key == false, stores the pointer as-is (caller must keep it alive).
 *  If copy_key == true, duplicates key into the pool.
 *  NOTE: Appending does **not** update any existing index; "get/find" views
 *  may be stale until rebuilt/inserted. */
static inline void ajsono_append(ajson_t *j, const char *key, ajson_t *item, bool copy_key);

/** Set (replace first matching key, else append). Slower than append.
 *  Maintains indexes: updates tree or invalidates array snapshot. */
static inline ajsono_t *ajsono_set(ajson_t *j, const char *key, ajson_t *item, bool copy_key);

/** Remove first matching key. Updates indexes appropriately. */
static inline bool ajsono_remove(ajson_t *j, const char *key);

/** Linear scans (no index). */
static inline ajson_t *ajsono_scan(ajson_t *j, const char *key);
static inline ajson_t *ajsono_scanr(ajson_t *j, const char *key);

/** Snapshot-based lookup. On first call, builds a sorted-array index over the
 *  current contents. **Appends after that are invisible** until the index is
 *  rebuilt (cleared implicitly by some mutators). */
static inline ajson_t  *ajsono_get(ajson_t *j, const char *key);
static inline ajsono_t *ajsono_get_node(ajson_t *j, const char *key);

/** Tree-based lookup/insert. If the RB-tree exists, ajsono_insert keeps it
 *  updated; plain ajsono_append does not. */
static inline ajsono_t *ajsono_find_node(ajson_t *j, const char *key);
static inline ajson_t  *ajsono_find(ajson_t *j, const char *key);
static inline ajsono_t *ajsono_insert(ajson_t *j, const char *key, ajson_t *item, bool copy_key);

/** Erase a specific object entry. Also updates indexes. */
static inline void ajsono_erase(ajsono_t *n);

/* Object value helpers (scan/get/find + convert). */
static inline int      ajsono_scan_int(ajson_t *j, const char *key, int default_value);
static inline int32_t  ajsono_scan_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_scan_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t  ajsono_scan_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_scan_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float    ajsono_scan_float(ajson_t *j, const char *key, float default_value);
static inline double   ajsono_scan_double(ajson_t *j, const char *key, double default_value);
static inline bool     ajsono_scan_bool(ajson_t *j, const char *key, bool default_value);
static inline char    *ajsono_scan_str(ajson_t *j, const char *key, const char *default_value);
static inline char    *ajsono_scan_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value);

/* Object “try” helpers — scan (linear), get (snapshot), find (tree). */
static inline bool ajsono_scan_try_int    (ajson_t *j, const char *key, int      *out);
static inline bool ajsono_scan_try_long   (ajson_t *j, const char *key, long     *out);
static inline bool ajsono_scan_try_int32  (ajson_t *j, const char *key, int32_t  *out);
static inline bool ajsono_scan_try_uint32 (ajson_t *j, const char *key, uint32_t *out);
static inline bool ajsono_scan_try_int64  (ajson_t *j, const char *key, int64_t  *out);
static inline bool ajsono_scan_try_uint64 (ajson_t *j, const char *key, uint64_t *out);
static inline bool ajsono_scan_try_float  (ajson_t *j, const char *key, float    *out);
static inline bool ajsono_scan_try_double (ajson_t *j, const char *key, double   *out);
static inline bool ajsono_scan_try_bool   (ajson_t *j, const char *key, bool     *out);


static inline int      ajsono_get_int(ajson_t *j, const char *key, int default_value);
static inline int32_t  ajsono_get_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_get_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t  ajsono_get_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_get_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float    ajsono_get_float(ajson_t *j, const char *key, float default_value);
static inline double   ajsono_get_double(ajson_t *j, const char *key, double default_value);
static inline bool     ajsono_get_bool(ajson_t *j, const char *key, bool default_value);
static inline char    *ajsono_get_str(ajson_t *j, const char *key, const char *default_value);
static inline char    *ajsono_get_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value);

static inline bool ajsono_get_try_int    (ajson_t *j, const char *key, int      *out);
static inline bool ajsono_get_try_long   (ajson_t *j, const char *key, long     *out);
static inline bool ajsono_get_try_int32  (ajson_t *j, const char *key, int32_t  *out);
static inline bool ajsono_get_try_uint32 (ajson_t *j, const char *key, uint32_t *out);
static inline bool ajsono_get_try_int64  (ajson_t *j, const char *key, int64_t  *out);
static inline bool ajsono_get_try_uint64 (ajson_t *j, const char *key, uint64_t *out);
static inline bool ajsono_get_try_float  (ajson_t *j, const char *key, float    *out);
static inline bool ajsono_get_try_double (ajson_t *j, const char *key, double   *out);
static inline bool ajsono_get_try_bool   (ajson_t *j, const char *key, bool     *out);

static inline int      ajsono_find_int(ajson_t *j, const char *key, int default_value);
static inline int32_t  ajsono_find_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_find_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t  ajsono_find_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_find_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float    ajsono_find_float(ajson_t *j, const char *key, float default_value);
static inline double   ajsono_find_double(ajson_t *j, const char *key, double default_value);
static inline bool     ajsono_find_bool(ajson_t *j, const char *key, bool default_value);
static inline char    *ajsono_find_str(ajson_t *j, const char *key, const char *default_value);
static inline char    *ajsono_find_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value);

static inline bool ajsono_find_try_int    (ajson_t *j, const char *key, int      *out);
static inline bool ajsono_find_try_long   (ajson_t *j, const char *key, long     *out);
static inline bool ajsono_find_try_int32  (ajson_t *j, const char *key, int32_t  *out);
static inline bool ajsono_find_try_uint32 (ajson_t *j, const char *key, uint32_t *out);
static inline bool ajsono_find_try_int64  (ajson_t *j, const char *key, int64_t  *out);
static inline bool ajsono_find_try_uint64 (ajson_t *j, const char *key, uint64_t *out);
static inline bool ajsono_find_try_float  (ajson_t *j, const char *key, float    *out);
static inline bool ajsono_find_try_double (ajson_t *j, const char *key, double   *out);
static inline bool ajsono_find_try_bool   (ajson_t *j, const char *key, bool     *out);
/* ==========
 * JSON path
 * ==========
 * Simple dotted path navigation over objects/arrays.
 * - Objects: dot-separated keys (encoded form).
 * - Arrays:
 *     • "idx" selects by 0-based index (linear scan w/ sscanf).
 *     • "key=value" selects first element whose object's key equals value
 *       (string compare on *encoded* ajsonv()).
 * Return NULL on miss.
 */
static inline ajson_t *ajsono_path(aml_pool_t *pool, ajson_t *j, const char *path);
static inline char    *ajsono_pathv(aml_pool_t *pool, ajson_t *j, const char *path);
static inline char    *ajsono_pathd(aml_pool_t *pool, ajson_t *j, const char *path);

/* Inline implementations */
#include "a-json-library/impl/ajson.h"

#ifdef __cplusplus
}
#endif

#endif /* _ajson_H */
