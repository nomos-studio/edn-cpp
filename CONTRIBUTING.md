# Contributing to edn-cpp

This document captures the conventions for this project. It is also the reference template for future nous native C++ satellite repos — if you are starting a new one, copy this and adapt the project-specific sections.

---

## Build system conventions

### CMake version and structure

- **Minimum: CMake 3.20.** All nous native repos use 3.20 as the floor.
- Root `CMakeLists.txt` defines the library target and aggregates subdirectory options.
- Each subdirectory (`src/`, `tests/`, `examples/`) has its own `CMakeLists.txt`.
- Use `add_subdirectory` — no `include()` of sibling directories.

### Target naming

- Library target: `<project-name>` (e.g. `edn-cpp`)
- Namespace alias: `<ns>::<name>` (e.g. `edn::edn`)  — consumers use the alias, never the raw target
- Test executable: `<project-name>-tests`
- Option prefix: `<PROJECT_NAME_UPPER>_` (e.g. `EDN_CPP_BUILD_TESTS`)

### Include paths

Always use generator expressions so the library works both in-tree (FetchContent) and installed:

```cmake
target_include_directories(my-lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
```

### C++ standard

- **C++20 minimum.** Set via target property, not globally:

```cmake
set_target_properties(my-lib PROPERTIES
  CXX_STANDARD          20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS        OFF
)
```

- C++23 features are used opportunistically via feature-test macros with C++20 fallbacks. Do not raise the minimum to C++23.

### Dependency policy

Following the nous native convention:

- **No Boost.** C++20 STL is sufficient.
- External dependencies via `FetchContent` at a **pinned tag**. Always `GIT_SHALLOW TRUE`.
- Set `FETCHCONTENT_UPDATES_DISCONNECTED ON` — skip network fetch on subsequent invocations.
- Tests use **Catch2 v3** (BSL-1.0, header-friendly). Pin the tag; update deliberately.
- No `find_package` for libraries that diverge across distro packaging (use FetchContent + pin instead).

### Package export

Every library provides CMake package export for `find_package` consumers:

```cmake
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(TARGETS my-lib EXPORT my-lib-targets ...)
install(EXPORT my-lib-targets
  FILE      my-lib-targets.cmake
  NAMESPACE my::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/my-lib)

configure_package_config_file(
  cmake/my-lib-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/my-lib-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/my-lib)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/my-lib-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
```

Template `cmake/my-lib-config.cmake.in`:

```cmake
@PACKAGE_INIT@
include("${CMAKE_CURRENT_LIST_DIR}/my-lib-targets.cmake")
check_required_components(my-lib)
```

---

## Code style

### Naming

- All types and functions: `lowercase_snake_case`, matching `std::` conventions.
- No `CamelCase` types; no Hungarian prefixes.
- Namespace everything under the project namespace (`edn`, `bwosc`, etc.).

### Headers

- One logical unit per header. Keep headers lean.
- SPDX identifier on the first line: `// SPDX-License-Identifier: BSL-1.0`
- `#pragma once` (not include guards).
- Forward-declare where possible; avoid transitive includes in public headers.

### Comments

Write no comments by default. Add a comment only when the **why** is non-obvious: a hidden constraint, a subtle invariant, or a workaround for a specific bug. Never describe what the code does — names do that.

### Error handling

- No exceptions in library code by default. Use a result type (`edn::result<T>`).
- On C++23 with `std::expected` available, switch via feature-test macro. Keep the same call-site API.
- Validate at system boundaries (user input, external APIs). Trust internal invariants.

### Memory and ownership

- Prefer value semantics. Use `std::unique_ptr` for heap-allocated recursive structures (e.g. `edn::tagged`).
- No raw `new`/`delete`.
- `std::shared_ptr` only when shared ownership is genuinely required.

### Recursion in types

When a type is self-referential (e.g. a variant that contains collections of itself):

1. Forward-declare the central type.
2. Define compound types using `std::vector<value>` — this is valid in C++17+ (P0674R1); `vector`'s size is fixed (3 pointers) regardless of element type.
3. Do **not** define member functions inline in struct bodies while the element type is incomplete — libc++ (Apple Clang) will reject it. Declare in the header, define in the `.cpp` after the type is complete.
4. For single-value recursion (e.g. `tagged`), use `std::unique_ptr<value>` to break the size cycle.

