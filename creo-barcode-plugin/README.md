# Creo Barcode Plugin

A plugin for PTC Creo 8.0+ that generates barcodes in engineering drawings based on part names.

## Requirements

- CMake 3.16+
- C++17 compatible compiler
- PTC Creo 8.0+ with Toolkit (for full plugin functionality)

## Dependencies (fetched automatically)

- ZXing-cpp - Barcode generation and decoding
- nlohmann/json - JSON configuration handling
- stb - Image writing
- Google Test - Unit testing
- RapidCheck - Property-based testing

## Building

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### With Creo Toolkit

```bash
cmake -DCREO_TOOLKIT_DIR="C:/path/to/creo/toolkit" ..
```

## Running Tests

```bash
cd build
ctest --output-on-failure
```

Or run specific test executables:

```bash
./bin/unit_tests
./bin/property_tests
```

## Project Structure

```
creo-barcode-plugin/
├── src/           - Source files
├── include/       - Header files
├── tests/         - Unit and property tests
├── resources/     - Configuration files
└── CMakeLists.txt - Build configuration
```
