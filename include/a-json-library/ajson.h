/*
Copyright 2019 Andy Curtis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _ajson_H
#define _ajson_H

//#define AJSON_DEBUG
//#define AJSON_FILL_TEST
//#define AJSON_SORT_TEST

#include "a-memory-library/aml_buffer.h"
#include "a-memory-library/aml_pool.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ajson_s;
typedef struct ajson_s ajson_t;
struct ajsona_s;
typedef struct ajsona_s ajsona_t;
struct ajsono_s;
typedef struct ajsono_s ajsono_t;

/* This is the core function for parsing json.  This parser is not fully
 * compliant in that keys are expected to not include encodings (or if they do,
 * then you must encode the keys in the same way to match).  This is
 * destructive to p/ep.  If you need to keep the original string, then you
 * should duplicate it before calling this function. */
 */
ajson_t *ajson_parse(aml_pool_t *pool, char *p, char *ep);

/* Similar to ajson_parse, but will duplicate the string and call ajson_parse
 * so that it is non-destructive to the original string. */
static inline
ajson_t *ajson_parse_string(aml_pool_t *pool, const char *s);

/* If the parse fails, the value returned will be marked such that
 * ajson_error returns true.  If this happens, you can dump it to the screen
 * or to a buffer.  This should always be called after ajson_parse or
 * ajson_parse_string.
 */
static inline bool ajson_is_error(ajson_t *j);
void ajson_dump_error(FILE *out, ajson_t *j);
void ajson_dump_error_to_buffer(aml_buffer_t *bh, ajson_t *j);

/* null, bool_false (to not conflict with keyword false), and zero are all
   grouped together on purpose as they all respresent false like values.
   number, decimal, and bool_true are grouped because they all respresent
   true like values.  The string and binary type may also represent true and
   false values (if one were considering t=true, f=false for example). */
typedef enum {
  error = 0,
  object = 1,
  array = 2,
  binary = 3,
  null = 4,
  string = 5,
  bool_false = 6,
  zero = 7,
  number = 8,
  decimal = 9,
  bool_true = 10
} ajson_type_t;

/* Depending upon the task, it may be handy to know what type the json is.
   Internally, all json is stored as strings and converted on demand. */
static inline ajson_type_t ajson_type(ajson_t *j);

/* Dump the json to a file or to a buffer */
void ajson_dump(FILE *out, ajson_t *a);
void ajson_dump_to_buffer(aml_buffer_t *bh, ajson_t *a);

/* Decode encoded json text */
char *ajson_decode(aml_pool_t *pool, char *s, size_t length);

/* Decode encoded json text and return the length of the decoded string.  This
   allows for binary data to be encoded. */
char *ajson_decode2(size_t *rlen, aml_pool_t *pool, char *s, size_t length);

/* Encode json text */
char *ajson_encode(aml_pool_t *pool, char *s, size_t length);

/* returns NULL if object, array, or error */
static inline char *ajsond(aml_pool_t *pool, ajson_t *j);
static inline char *ajsonv(ajson_t *j);
static inline char *ajsonb(ajson_t *j, size_t *length);

static inline ajson_t *ajsono(aml_pool_t *pool);
static inline ajson_t *ajsona(aml_pool_t *pool);

static inline ajson_t *ajson_binary(aml_pool_t *pool, char *s,
                                        size_t length);
static inline ajson_t *ajson_string(aml_pool_t *pool, const char *s,
                                        size_t length);
static inline ajson_t *ajson_str(aml_pool_t *pool, const char *s);
static inline ajson_t *ajson_encode_string(aml_pool_t *pool,
                                               const char *s, size_t length);
static inline ajson_t *ajson_encode_str(aml_pool_t *pool, const char *s);
static inline ajson_t *ajson_true(aml_pool_t *pool);
static inline ajson_t *ajson_false(aml_pool_t *pool);
static inline ajson_t *ajson_null(aml_pool_t *pool);
static inline ajson_t *ajson_zero(aml_pool_t *pool);

static inline ajson_t *ajson_number(aml_pool_t *pool, ssize_t n);
static inline ajson_t *ajson_number_string(aml_pool_t *pool, char *s);
static inline ajson_t *ajson_decimal_string(aml_pool_t *pool, char *s);

/* Determine if current json object is an object, array, or something else */
static inline bool ajson_is_object(ajson_t *j);
static inline bool ajson_is_array(ajson_t *j);

/* convert from json to raw type */
static inline int ajson_to_int(ajson_t *j, int default_value);
static inline int32_t ajson_to_int32(ajson_t *j, int32_t default_value);
static inline uint32_t ajson_to_uint32(ajson_t *j, uint32_t default_value);
static inline int64_t ajson_to_int64(ajson_t *j, int64_t default_value);
static inline uint64_t ajson_to_uint64(ajson_t *j, uint64_t default_value);
static inline float ajson_to_float(ajson_t *j, float default_value);
static inline double ajson_to_double(ajson_t *j, double default_value);
static inline bool ajson_to_bool(ajson_t *j, bool default_value);
static inline char *ajson_to_str(ajson_t *j, const char *default_value);
static inline char *ajson_to_strd(aml_pool_t *pool, ajson_t *j, const char *default_value);

