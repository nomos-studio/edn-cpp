# edn-cpp

A standalone, first-rate C++20 EDN parser and emitter. Zero Clojure dependency — it speaks EDN as a data format, not as a Clojure artifact.

Part of the [nous](https://github.com/nous) ecosystem. Used directly by the nous sidecar and CLAP host container for reading patch files, plugin graph descriptions, and transaction log interchange.

## Status

Active development. API is stabilising; not yet at 1.0.

## Features

- Full EDN value type coverage: nil, bool, int64, double, rational, bigint, bigdec, string, character, keyword, symbol, list, vector, map, set, tagged literals
- String interning for keyword and symbol (program-lifetime `string_view` handles)
- Sorted-vector backing for `edn::map` and `edn::set` (cache-friendly; correct for small n)
- No-exception result type (`edn::result<T>`, switches to `std::expected` on C++23)
- Tag dispatch via `parser_opts` — bring your own handlers; defaults preserve unknown tags as `edn::tagged`
- Built-in opt-in handlers: `#inst` (RFC 3339 → UTC microseconds), `#uuid`
- Compact emitter (`edn::to_string`) and pretty-printer (`edn::pretty_string`)
- No Boost; no heavy dependencies

## Quick start

```cpp
#include <edn/parser.hpp>
#include <edn/emitter.hpp>

// Parse
auto result = edn::parse("{:tempo 120 :scale [:C :D :E]}");
if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
}

const auto& m = result->get<edn::map>();
const auto* tempo = m.find_kw("tempo");  // returns const edn::value*
// tempo->get<int64_t>() == 120

// Emit
std::string s = edn::to_string(*result);
// "{:scale [:C :D :E] :tempo 120}"  (map keys sorted by value_less)
```

## Requirements

- C++20 or later
- CMake 3.20 or later
- No other build-time dependencies (tests use Catch2, fetched automatically)

## Building

Using CMake presets:

```sh
cmake --preset dev        # configure: tests on, Debug, compile_commands.json
cmake --build --preset dev
ctest --preset dev
```

Or manually:

```sh
cmake -S . -B build -DEDN_CPP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

Release build:

```sh
cmake --preset release
cmake --build --preset release
```

## Using as a dependency

**Via FetchContent (recommended):**

```cmake
FetchContent_Declare(edn-cpp
  GIT_REPOSITORY https://github.com/nous/edn-cpp.git
  GIT_TAG        v0.1.0
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(edn-cpp)

target_link_libraries(my-target PRIVATE edn::edn)
```

**Via find_package (after install):**

```cmake
find_package(edn-cpp REQUIRED)
target_link_libraries(my-target PRIVATE edn::edn)
```

## API overview

### Types (`edn/value.hpp`)

| EDN type       | C++ type           |
|----------------|--------------------|
| nil            | `std::monostate`   |
| boolean        | `bool`             |
| integer        | `int64_t`          |
| float          | `double`           |
| rational       | `edn::rational`    |
| bigint         | `edn::bigint`      |
| bigdec         | `edn::bigdec`      |
| string         | `std::string`      |
| character      | `edn::character`   |
| keyword        | `edn::keyword`     |
| symbol         | `edn::symbol`      |
| list           | `edn::list`        |
| vector         | `edn::vector`      |
| map            | `edn::map`         |
| set            | `edn::set`         |
| tagged literal | `edn::tagged`      |

All are alternatives of `edn::value` (a `std::variant`). Use `std::visit` or the typed `.get<T>()` / `.is<T>()` accessors.

### Parsing

```cpp
edn::result<edn::value> edn::parse(std::string_view, edn::parser_opts = {});
edn::result<edn::value> edn::parse(std::istream&,    edn::parser_opts = {});
```

### Emitting

```cpp
std::string edn::to_string(const edn::value&);
void        edn::write(std::ostream&, const edn::value&);
std::string edn::pretty_string(const edn::value&, edn::format_opts = {});
void        edn::pretty_print(std::ostream&, const edn::value&, edn::format_opts = {});
```

### Built-in tag handlers

```cpp
#include <edn/builtins.hpp>

edn::parser_opts opts;
edn::builtins::register_inst(opts);   // #inst → tagged{"inst", int64_t{utc_microseconds}}
edn::builtins::register_uuid(opts);   // #uuid → tagged{"uuid", string{canonical}}
edn::builtins::register_all(opts);
```

## Canonical total ordering

`edn::value_less` provides a strict weak ordering over all EDN values, used internally by `edn::map` and `edn::set`. Type rank:

```
nil < bool < int64 < double < rational < bigint < bigdec
    < string < character < keyword < symbol
    < vector < list < map < set < tagged
```

Within each type: numeric/lexicographic/recursive ordering.

## License

BSL-1.0. See [LICENSE](LICENSE).
