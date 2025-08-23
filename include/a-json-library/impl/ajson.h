// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include "the-macro-library/macro_map.h"
#include "the-macro-library/macro_bsearch.h"
#include "the-macro-library/macro_to.h"

#define AJSON_ERROR 0
#define AJSON_VALID 1
#define AJSON_OBJECT 1
#define AJSON_ARRAY 2
#define AJSON_NULL 4
#define AJSON_STRING 5
#define AJSON_FALSE 6
#define AJSON_ZERO 7
#define AJSON_NUMBER 8
#define AJSON_DECIMAL 9
#define AJSON_TRUE 10

struct ajson_s {
  uint32_t type;
  uint32_t length;
  ajson_t *parent;
  char *value;
};

static inline ajson_type_t ajson_type(ajson_t *j) {
  return (ajson_type_t)(j->type);
}

struct ajsona_s {
  ajson_t *value;
  ajsona_t *next;
  ajsona_t *previous;
};

struct ajsono_s {
  macro_map_t map;
  char *key;
  ajson_t *value;
  ajsono_t *next;
  ajsono_t *previous;
};

static inline
ajson_t *ajson_parse_string(aml_pool_t *pool, const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *p = aml_pool_dup(pool, s, len+1);
    char *ep = p + len;
    return ajson_parse(pool, p, ep);
}

static inline ajson_t *ajson_string(aml_pool_t *pool, const char *s,
                                        size_t length) {
  return ajson_string_nocopy(pool, (char *)aml_pool_udup(pool, s, length), length);
}

static inline ajson_t *ajson_str(aml_pool_t *pool, const char *s) {
  if (!s)
    return NULL;
  return ajson_str_nocopy(pool, aml_pool_strdup(pool, s));
}

static inline ajson_t *ajson_encode_string(aml_pool_t *pool, const char *s, size_t length) {
  return ajson_encode_string_nocopy(pool, (char *)aml_pool_udup(pool, s, length), length);
}

static inline ajson_t *ajson_encode_str(aml_pool_t *pool, const char *s) {
  if (!s)
    return NULL;

  return ajson_encode_str_nocopy(pool, aml_pool_strdup(pool, s));
}

static inline ajson_t *ajson_string_nocopy(aml_pool_t *pool, const char *s,
                                           size_t length) {
  if(!s)
     return NULL;
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)s;
  j->length = length;
  return j;
}

static inline ajson_t *ajson_str_nocopy(aml_pool_t *pool, const char *s) {
  if (!s)
    return NULL;
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)s;
  j->length = strlen(s);
  return j;
}

static inline ajson_t *ajson_encode_string_nocopy(aml_pool_t *pool, const char *s, size_t length) {
  if (!s)
    return NULL;

  char *v = ajson_encode(pool, (char *)s, length);
  size_t vlen = (v == s) ? length : strlen(v);

  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = v;
  j->length = (uint32_t)vlen;
  return j;
}

static inline ajson_t *ajson_encode_str_nocopy(aml_pool_t *pool, const char *s) {
  if (!s)
    return NULL;

  char *v = ajson_encode(pool, (char *)s, strlen(s));

  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)v;
  j->length = strlen(v);
  return j;
}


static inline ajson_t *ajson_true(aml_pool_t *pool) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_TRUE;
  j->value = (char *)"true";
  j->length = 4;
  return j;
}

static inline ajson_t *ajson_false(aml_pool_t *pool) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_FALSE;
  j->value = (char *)"false";
  j->length = 5;
  return j;
}

static inline ajson_t *ajson_bool(aml_pool_t *pool, bool v) {
  if(v)
    return ajson_true(pool);
  else
    return ajson_false(pool);
}


static inline ajson_t *ajson_null(aml_pool_t *pool) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_NULL;
  j->value = (char *)"null";
  j->length = 4;
  return j;
}

static inline ajson_t *ajson_zero(aml_pool_t *pool) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_ZERO;
  j->value = (char *)"0";
  j->length = 1;
  return j;
}

static inline ajson_t *ajson_uint64(aml_pool_t *pool, uint64_t n) {
    ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + 22);
    j->parent = NULL;
    j->value = (char *)(j + 1);
    j->type = AJSON_NUMBER;
    snprintf(j->value, 22, "%" PRIu64, n);
    j->length = strlen(j->value);
    return j;
}

static inline ajson_t *ajson_number(aml_pool_t *pool, ssize_t n) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + 22);
  j->parent = NULL;
  j->value = (char *)(j + 1);
  j->type = AJSON_NUMBER;
  snprintf(j->value, 22, "%zd", n);
  j->length = strlen(j->value);
  return j;
}

