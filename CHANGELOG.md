## 08/22/2025

**Refactor build system, improve JSON library API, add tests**

## Summary:

This PR introduces a major restructuring of the build system, refines API contracts, improves JSON parsing/dumping behavior, and adds a full test suite.

### Build System

* Removed **Changie** configs (`.changie.yaml`, `.changes/*`, `CHANGELOG.md`, `NEWS.md`, `build_install.sh`).
* Introduced **unified CMake build** with per-variant (`debug`, `memory`, `static`, `shared`) targets and aliasing.
* Added `build.sh` script with `build|install|coverage|clean` commands.
* Added `tests/CMakeLists.txt` and `tests/build.sh` for dedicated test builds.
* Updated **Dockerfile** to build on Ubuntu base image with explicit CMake installation.
* Simplified `.gitignore` with new build dirs (`build-unix_makefiles`, `build-cov`, `build-coverage`).

### Source & API

* Updated SPDX headers with clear ownership and contact.
* Expanded **ajson.h** documentation with design notes and contracts.
* Removed unused **binary JSON** type.
* Added **nocopy** string constructors for performance/memory control.
* Added **try\_**\* converters (`ajson_try_to_int`, etc.) for safer parsing without defaults.
* Added **object mutation APIs**: `ajsono_set`, `ajsono_remove`, `ajsona_clear`.
* Parser tightened: rejects trailing commas, leading zeros, lone surrogates; requires lookahead for numbers.
* Dumpers:

    * Ensure invalid UTF-8 in string values is filtered out.
    * Added pretty-printing with configurable indent.
    * Added size estimators (`ajson_dump_estimate`, `ajson_dump_pretty_estimate`).

### Tests

* Introduced **comprehensive test suite** in `tests/src/test_ajson.c`:

    * Type predicates, parsing, dumping, conversions.
    * UTF-8 handling (valid & invalid sequences).
    * Object/array mutations, path navigation, duplicate keys.
    * Encode/decode helpers, edge cases (trailing commas, BOM, literals).
    * Coverage instrumentation via LLVM `llvm-cov`.

### Misc

* Updated `AUTHORS` with GitHub link.
* Simplified `NOTICE` text.
