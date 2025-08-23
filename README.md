# Overview of A JSON Library

AJSON is a very efficient JSON processing library written in C, providing robust functions for parsing, error handling, and data conversion of JSON structures. Tailored for flexibility and efficiency, AJSON enables easy manipulation of JSON objects and arrays, along with seamless conversion to various data types. The library's integration with memory and buffer management from AML further enhances its capability to handle complex JSON operations with minimized memory overhead.

## Dependencies

* [A cmake library](https://github.com/knode-ai-open-source/a-cmake-library) needed for the cmake build
* [A memory library](https://github.com/knode-ai-open-source/a-memory-library) for the memory handling
* [The macro library](https://github.com/knode-ai-open-source/the-macro-library) for sorting, searching, and conversions

## Installation

### Clone the library and change to the directory

```bash
git clone https://github.com/knode-ai-open-source/a-json-library.git
cd a-json-library
```

### Build and install library

```bash
mkdir -p build
cd build
cmake ..
make
make install
```

## An Example

```c
#include <stdio.h>
#include "a-json-library/ajson.h"
#include "a-memory-library/aml_pool.h"

int main() {
    // Sample JSON string
    const char* json_str = "{\"name\": \"John Doe\", \"age\": 30, \"is_student\": false}";

    // Create a memory pool for efficient memory management
    aml_pool_t *pool = aml_pool_init(1024);

    // the ajson_parse is destructive to the input string
    char *str = aml_pool_strdup(pool, json_str);

    // Parse the JSON string
    ajson_t *json = ajson_parse(pool, str, str+strlen(str));

    // Check for parsing errors
    if (ajson_is_error(json)) {
        fprintf(stderr, "Error parsing JSON:\n");
        ajson_dump_error(stderr, json);
        aml_pool_destroy(pool);
        return 1;
    }

    // Access elements in the JSON object
    const char* name = ajsono_scan_str(json, "name", "unknown");
    int age = ajsono_scan_int(json, "age", -1);
    bool is_student = ajsono_scan_bool(json, "is_student", false);

    // Print the values
    printf("Name: %s\n", name);
    printf("Age: %d\n", age);
    printf("Is Student: %s\n", is_student ? "true" : "false");

    // Clean up
    aml_pool_destroy(pool);
    return 0;
}
```

## Core Functions

- **ajson_parse**: Parses JSON text into ajson structures.
- **ajson_is_error**: Checks if the parsed JSON is marked as an error.
- **ajson_dump_error**: Outputs JSON parsing errors to a file.
- **ajson_dump_error_to_buffer**: Writes JSON parsing errors to a buffer.

## Type Handling

- **ajson_type**: Determines the type of a JSON object.

## Output Functions

- **ajson_dump**: Outputs the JSON structure to a file.
- **ajson_dump_to_buffer**: Writes the JSON structure to a buffer.

## Encoding and Decoding

- **ajson_decode**: Decodes encoded JSON text.
- **ajson_decode2**: Decodes encoded JSON text with binary data support.
- **ajson_encode**: Encodes JSON text.

## JSON Object Functions

- **ajsono**: Creates a new JSON object.
- **ajsona**: Creates a new JSON array.
- **ajson_encode_str**: Creates a JSON encoded string from a C string.
- **ajson_encode_string**: Creates a JSON encoded string from a string and a length.
- **ajson_str**: Creates a JSON string from a C string.
- **ajson_string**: Creates a JSON string from a string and a length.
- **ajson_true**: Creates a JSON true value.
- **ajson_false**: Creates a JSON false value.
- **ajson_null**: Creates a JSON null value.
- **ajson_zero**: Creates a JSON zero value.
- **ajson_number**: Creates a JSON number.
- **ajson_number_string**: Creates a JSON number from a string.
- **ajson_decimal_string**: Creates a JSON decimal from a string.
- **ajson_binary**: Creates a JSON binary type.

## JSON Array Functions

- **ajsona_count**: Counts elements in a JSON array.
- **ajsona_scan**: Retrieves an element from a JSON array.

## JSON Object Access

- **ajsono_scan**: Retrieves a JSON object by key.
- **ajsono_scanr**: Retrieves a JSON object by key in reverse.
- **ajsono_get**: Retrieves a JSON object by key, internally sorts nodes in first get call by object.
- **ajsono_insert**: Inserts a JSON object by key.
- **ajsono_find**: Finds a JSON object by key, internally converting nodes of object into a map on first find call.

## JSON Path Functions

- **ajsono_path**: Retrieves a JSON object by JSON path.
- **ajsono_pathv**: Retrieves a string value from a JSON path.
- **ajsono_pathd**: Retrieves a string value from a JSON path with decoding.

## Conversion Functions

- **ajson_to_int**: Converts JSON to an integer.
- **ajson_to_int32**: Converts JSON to a 32-bit integer.
- **ajson_to_uint32**: Converts JSON to an unsigned 32-bit integer.
- **ajson_to_int64**: Converts JSON to a 64-bit integer.
- **ajson_to_uint64**: Converts JSON to an unsigned 64-bit integer.
- **ajson_to_float**: Converts JSON to a float.
- **ajson_to_double**: Converts JSON to a double.
- **ajson_to_bool**: Converts JSON to a boolean.
- **ajson_to_str**: Converts JSON to a string.
- **ajson_to_strd**: Converts JSON to a string with decoding.

## Object Helper Functions

- **ajsono_scan_int**: Retrieves a JSON object by key and converts JSON to an integer.
- **ajsono_scan_int32**: Retrieves a JSON object by key and converts JSON to a 32-bit integer.
- **ajsono_scan_uint32**: Retrieves a JSON object by key and converts JSON to an unsigned 32-bit integer.
- **ajsono_scan_int64**: Retrieves a JSON object by key and converts JSON to a 64-bit integer.
- **ajsono_scan_uint64**: Retrieves a JSON object by key and converts JSON to an unsigned 64-bit integer.
- **ajsono_scan_float**: Retrieves a JSON object by key and converts JSON to a float.
- **ajsono_scan_double**: Retrieves a JSON object by key and converts JSON to a double.
- **ajsono_scan_bool**: Retrieves a JSON object by key and converts JSON to a boolean.
- **ajsono_scan_str**: Retrieves a JSON object by key and converts JSON to a string.
- **ajsono_scan_strd**: Retrieves a JSON object by key and converts JSON to a string with decoding.

The same functions also exist for get and find.  For example,
- **ajsono_get_int**: Retrieves a JSON object by key and converts JSON to an integer.
- **ajsono_find_int**: Retrieves a JSON object by key and converts JSON to an integer.