static inline ajson_t *ajson_number_string(aml_pool_t *pool, char *s) {
  size_t length = strlen(s);
  ajson_t *j =
      (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + length + 1);
  j->parent = NULL;
  j->value = (char *)(j + 1);
  j->type = AJSON_NUMBER;
  memcpy(j->value, s, length);
  j->value[length] = 0;
  j->length = length;
  return j;
}

static inline ajson_t *ajson_number_stringf(aml_pool_t *pool, char *s, ...) {
  va_list ap;
  va_start(ap, s);
  char *buffer = aml_pool_strdupvf(pool, s, ap);
  va_end(ap);
  return ajson_number_string(pool, buffer);
}

static inline ajson_t *ajson_decimal_string(aml_pool_t *pool, char *s) {
  size_t length = strlen(s);
  ajson_t *j =
      (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + length + 1);
  j->parent = NULL;
  j->value = (char *)(j + 1);
  j->type = AJSON_DECIMAL;
  memcpy(j->value, s, length);
  j->value[length] = 0;
  j->length = length;
  return j;
}

static inline ajson_t *ajson_decimal_stringf(aml_pool_t *pool, char *s, ...) {
  va_list ap;
  va_start(ap, s);
  char *buffer = aml_pool_strdupvf(pool, s, ap);
  va_end(ap);
  return ajson_decimal_string(pool, buffer);
}

static inline char *ajsond(aml_pool_t *pool, ajson_t *j) {
  if (!j)
    return NULL;
  else if (j->type == AJSON_STRING)
    return ajson_decode(pool, j->value, j->length);
  else if (j->type > AJSON_STRING)
    return j->value;
  else
    return NULL;
}

static inline char *ajsonv(ajson_t *j) {
  if (j && j->type >= AJSON_STRING)
    return j->value;
  else
    return NULL;
}

static inline int ajsono_compare(const char *key, const ajsono_t **o) {
  return strcmp(key, (*o)->key);
}

static inline int ajsono_compare2(const char *key, const ajsono_t *o) {
  return strcmp(key, o->key);
}

static inline int ajsono_insert_compare(const ajsono_t *a,
                                          const ajsono_t *b) {
  return strcmp(a->key, b->key);
}

static inline
macro_map_find_kv(__ajson_find, char, ajsono_t,
                  ajsono_compare2)

static inline macro_map_insert(__ajson_insert, ajsono_t,
                                  ajsono_insert_compare)

static inline macro_bsearch_first_kv(__ajson_search, char, ajsono_t *,
                                  ajsono_compare)

struct ajson_error_s;
typedef struct ajson_error_s ajson_error_t;

struct _ajsono_s;
typedef struct _ajsono_s _ajsono_t;

struct ajson_error_s {
  uint32_t type;
  uint16_t line; // for debug lines see AJSON_DEBUG block (defaults to zero)
  uint16_t line2;
  char *error_at;
  char *source;
  aml_pool_t *pool;
};

/* Predicates: all return false if j == NULL. */
static inline bool ajson_is_error(ajson_t *j) {
  return j && j->type == AJSON_ERROR;
}

static inline bool ajson_is_object(ajson_t *j) {
  return j && j->type == AJSON_OBJECT;
}

static inline bool ajson_is_array(ajson_t *j) {
  return j && j->type == AJSON_ARRAY;
}

static inline bool ajson_is_null(ajson_t *j) {
  return j && j->type == AJSON_NULL;
}

static inline bool ajson_is_bool(ajson_t *j) {
  return j && (j->type == AJSON_TRUE || j->type == AJSON_FALSE);
}

static inline bool ajson_is_string(ajson_t *j) {
  return j && j->type == AJSON_STRING;
}

/* NUMBER-like = ZERO | NUMBER | DECIMAL */
static inline bool ajson_is_number(ajson_t *j) {
  return j && (j->type == AJSON_ZERO ||
               j->type == AJSON_NUMBER ||
               j->type == AJSON_DECIMAL);
}

struct _ajsono_s {
  uint32_t type;
  uint32_t num_entries;
  ajson_t *parent;
  macro_map_t *root;
  size_t num_sorted_entries;
  ajsono_t *head;
  ajsono_t *tail;
  aml_pool_t *pool;
};

typedef struct {
  uint32_t type;
  uint32_t num_entries;
  ajson_t *parent;
  ajsona_t **array;
  ajsona_t *head;
  ajsona_t *tail;
  aml_pool_t *pool;
} _ajsona_t;