/* json array functions */
static inline int ajsona_count(ajson_t *j);
static inline ajson_t *ajsona_scan(ajson_t *j, int nth);

static inline ajsona_t *ajsona_first(ajson_t *j);
static inline ajsona_t *ajsona_last(ajson_t *j);
static inline ajsona_t *ajsona_next(ajsona_t *j);
static inline ajsona_t *ajsona_previous(ajsona_t *j);

/* These functions cause an internal direct access table to be created.  This
 * would be more efficient if accessing many different array elements.
 * ajsona_erase and ajsona_append will destroy the direct
 * access table.  Care should be taken when calling append frequently and nth
 * or nth_node. */
static inline ajson_t *ajsona_nth(ajson_t *j, int nth);
static inline ajsona_t *ajsona_nth_node(ajson_t *j, int nth);
static inline void ajsona_erase(ajsona_t *n);
static inline void ajsona_append(ajson_t *j, ajson_t *item);

/* json object functions */
static inline int ajsono_count(ajson_t *j);
static inline ajsono_t *ajsono_first(ajson_t *j);
static inline ajsono_t *ajsono_last(ajson_t *j);
static inline ajsono_t *ajsono_next(ajsono_t *j);
static inline ajsono_t *ajsono_previous(ajsono_t *j);

/* append doesn't lookup key prior to inserting, so it should be used with
 * caution.  It is more efficient because it doesn't need to lookup key or
 * maintain a tree. */
static inline void ajsono_append(ajson_t *j, const char *key,
                                   ajson_t *item, bool copy_key);

static inline ajson_t *ajsono_scan(ajson_t *j, const char *key);
static inline ajson_t *ajsono_scanr(ajson_t *j, const char *key);

static inline int ajsono_scan_int(ajson_t *j, const char *key, int default_value);
static inline int32_t ajsono_scan_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_scan_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t ajsono_scan_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_scan_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float ajsono_scan_float(ajson_t *j, const char *key, float default_value);
static inline double ajsono_scan_double(ajson_t *j, const char *key, double default_value);
static inline bool ajsono_scan_bool(ajson_t *j, const char *key, bool default_value);
static inline char *ajsono_scan_str(ajson_t *j, const char *key, const char *default_value);
static inline char *ajsono_scan_strd(aml_pool_t *pool, ajson_t *j,
                                     const char *key, const char *default_value);

/* use _get if json is meant to be read only and _find if not.
  The ajsono_get method is faster than the ajsono_find
  method as it creates a sorted array vs a red black tree (or map).  The
  find/insert methods are useful if you need to lookup keys and insert.

  ajsono_get/get_node/find will not find items which are appended.
*/
static inline ajson_t *ajsono_get(ajson_t *j, const char *key);

static inline int ajsono_get_int(ajson_t *j, const char *key, int default_value);
static inline int32_t ajsono_get_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_get_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t ajsono_get_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_get_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float ajsono_get_float(ajson_t *j, const char *key, float default_value);
static inline double ajsono_get_double(ajson_t *j, const char *key, double default_value);
static inline bool ajsono_get_bool(ajson_t *j, const char *key, bool default_value);
static inline char *ajsono_get_str(ajson_t *j, const char *key, const char *default_value);
static inline char *ajsono_get_strd(aml_pool_t *pool, ajson_t *j,
                                    const char *key, const char *default_value);
static inline ajsono_t *ajsono_get_node(ajson_t *j, const char *key);

/* in this case, don't use _get (only _find) */
static inline void ajsono_erase(ajsono_t *n);
static inline ajsono_t *ajsono_insert(ajson_t *j, const char *key,
                                          ajson_t *item, bool copy_key);
static inline ajsono_t *ajsono_find_node(ajson_t *j, const char *key);
static inline ajson_t *ajsono_find(ajson_t *j, const char *key);

static inline int ajsono_find_int(ajson_t *j, const char *key, int default_value);
static inline int32_t ajsono_find_int32(ajson_t *j, const char *key, int32_t default_value);
static inline uint32_t ajsono_find_uint32(ajson_t *j, const char *key, uint32_t default_value);
static inline int64_t ajsono_find_int64(ajson_t *j, const char *key, int64_t default_value);
static inline uint64_t ajsono_find_uint64(ajson_t *j, const char *key, uint64_t default_value);
static inline float ajsono_find_float(ajson_t *j, const char *key, float default_value);
static inline double ajsono_find_double(ajson_t *j, const char *key, double default_value);
static inline bool ajsono_find_bool(ajson_t *j, const char *key, bool default_value);
static inline char *ajsono_find_str(ajson_t *j, const char *key, const char *default_value);
static inline char *ajsono_find_strd(aml_pool_t *pool, ajson_t *j,
                                     const char *key, const char *default_value);

/* json path functions */
static inline ajson_t *ajsono_path(aml_pool_t *pool, ajson_t *j, const char *path);
static inline char *ajsono_pathv(aml_pool_t *pool, ajson_t *j, const char *path);
static inline char *ajsono_pathd(aml_pool_t *pool, ajson_t *j, const char *path);

#include "a-json-library/impl/ajson.h"

#ifdef __cplusplus
}
#endif

#endif