---

## Testing

- Framework: **Catch2 v3** (`Catch2::Catch2WithMain` target).
- One test file per module: `test_<module>.cpp`.
- Tests registered automatically via `catch_discover_tests`.
- Test all public API surface. Aim for: leaf types, compound types, parser round-trips, error cases, value_less ordering.
- Avoid using `std::string_view` directly in `CHECK()`/`REQUIRE()` comparisons — Catch2 may lack a `StringMaker` for it. Cast to `std::string` at the comparison site.

### Running tests

Using the `dev` CMake preset (recommended):

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Or manually:

```sh
cmake -S . -B build -DEDN_CPP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## C++ formatting

All C++ source files are formatted with **clang-format** using the project `.clang-format` config at the repo root. The style is based on LLVM with Boost-friendly overrides: 4-space indentation, left pointer/reference alignment (`T& x`, `T* x`), 100-column limit, and aligned consecutive declarations within a block.

### Formatting a file

```sh
clang-format -i path/to/file.cpp
```

### Formatting all C++ sources at once

```sh
clang-format -i include/edn/*.hpp src/*.cpp tests/*.cpp
```

### Checking without modifying (CI / pre-commit style)

```sh
clang-format --dry-run --Werror path/to/file.cpp
```

Exits non-zero if the file would be changed.

---

## Git hooks

Project hooks live in `scripts/` and are checked into the repo. They guard against leaking personal paths, secrets, and credential files, and enforce clang-format on staged C++ files.

Install once after cloning:

```sh
bash scripts/install-hooks.sh
```

The pre-commit hook (`scripts/pre-commit`) blocks on:
- Credential file extensions (`.pem`, `.key`, `.env`, …)
- Known secret patterns (AWS keys, PEM headers, API key assignments, …)
- Hardcoded absolute personal paths (`/Users/<name>/…`, `/home/<name>/…`)
- C++ files that do not pass `clang-format --dry-run --Werror`

If `clang-format` is not installed, the format check is skipped with a warning. Emergency bypass: `git commit --no-verify`.

---

## clangd / IDE integration

Generate `compile_commands.json` and symlink it to the project root:

```sh
cmake --preset dev          # sets CMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json compile_commands.json
```

A `.clangd` file in the project root provides fallback flags for editors that don't pick up the compilation database automatically:

```yaml
CompileFlags:
  Add:
    - "-std=c++20"
    - "-Iinclude"
```

---

## Licensing

All source files carry an SPDX identifier on the first line. nous satellite repos default to **BSL-1.0** for standalone C++ libraries — permissive, common in the C++ ecosystem, requires license text only in source distributions. MIT is an alternative for repos targeting a broader ecosystem audience. The main nous repo uses LGPL-2.1-or-later; note the distinction.

Compliance is checked with [REUSE](https://reuse.software/) (`pip install reuse`):

```sh
reuse lint
```

This must exit 0 before a release tag. CI runs it automatically. Each repo carries a `LICENSES/` directory at its root with the full text of every identifier it uses.

---

## Checklist for new native C++ satellite repos

- [ ] Copy this CONTRIBUTING.md and adapt project-specific sections
- [ ] Root `CMakeLists.txt` with correct target naming, generator expressions, and package export
- [ ] `cmake/` directory with `config.cmake.in`
- [ ] SPDX headers on all files; `reuse lint` exits 0
- [ ] `LICENSES/` directory at repo root with the full license text(s) used
- [ ] `.clangd` with relative `-Iinclude` flag
- [ ] `compile_commands.json` symlink in `.gitignore` (it's a generated file)
- [ ] `.clang-format` at repo root (copy from edn-cpp; adjust column limit if needed)
- [ ] `CMakePresets.json` with `dev`, `ci`, and `release` presets (copy from edn-cpp; adjust option prefix)
- [ ] `.github/workflows/ci.yml` with build+test matrix, clang-format check, and `reuse lint` step (copy from edn-cpp)
- [ ] `scripts/pre-commit` and `scripts/install-hooks.sh` (copy from edn-cpp)
- [ ] Catch2 test target with `catch_discover_tests`
- [ ] `LICENSE` file (BSL-1.0 or MIT — decide per repo; default BSL-1.0 for C++ libs)