static inline ajson_t *ajsono(aml_pool_t *pool) {
  _ajsono_t *obj = (_ajsono_t *)aml_pool_zalloc(pool, sizeof(_ajsono_t));
  obj->type = AJSON_OBJECT;
  obj->pool = pool;
  return (ajson_t *)obj;
}

static inline ajson_t *ajsona(aml_pool_t *pool) {
  _ajsona_t *a = (_ajsona_t *)aml_pool_zalloc(pool, sizeof(_ajsona_t));
  a->type = AJSON_ARRAY;
  a->pool = pool;
  return (ajson_t *)a;
}

static inline void _ajsona_fill(_ajsona_t *arr) {
  arr->array = (ajsona_t **)aml_pool_alloc(arr->pool, sizeof(ajsona_t *) *
                                                           arr->num_entries);
  ajsona_t **awp = arr->array;
  ajsona_t *n = arr->head;
  while (n) {
    *awp++ = n;
    n = n->next;
  }
  arr->num_entries = awp - arr->array;
}

static inline ajson_t *ajsona_nth(ajson_t *j, int nth) {
  _ajsona_t *arr = (_ajsona_t *)j;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries) return NULL;
  if (!arr->array)
    _ajsona_fill(arr);
  return arr->array[nth]->value;
}

static inline ajsona_t *ajsona_nth_node(ajson_t *j, int nth) {
  _ajsona_t *arr = (_ajsona_t *)j;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries)
    return NULL;
  if (!arr->array)
    _ajsona_fill(arr);
  return arr->array[nth];
}

static inline ajson_t *ajsona_scan(ajson_t *j, int nth) {
  _ajsona_t *arr = (_ajsona_t *)j;
  if (nth < 0 || (uint32_t)nth >= arr->num_entries)
    return NULL;
  if ((uint32_t)nth > (arr->num_entries >> 1)) {
    nth = arr->num_entries - nth;
    nth--;
    ajsona_t *n = arr->tail;
    while (nth) {
      nth--;
      n = n->previous;
    }
    return n->value;
  } else {
    ajsona_t *n = arr->head;
    while (nth) {
      nth--;
      n = n->next;
    }
    return n->value;
  }
}

static inline void ajsona_append(ajson_t *j, ajson_t *item) {
    // Check that both the container and the item exist.
    if (!j || !item)
        return;

    // Ensure that j is really an array.
    if (j->type != AJSON_ARRAY)
        return; // or handle error appropriately

    // Cast the container to the internal array type.
    _ajsona_t *arr = (_ajsona_t *)j;

    // Invalidate any cached direct–access array (to be rebuilt on demand).
    arr->array = NULL;

    // Allocate a new array node for this item.
    ajsona_t *node = (ajsona_t *)aml_pool_zalloc(arr->pool, sizeof(*node));
    if (!node)
        return; // out-of-memory: handle error as needed

    // Initialize the new node.
    node->value = item;
    node->next = NULL;
    node->previous = NULL;

    // Set the item's parent pointer to this array.
    item->parent = j;

    // Append the node to the end of the linked list.
    if (arr->tail) {
        // There are already nodes in the array; link the new node after the tail.
        node->previous = arr->tail;
        arr->tail->next = node;
        arr->tail = node;
    } else {
        // First element: set both head and tail to the new node.
        arr->head = node;
        arr->tail = node;
    }

    // Increase the count of elements.
    arr->num_entries++;
}

static inline int ajsona_count(ajson_t *j) {
  if(!j) return 0;
  _ajsona_t *arr = (_ajsona_t *)j;
  return arr->num_entries;
}

static inline ajsona_t *ajsona_first(ajson_t *j) {
  if(!j) return NULL;
  _ajsona_t *arr = (_ajsona_t *)j;
  return arr->head;
}

static inline ajsona_t *ajsona_last(ajson_t *j) {
  if(!j) return NULL;
  _ajsona_t *arr = (_ajsona_t *)j;
  return arr->tail;
}

static inline ajsona_t *ajsona_next(ajsona_t *j) { return j->next; }

static inline ajsona_t *ajsona_previous(ajsona_t *j) {
  return j->previous;
}

static inline void ajsona_erase(ajsona_t *n) {
  _ajsona_t *arr = (_ajsona_t *)(n->value->parent);
  arr->num_entries--;
  /* unlink from list */
  if (n->previous)
    n->previous->next = n->next;
  else
    arr->head = n->next;
  if (n->next)
    n->next->previous = n->previous;
  else
    arr->tail = n->previous;

  /* always invalidate direct-access cache */
  arr->array = NULL;

  /* optional: orphan node pointers (tidy) */
  n->next = n->previous = NULL;
  if (n->value)
    n->value->parent = NULL;
}

