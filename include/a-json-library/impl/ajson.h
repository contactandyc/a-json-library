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

#include "the-macro-library/macro_map.h"
#include "the-macro-library/macro_bsearch.h"
#include "the-macro-library/macro_to.h"

#define AJSON_ERROR 0
#define AJSON_VALID 1
#define AJSON_OBJECT 1
#define AJSON_ARRAY 2
#define AJSON_BINARY 3
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

static inline ajson_t *ajson_binary(aml_pool_t *pool, char *s,
                                        size_t length) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_BINARY;
  j->value = s;
  j->length = length;
  return j;
}

static inline ajson_t *ajson_string(aml_pool_t *pool, const char *s,
                                        size_t length) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)s;
  j->length = length;
  return j;
}

static inline ajson_t *ajson_str(aml_pool_t *pool, const char *s) {
  if (!s)
    return NULL;
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)s;
  j->length = strlen(s);
  return j;
}

static inline ajson_t *ajson_encode_string(aml_pool_t *pool, const char *s, size_t length) {
  if (!s)
    return NULL;

  char *v = ajson_encode(pool, (char *)s, length);

  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t));
  j->parent = NULL;
  j->type = AJSON_STRING;
  j->value = (char *)v;
  j->length = strlen(v);
  return j;
}

static inline ajson_t *ajson_encode_str(aml_pool_t *pool, const char *s) {
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

static inline ajson_t *ajson_number(aml_pool_t *pool, ssize_t n) {
  ajson_t *j = (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + 22);
  j->parent = NULL;
  j->value = (char *)(j + 1);
  j->type = AJSON_NUMBER;
  snprintf(j->value, 22, "%ld", n);
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
  memcpy(j->value, s, length + 1);
  j->length = length;
  return j;
}

static inline ajson_t *ajson_decimal_string(aml_pool_t *pool, char *s) {
  size_t length = strlen(s);
  ajson_t *j =
      (ajson_t *)aml_pool_alloc(pool, sizeof(ajson_t) + length + 1);
  j->parent = NULL;
  j->value = (char *)(j + 1);
  j->type = AJSON_DECIMAL;
  memcpy(j->value, s, length + 1);
  j->length = length;
  return j;
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

static inline char *ajsonb(ajson_t *j, size_t *length) {
  if (j && j->type >= AJSON_BINARY) {
    *length = j->length;
    return j->value;
  } else
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
                  ajsono_compare2);

static inline macro_map_insert(__ajson_insert, ajsono_t,
                                  ajsono_insert_compare);

static inline macro_bsearch_first_kv(__ajson_search, char, ajsono_t *,
                                  ajsono_compare);

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

static inline bool ajson_is_error(ajson_t *j) {
  return j->type == AJSON_ERROR;
}

static inline bool ajson_is_object(ajson_t *j) {
  return j->type == AJSON_OBJECT;
}

static inline bool ajson_is_array(ajson_t *j) {
  return j->type == AJSON_ARRAY;
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
  if (nth >= arr->num_entries)
    return NULL;
  if (!arr->array)
    _ajsona_fill(arr);
  return arr->array[nth]->value;
}

static inline ajsona_t *ajsona_nth_node(ajson_t *j, int nth) {
  _ajsona_t *arr = (_ajsona_t *)j;
  if (nth >= arr->num_entries)
    return NULL;
  if (!arr->array)
    _ajsona_fill(arr);
  return arr->array[nth];
}

static inline ajson_t *ajsona_scan(ajson_t *j, int nth) {
  _ajsona_t *arr = (_ajsona_t *)j;
  if (nth >= arr->num_entries)
    return NULL;
  if ((nth << 1) > arr->num_entries) {
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
  if (!item)
    return;

  _ajsona_t *arr = (_ajsona_t *)j;
  ajsona_t *n = (ajsona_t *)aml_pool_alloc(arr->pool, sizeof(*n));
  item->parent = j;
  n->value = item;
  n->next = NULL;
  arr->num_entries++;
  arr->array = NULL;
  if (arr->head) {
    n->previous = arr->tail;
    arr->tail->next = n;
    arr->tail = n;
  } else
    arr->head = arr->tail = n;
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
  if (n->previous) {
    n->previous->next = n->next;
    if (n->next) {
      n->next->previous = n->previous;
      arr->array = NULL;
    } else
      arr->tail = n->previous;
  } else {
    arr->head = n->next;
    if (n->next) {
      n->next->previous = NULL;
      if (arr->array)
        arr->array++;
    } else {
      arr->head = arr->tail = NULL;
      arr->array = NULL;
    }
  }
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
  return *res;
}

static inline ajson_t *ajsono_get(ajson_t *j, const char *key) {
  _ajsono_t *o = (_ajsono_t *)j;
  if (!o->root) {
    if (o->head)
      _ajsono_fill(o);
    else
      return NULL;
  }
  ajsono_t **res = __ajson_search(key, (const ajsono_t **)o->root,
                                      o->num_sorted_entries);
  if (res) {
    ajsono_t *r = *res;
    if (r)
      return r->value;
  }
  return NULL;
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
        if(sscanf(paths[i], "%lu", &num) != 1)
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
