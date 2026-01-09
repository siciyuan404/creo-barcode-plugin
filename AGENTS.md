# AGENTS.md

This file provides guidelines for agentic coding assistants working on the Creo Barcode Plugin repository.

## Build Commands

### Standard Build
```bash
cmake -S . -B build
cmake --build build --config Release
```

### Build Plugin (with Creo Toolkit)
```bash
creo-barcode-plugin\build_plugin.bat
# Or manually:
cmake -S creo-barcode-plugin -B build_dll -DCREO_TOOLKIT_DIR="C:\Program Files\PTC\Creo 8.0.9.0\Common Files\protoolkit" -G "Visual Studio 17 2022" -A x64
cmake --build build_dll --config Release --target creo_barcode_plugin
```

### Run All Tests
```bash
cd build
ctest --output-on-failure
# Or via batch:
creo-barcode-plugin\run_tests.bat
```

### Run Single Test
```bash
# Run all tests in a specific test suite:
.\build\bin\Release\unit_tests.exe --gtest_filter=BarcodeGeneratorTest*

# Run specific test:
.\build\bin\Release\unit_tests.exe --gtest_filter=BarcodeGeneratorTest.ValidateDataCode128AcceptsAscii

# Run multiple tests with wildcard:
.\build\bin\Release\unit_tests.exe --gtest_filter=*Property4_COM_Grid*
```

## Code Style Guidelines

### C++ Files (src/, include/)

**Standard**: C++17 enforced in CMakeLists.txt
**Namespace**: `creo_barcode` (all C++ code lives in this namespace)

**Naming Conventions:**
- Classes: PascalCase (e.g., `BarcodeGenerator`, `ConfigManager`)
- Methods: camelCase (e.g., `generate()`, `validateData()`)
- Member variables: snake_case with trailing underscore (e.g., `lastError_`, `config_`)
- Local variables: snake_case (e.g., `testDir`, `outputPath`)
- Constants: ALL_CAPS (rare, prefer `constexpr`)
- Enums: PascalCase with values in SCREAMING_SNAKE_CASE
```cpp
enum class BarcodeType {
    CODE_128,
    CODE_39,
    QR_CODE
};
```

**Imports/Includes:**
- Include corresponding header first (e.g., `#include "barcode_generator.h"` in barcode_generator.cpp)
- Group external includes alphabetically
- Use forward declarations in headers when possible

**Formatting:**
- 4-space indentation (no tabs)
- Opening brace on same line for functions/methods
- Opening brace on new line for namespaces
- Maximum line length: ~120 characters
- One statement per line
- Empty line between functions

**Error Handling:**
- Return `bool` or `std::optional<T>` for operations that can fail
- Store error info in `ErrorInfo lastError_` member, accessible via `getLastError()`
- Use `ErrorCode` enum from error_codes.h for error classification
- Always validate inputs before processing
```cpp
if (data.empty()) {
    lastError_ = ErrorInfo(ErrorCode::INVALID_DATA, "Empty data");
    return false;
}
```

**Comments:**
- Doxygen-style comments for public APIs: `/// Brief description`
- C-style block comments for file headers: `/** @file ... */`
- Minimal inline comments for non-obvious logic only

**Const Correctness:**
- Mark methods `const` if they don't modify state
- Pass strings by `const std::string&` for non-nullable parameters
- Use `std::optional<std::string>` for nullable string returns

### C Files (src/*_pure_c.c, src/main_pure_c.c)

**Purpose**: Pure C implementation for Creo plugin DLL (avoids C++ runtime issues)

**Naming Conventions:**
- Functions: snake_case (e.g., `barcode_generate_pure_c()`, `calculate_checksum()`)
- Types: PascalCase with `_t` suffix for typedefs (e.g., `BMPFILEHEADER`)
- Macros: ALL_CAPS (e.g., `CODE128_PATTERNS`, `DRAW_PATTERN`)
- Global variables: prefix `g_` (e.g., `g_purec_error`)

**Comments:**
- Doxygen style: `/** @file ... @brief ... */`
- Use `/* */` for multi-line, `//` for single-line

**Error Handling:**
- Return int codes (0 = success, negative = error)
- Store error messages in global error buffer
- Use static local error buffers for thread safety

### Test Files (tests/)

**Naming:**
- Test files: `test_<module>.cpp`
- Test class: `<Module>Test` (e.g., `BarcodeGeneratorTest`)
- Test methods: `ShouldDoWhatWhenCondition` or `<Action><Result>` (e.g., `ValidateDataCode128AcceptsAscii`)

**Structure:**
- Group related tests with separator comments: `// ============================================================================
// Section Name
// ============================================================================`
- Use `SetUp()` and `TearDown()` for test fixture initialization
- Place test directory in temp directory for isolation

**Assertions:**
- Use `EXPECT_*` for non-fatal checks
- Use `ASSERT_*` when subsequent code depends on success
- Include context in failure messages for clarity

**Property Tests (property_tests.cpp):**
- Use RapidCheck for generative testing
- Combine with Google Test: `gtest_discover_tests(property_tests)`
- Focus on invariants and edge cases

## Project Structure

```
creo-barcode-plugin/
├── include/          - Public headers (no implementation)
├── src/              - Implementation files
├── tests/            - Unit and property tests
├── resources/        - Default config files
└── text/             - UI resources (menus, messages)
```

## Key Dependencies

- **ZXing-cpp**: Barcode generation/decoding
- **nlohmann/json**: JSON config parsing
- **stb**: Image I/O (header-only)
- **Google Test**: Unit testing framework
- **RapidCheck**: Property-based testing

## Important Notes

**DLL Safety**: 
- Logging macros are disabled (`LOG_INFO` etc. are no-ops) to avoid static initialization issues
- Plugin uses pure C implementation (no C++ runtime in DLL)

**MSVC Build**:
- UTF-8 source encoding enabled via `/utf-8` flag
- Release runtime forced: `MultiThreadedDLL` (no debug runtime in release)

**Testing**:
- Build directory: `build_unit` for unit tests
- Run specific tests using `--gtest_filter` flag
- Property tests available via `property_tests` executable