static inline int ajsono_count(ajson_t *j) {
  if(!j)
    return 0;
  _ajsono_t *o = (_ajsono_t *)j;
  return o->num_entries;
}

static inline ajsono_t *ajsono_first(ajson_t *j) {
  if(!j)
    return NULL;
  _ajsono_t *o = (_ajsono_t *)j;
  return o->head;
}

static inline ajsono_t *ajsono_last(ajson_t *j) {
  if(!j)
    return NULL;
  _ajsono_t *o = (_ajsono_t *)j;
  return o->tail;
}

static inline ajsono_t *ajsono_next(ajsono_t *j) { return j->next; }

static inline ajsono_t *ajsono_previous(ajsono_t *j) {
  return j->previous;
}

static inline void ajsono_erase(ajsono_t *n) {
  _ajsono_t *o = (_ajsono_t *)(n->value->parent);
  o->num_entries--;
  if (o->root) {
    if (o->num_sorted_entries) {
      o->root = NULL;
      o->num_sorted_entries = 0;
    } else
      macro_map_erase(&o->root, (macro_map_t *)n);
  }
  if (n->previous) {
    n->previous->next = n->next;
    if (n->next) {
      n->next->previous = n->previous;
    } else
      o->tail = n->previous;
  } else {
    o->head = n->next;
    if (n->next)
      n->next->previous = NULL;
    else
      o->head = o->tail = NULL;
  }
}

void _ajsono_fill(_ajsono_t *o);

static inline ajsono_t *ajsono_get_node(ajson_t *j, const char *key) {
  _ajsono_t *o = (_ajsono_t *)j;
  if (!o->root) {
    if (o->head)
      _ajsono_fill(o);
    else
      return NULL;
  }
  ajsono_t **res = __ajson_search((char *)key, (const ajsono_t **)o->root,
                                      o->num_sorted_entries);
  return res ? *res : NULL;
}

static inline ajson_t *ajsono_get(ajson_t *j, const char *key) {
  _ajsono_t *o = (_ajsono_t *)j;
  if (!o->root || o->num_sorted_entries == 0) {   // <-- note the == 0 guard
    if (o->head) _ajsono_fill(o);
    else return NULL;
  }
  ajsono_t **res = __ajson_search(key, (const ajsono_t **)o->root, o->num_sorted_entries);
  return res ? (*res ? (*res)->value : NULL) : NULL;
}

static inline ajson_t *ajsono_scanr(ajson_t *j, const char *key) {
  if (!j || j->type != AJSON_OBJECT)
    return NULL;
  _ajsono_t *o = (_ajsono_t *)j;
  ajsono_t *r = o->tail;
  while (r) {
    if (!strcmp(r->key, key))
      return r->value;
    r = r->previous;
  }
  return NULL;
}

static inline ajson_t *ajsono_scan(ajson_t *j, const char *key) {
  if (!j || j->type != AJSON_OBJECT)
    return NULL;
  _ajsono_t *o = (_ajsono_t *)j;
  ajsono_t *r = o->head;
  while (r) {
    if (!strcmp(r->key, key))
      return r->value;
    r = r->next;
  }
  return NULL;
}

static inline int ajson_to_int(ajson_t *j, int default_value) {
    return macro_to_int(ajsonv(j), default_value);
}

static inline int32_t ajson_to_int32(ajson_t *j, int32_t default_value) {
    return macro_to_int32(ajsonv(j), default_value);
}

static inline uint32_t ajson_to_uint32(ajson_t *j, uint32_t default_value) {
    return macro_to_uint32(ajsonv(j), default_value);
}

static inline int64_t ajson_to_int64(ajson_t *j, int64_t default_value) {
    return macro_to_int64(ajsonv(j), default_value);
}

static inline uint64_t ajson_to_uint64(ajson_t *j, uint64_t default_value) {
    return macro_to_uint64(ajsonv(j), default_value);
}

static inline float ajson_to_float(ajson_t *j, float default_value) {
    return macro_to_float(ajsonv(j), default_value);
}

static inline double ajson_to_double(ajson_t *j, double default_value) {
    return macro_to_double(ajsonv(j), default_value);
}

static inline bool ajson_to_bool(ajson_t *j, bool default_value) {
    return macro_to_bool(ajsonv(j), default_value);
}

static inline char *ajson_to_str(ajson_t *j, const char *default_value) {
    char *value = ajsonv(j);
    return value ? value : (char *)default_value;
}

static inline char *ajson_to_strd(aml_pool_t *pool, ajson_t *j, const char *default_value) {
    char *value = ajsond(pool, j);
    return value ? value : (char *)default_value;
}


/* Try (no defaults): forward to macro_try_to_* on the node's string view. */
static inline bool ajson_try_to_int    (ajson_t *j, int      *out) { return macro_try_to_int    (ajsonv(j), out); }
static inline bool ajson_try_to_long   (ajson_t *j, long     *out) { return macro_try_to_long   (ajsonv(j), out); }
static inline bool ajson_try_to_int32  (ajson_t *j, int32_t  *out) { return macro_try_to_int32  (ajsonv(j), out); }
static inline bool ajson_try_to_uint32 (ajson_t *j, uint32_t *out) { return macro_try_to_uint32 (ajsonv(j), out); }
static inline bool ajson_try_to_int64  (ajson_t *j, int64_t  *out) { return macro_try_to_int64  (ajsonv(j), out); }
static inline bool ajson_try_to_uint64 (ajson_t *j, uint64_t *out) { return macro_try_to_uint64 (ajsonv(j), out); }
static inline bool ajson_try_to_float  (ajson_t *j, float    *out) { return macro_try_to_float  (ajsonv(j), out); }
static inline bool ajson_try_to_double (ajson_t *j, double   *out) { return macro_try_to_double (ajsonv(j), out); }
static inline bool ajson_try_to_bool   (ajson_t *j, bool     *out) { return macro_try_to_bool   (ajsonv(j), out); }


/* ===========================
 * Object helpers: try variants
 * =========================== */

/* --- Node-level try: returns true and sets *out if key found; else false --- */
static inline bool ajsono_scan_try(ajson_t *j, const char *key, ajson_t **out) {
  if (!out) return false;
  ajson_t *n = ajsono_scan(j, key);
  if (!n) return false;
  *out = n; return true;
}

static inline bool ajsono_get_try(ajson_t *j, const char *key, ajson_t **out) {
  if (!out) return false;
  ajson_t *n = ajsono_get(j, key);
  if (!n) return false;
  *out = n; return true;
}

static inline bool ajsono_find_try(ajson_t *j, const char *key, ajson_t **out) {
  if (!out) return false;
  ajson_t *n = ajsono_find(j, key);
  if (!n) return false;
  *out = n; return true;
}

#define AJSONO_TRY_TYPED(API, CTYPE, CONVFN)                                        \
static inline bool ajsono_##API##_try_##CONVFN(ajson_t *j, const char *key,         \
                                               CTYPE *out) {   \
  if (!out) return false;                                                           \
  ajson_t *n = ajsono_##API(j, key);                                                \
  if (!n) { return false; }                                   \
  return ajson_try_to_##CONVFN(n, out); \
}

/* int, int32, uint32, int64, uint64, float, double, bool */
AJSONO_TRY_TYPED(scan,   int,     int)
AJSONO_TRY_TYPED(get,    int,     int)
AJSONO_TRY_TYPED(find,   int,     int)

AJSONO_TRY_TYPED(scan,   int32_t, int32)
AJSONO_TRY_TYPED(get,    int32_t, int32)
AJSONO_TRY_TYPED(find,   int32_t, int32)

AJSONO_TRY_TYPED(scan,   uint32_t, uint32)
AJSONO_TRY_TYPED(get,    uint32_t, uint32)
AJSONO_TRY_TYPED(find,   uint32_t, uint32)

AJSONO_TRY_TYPED(scan,   int64_t, int64)
AJSONO_TRY_TYPED(get,    int64_t, int64)
AJSONO_TRY_TYPED(find,   int64_t, int64)

AJSONO_TRY_TYPED(scan,   uint64_t, uint64)
AJSONO_TRY_TYPED(get,    uint64_t, uint64)
AJSONO_TRY_TYPED(find,   uint64_t, uint64)

AJSONO_TRY_TYPED(scan,   float,   float)
AJSONO_TRY_TYPED(get,    float,   float)
AJSONO_TRY_TYPED(find,   float,   float)

AJSONO_TRY_TYPED(scan,   double,  double)
AJSONO_TRY_TYPED(get,    double,  double)
AJSONO_TRY_TYPED(find,   double,  double)

AJSONO_TRY_TYPED(scan,   bool,    bool)
AJSONO_TRY_TYPED(get,    bool,    bool)
AJSONO_TRY_TYPED(find,   bool,    bool)

/* Strings:
 *  - *_try_str  : returns encoded internal view (ajsonv), default if missing
 *  - *_try_strd : returns decoded copy in pool (ajsond), default if missing
 * Both return true if the key existed (even if it wasn’t a string node).
 */
#define AJSONO_TRY_STR_COMMON(API)                                                           \
static inline bool ajsono_##API##_try_str(ajson_t *j, const char *key,                       \
                                          const char *default_value, char **out) {           \
  if (!out) return false;                                                                    \
  ajson_t *n = ajsono_##API(j, key);                                                         \
  if (!n) { *out = (char *)default_value; return false; }                                    \
  *out = ajson_to_str(n, default_value);                                                     \
  return true;                                                                               \
}                                                                                            \
static inline bool ajsono_##API##_try_strd(aml_pool_t *pool, ajson_t *j, const char *key,    \
                                           const char *default_value, char **out) {          \
  if (!out) return false;                                                                    \
  ajson_t *n = ajsono_##API(j, key);                                                         \
  if (!n) { *out = (char *)default_value; return false; }                                    \
  *out = ajson_to_strd(pool, n, default_value);                                              \
  return true;                                                                               \
}

AJSONO_TRY_STR_COMMON(scan)
AJSONO_TRY_STR_COMMON(get)
AJSONO_TRY_STR_COMMON(find)

#undef AJSONO_TRY_STR_COMMON
#undef AJSONO_TRY_TYPED


static inline int ajsono_scan_int(ajson_t *j, const char *key, int default_value) {
    return ajson_to_int(ajsono_scan(j, key), default_value);
}

static inline int32_t ajsono_scan_int32(ajson_t *j, const char *key, int32_t default_value) {
    return ajson_to_int32(ajsono_scan(j, key), default_value);
}

static inline uint32_t ajsono_scan_uint32(ajson_t *j, const char *key, uint32_t default_value) {
    return ajson_to_uint32(ajsono_scan(j, key), default_value);
}

static inline int64_t ajsono_scan_int64(ajson_t *j, const char *key, int64_t default_value) {
    return ajson_to_int64(ajsono_scan(j, key), default_value);
}

static inline uint64_t ajsono_scan_uint64(ajson_t *j, const char *key, uint64_t default_value) {
    return ajson_to_uint64(ajsono_scan(j, key), default_value);
}

static inline float ajsono_scan_float(ajson_t *j, const char *key, float default_value) {
    return ajson_to_float(ajsono_scan(j, key), default_value);
}

static inline double ajsono_scan_double(ajson_t *j, const char *key, double default_value) {
    return ajson_to_double(ajsono_scan(j, key), default_value);
}

static inline bool ajsono_scan_bool(ajson_t *j, const char *key, bool default_value) {
    return ajson_to_bool(ajsono_scan(j, key), default_value);
}

static inline char *ajsono_scan_str(ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_str(ajsono_scan(j, key), default_value);
}

static inline char *ajsono_scan_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_strd(pool, ajsono_scan(j, key), default_value);
}

static inline int ajsono_get_int(ajson_t *j, const char *key, int default_value) {
    return ajson_to_int(ajsono_get(j, key), default_value);
}

static inline int32_t ajsono_get_int32(ajson_t *j, const char *key, int32_t default_value) {
    return ajson_to_int32(ajsono_get(j, key), default_value);
}

static inline uint32_t ajsono_get_uint32(ajson_t *j, const char *key, uint32_t default_value) {
    return ajson_to_uint32(ajsono_get(j, key), default_value);
}

static inline int64_t ajsono_get_int64(ajson_t *j, const char *key, int64_t default_value) {
    return ajson_to_int64(ajsono_get(j, key), default_value);
}

static inline uint64_t ajsono_get_uint64(ajson_t *j, const char *key, uint64_t default_value) {
    return ajson_to_uint64(ajsono_get(j, key), default_value);
}

static inline float ajsono_get_float(ajson_t *j, const char *key, float default_value) {
    return ajson_to_float(ajsono_get(j, key), default_value);
}

static inline double ajsono_get_double(ajson_t *j, const char *key, double default_value) {
    return ajson_to_double(ajsono_get(j, key), default_value);
}

static inline bool ajsono_get_bool(ajson_t *j, const char *key, bool default_value) {
    return ajson_to_bool(ajsono_get(j, key), default_value);
}

static inline char *ajsono_get_str(ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_str(ajsono_get(j, key), default_value);
}

static inline char *ajsono_get_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_strd(pool, ajsono_get(j, key), default_value);
}

static inline int ajsono_find_int(ajson_t *j, const char *key, int default_value) {
    return ajson_to_int(ajsono_find(j, key), default_value);
}

static inline int32_t ajsono_find_int32(ajson_t *j, const char *key, int32_t default_value) {
    return ajson_to_int32(ajsono_find(j, key), default_value);
}

static inline uint32_t ajsono_find_uint32(ajson_t *j, const char *key, uint32_t default_value) {
    return ajson_to_uint32(ajsono_find(j, key), default_value);
}

static inline int64_t ajsono_find_int64(ajson_t *j, const char *key, int64_t default_value) {
    return ajson_to_int64(ajsono_find(j, key), default_value);
}

static inline uint64_t ajsono_find_uint64(ajson_t *j, const char *key, uint64_t default_value) {
    return ajson_to_uint64(ajsono_find(j, key), default_value);
}

static inline float ajsono_find_float(ajson_t *j, const char *key, float default_value) {
    return ajson_to_float(ajsono_find(j, key), default_value);
}

static inline double ajsono_find_double(ajson_t *j, const char *key, double default_value) {
    return ajson_to_double(ajsono_find(j, key), default_value);
}

static inline bool ajsono_find_bool(ajson_t *j, const char *key, bool default_value) {
    return ajson_to_bool(ajsono_find(j, key), default_value);
}

static inline char *ajsono_find_str(ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_str(ajsono_find(j, key), default_value);
}

static inline char *ajsono_find_strd(aml_pool_t *pool, ajson_t *j, const char *key, const char *default_value) {
    return ajson_to_strd(pool, ajsono_find(j, key), default_value);
}

static inline void _ajsono_fill_tree(_ajsono_t *o) {
  ajsono_t *r = o->head;
  o->root = NULL;
  o->num_sorted_entries = 0;
  while (r) {
    __ajson_insert(&(o->root), r);
    r = r->next;
  }
}

static inline ajsono_t *ajsono_find_node(ajson_t *j, const char *key) {
  _ajsono_t *o = (_ajsono_t *)j;
  if (!o->root || o->num_sorted_entries) {
    if (o->head)
      _ajsono_fill_tree(o);
    else
      return NULL;
  }
  return __ajson_find(o->root, key);
}

static inline ajson_t *ajsono_find(ajson_t *j, const char *key) {
  ajsono_t *r = ajsono_find_node(j, key);
  if (r)
    return r->value;
  return NULL;
}

static inline ajsono_t *ajsono_insert(ajson_t *j, const char *key,
                                          ajson_t *item, bool copy_key) {
  if (!item)
    return NULL;
  ajsono_t *res = ajsono_find_node(j, key);
  if (res) {
    item->parent = j;
    res->value = item;
  } else {
    ajsono_append(j, key, item, copy_key);
    _ajsono_t *o = (_ajsono_t *)j;
    __ajson_insert(&(o->root), o->tail);
  }
  return res;
}

static inline void ajsono_append(ajson_t *j, const char *key,
                                   ajson_t *item, bool copy_key) {
  if (!item)
    return;

  _ajsono_t *o = (_ajsono_t *)j;
  ajsono_t *on;
  if (copy_key) {
    on = (ajsono_t *)aml_pool_zalloc(o->pool,
                                      sizeof(ajsono_t) + strlen(key) + 1);
    on->key = (char *)(on + 1);
    strcpy(on->key, key);
  } else {
    on = (ajsono_t *)aml_pool_zalloc(o->pool, sizeof(ajsono_t));
    on->key = (char *)key;
  }
  on->value = item;
  item->parent = j;

  o->num_entries++;
  if (!o->head)
    o->head = o->tail = on;
  else {
    on->previous = o->tail;
    o->tail->next = on;
    o->tail = on;
  }
}

static inline ajson_t *ajsono_path(aml_pool_t *pool, ajson_t *j, const char *path) {
  size_t num_paths = 0;
  char **paths = aml_pool_split_with_escape2(pool, &num_paths, '.', '\\', path);
  for( size_t i=0; i<num_paths; i++ ) {
    if(ajson_is_array(j)) {
      char *value = strchr(paths[i], '=');
      if(value) {
        *value = 0;
        value++;
        ajson_t *next = NULL;
        ajsona_t *iter = ajsona_first(j);
        while(iter) {
          char *v = ajsonv(ajsono_scan(iter->value, paths[i]));
          if(v && !strcmp(v, value)) {
            next = iter->value;
            break;
          }
          iter = ajsona_next(iter);
        }
        j = next;
      }
      else {
        size_t num = 0;
        if(sscanf(paths[i], "%zu", &num) != 1)
          return NULL;
        j = ajsona_scan(j, num);
      }
    }
    else
      j = ajsono_scan(j, paths[i]);

    if(!j)
      return NULL;
  }
  return j;
}

static inline char *ajsono_pathv(aml_pool_t *pool, ajson_t *j, const char *path) {
  j = ajsono_path(pool, j, path);
  return ajsonv(j);
}

static inline char *ajsono_pathd(aml_pool_t *pool, ajson_t *j, const char *path) {
  j = ajsono_path(pool, j, path);
  return ajsond(pool, j);
}

static inline char *ajson_extract_string(aml_pool_t *pool, ajson_t *node) {
    char *response = ajson_to_strd(pool, node, NULL);
    return response ? response : (char *)"";
}

static inline int ajson_extract_int(ajson_t *node) {
    return ajson_to_int(node, 0);
}

static inline bool ajson_extract_bool(ajson_t *node) {
    return ajson_to_bool(node, false);
}

static inline uint32_t ajson_extract_uint32(ajson_t *node) {
    return ajson_to_uint32(node, 0);
}

static inline char **ajson_extract_string_array(size_t *count, aml_pool_t *pool, ajson_t *node) {
    if(!node) {
        *count = 0;
        return NULL;
    }
    if (!ajson_is_array(node)) {
        char **result = (char **)aml_pool_alloc(pool, 2 * sizeof(char *));
        result[0] = ajson_extract_string(pool, node);
        result[1] = NULL;
        *count = 1;
        return result;
    }

    size_t n = ajsona_count(node);
    char **result = (char **)aml_pool_alloc(pool, (n+1) * sizeof(char *));
    for (size_t i = 0; i < n; ++i) {
        result[i] = ajson_extract_string(pool, ajsona_nth(node, i));
    }
    *count = n;
    result[n] = NULL;
    return result;
}

static inline
float *ajson_extract_float_array(size_t *num, aml_pool_t *pool, ajson_t *node) {
    if(!node || !ajson_is_array(node)) {
        *num = 0;
        return NULL;
    }
    size_t num_f = ajsona_count(node);
    if(!num_f) {
        *num = 0;
        return NULL;
    }
    float *f = aml_pool_alloc(pool, sizeof(float) * num_f);
    ajsona_t *ev = ajsona_first(node);
    float *fp = f;
    while(ev) {
        *fp++ = ajson_to_float(ev->value, 0.0);
        ev = ajsona_next(ev);
    }
    *num = num_f;
    return f;
}


/* Replace first matching key (linear scan, like ajsono_scan).
   If missing, append. Keeps insertion order. Updates indexes:
   - if RB-tree exists -> insert new node
   - if sorted-array index exists -> invalidate so it rebuilds on demand */
static inline ajsono_t *ajsono_set(ajson_t *j, const char *key,
                                   ajson_t *item, bool copy_key) {
  if (!j || j->type != AJSON_OBJECT || !key || !item) return NULL;

  _ajsono_t *o = (_ajsono_t *)j;

  // Linear scan for existing key (same behavior as ajsono_scan)
  for (ajsono_t *n = o->head; n; n = n->next) {
    if (!strcmp(n->key, key)) {
      n->value = item;
      item->parent = j;
      return n;
    }
  }

  // Not found -> append
  ajsono_append(j, key, item, copy_key);

  // Maintain lookup structures
  if (o->root) {
    if (o->num_sorted_entries) {
      // Invalidate sorted array so ajsono_get will rebuild
      o->root = NULL;
      o->num_sorted_entries = 0;
    } else {
      // RB-tree present: insert new tail node
      __ajson_insert(&(o->root), o->tail);
    }
  }

  return o->tail;
}

static inline bool ajsono_remove(ajson_t *j, const char *key) {
  if (!j || j->type != AJSON_OBJECT || !key) return false;

  _ajsono_t *o = (_ajsono_t *)j;
  for (ajsono_t *n = o->head; n; n = n->next) {
    if (!strcmp(n->key, key)) {
      ajsono_erase(n);         // updates num_entries, links, and lookup indexes
      return true;
    }
  }
  return false; // key not found
}

/* Clear an array: unlink everything and drop direct-access cache.
   (Pool owns memory; nodes/items are not freed.) */
static inline void ajsona_clear(ajson_t *j) {
  if (!j || j->type != AJSON_ARRAY) return;

  _ajsona_t *arr = (_ajsona_t *)j;

  // Orphan existing nodes/items (optional but tidy)
  for (ajsona_t *n = arr->head; n; ) {
    ajsona_t *next = n->next;
    if (n->value) n->value->parent = NULL;
    n->next = n->previous = NULL;
    n = next;
  }

  arr->head = arr->tail = NULL;
  arr->num_entries = 0;
  arr->array = NULL; // invalidate direct-access table
}
