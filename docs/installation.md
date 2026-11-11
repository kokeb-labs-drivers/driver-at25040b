# Installation

Add the driver to a CMake project on any MCU or on your PC, or to an ESP-IDF project. It is a header-only C++20
library with no dependencies, and one root `CMakeLists.txt` serves every build.

## Requirements

- A C++20 compiler: GCC 10+, Clang 13+ or a current MSVC. The driver uses concepts and `requires`.
- CMake 3.16 or later: it generates the version header the driver includes (`at25040b_version.h`).
- No RTOS, heap, exceptions or RTTI: every call is `noexcept` and returns a `Result`.

## Get the source

A git submodule keeps the driver pinned to the version you chose:

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-at25040b.git external/driver-at25040b
git submodule update --init external/driver-at25040b
```

Only `inc/`, `src/`, `cmake/`, `_config/` and `CMakeLists.txt` are needed to build. The examples are not; the ESP32
example even has its own submodule (`examples/esp32/scripts`).

## CMake (any MCU, any host)

This is the MCU-neutral path: the same two lines work for a host build, STM32 (CubeMX CMake projects), RP2040
(Pico SDK), nRF (nRF Connect SDK) or any other CMake toolchain. [Platforms](platforms.md) lists what has a port
today.

```cmake
add_subdirectory(external/driver-at25040b)
target_link_libraries(my_app PRIVATE at25040b::at25040b)
```

`at25040b::at25040b` is an alias of the `at25040b` INTERFACE target. It carries the include directories (the headers
and the generated version header) and requires C++20, so your target does not need to set either.

| Option | Default | Effect |
| --- | --- | --- |
| `AT25040B_ENABLE_WARNINGS` | `OFF` | `-Wall -Wextra -Wpedantic` on every target that links the driver |
| `AT25040B_BUILD_TESTS` | `OFF` | Builds the host tests in `tests/` and the host example ([Testing and CI](testing.md)) |

### Installed package

To use the driver without adding its source tree to your build, install it and find the package:

```bash
cmake -S external/driver-at25040b -B build-at25040b
cmake --install build-at25040b --prefix /opt/sdk
```

```cmake
find_package(at25040b 0.1 REQUIRED)            # with CMAKE_PREFIX_PATH=/opt/sdk
target_link_libraries(my_app PRIVATE at25040b::at25040b)
```

The target is `at25040b::at25040b` in both cases (`EXPORT_NAME`), so a project can switch between `add_subdirectory()`
and `find_package()` without changing its `target_link_libraries()`. The headers and the generated
`at25040b_version.h` go to `include/at25040b/inc/`, the template implementation to
`include/at25040b/src/at25040b.ipp`. The host CI installs the package and builds a consumer against it on every
pull request ([Testing and CI](testing.md)).

## ESP-IDF

Under ESP-IDF the root [`CMakeLists.txt`](../CMakeLists.txt) registers the repository itself as a component: with
`ESP_PLATFORM` set it calls `idf_component_register()` from the same build settings, with the menu in
[`_config/Kconfig`](../_config/Kconfig). There is no wrapper to copy. ESP-IDF names a component after its folder,
so the name in your `REQUIRES` is the name of the folder the repository is in. Pick one of these:

| How | What to write | Component name |
| --- | --- | --- |
| Folder of extra components | `list(APPEND EXTRA_COMPONENT_DIRS external/driver-at25040b)` in the project `CMakeLists.txt`, before `project.cmake` | `driver-at25040b` (the folder) |
| Submodule under `components/` | `git submodule add https://github.com/kokeb-labs-drivers/driver-at25040b.git components/at25040b` | `at25040b` (the folder) |
| Component manager, from git | The dependency below in `main/idf_component.yml` | `at25040b` (the key) |

```yaml
# main/idf_component.yml of your project
dependencies:
  at25040b:
    git: https://github.com/kokeb-labs-drivers/driver-at25040b.git
    version: v0.1.0        # a release tag
```

Your component then declares `REQUIRES at25040b` (or the folder name), and `idf.py menuconfig` shows the driver's
menu ([Configuration](configuration.md#kconfig)). The ESP32 example uses the first way, with
`EXTRA_COMPONENT_DIRS` pointing at the repository root, and works out the folder name in `main/CMakeLists.txt`; CI
builds it on every pull request.

The repository has no `idf_component.yml`. The component manager does not need one for a git dependency; the
manifest is only required to publish to the ESP Component Registry, which these releases do not use. If registry
publishing is chosen, the manifest is added at the root then.

## How the build is organised

| Layer | File | Role |
| --- | --- | --- |
| Settings | [`cmake/at25040b_build_settings.cmake`](../cmake/at25040b_build_settings.cmake) | Target name, include directories, sources, version: the single source of truth |
| Library | [`CMakeLists.txt`](../CMakeLists.txt) | INTERFACE target, alias, install rules and the package export, or the ESP-IDF component |
| Tool configuration | [`_config/`](../_config/Doxyfile) | Doxyfile, the docs site, Kconfig, clang-format, clang-tidy and the linters |

## Version

The version is set once in the settings file and written into `at25040b_version.h` in the build tree at configure
time. The Doxyfile (`_config/Doxyfile`) carries a copy; a host test fails when it differs. Read it at run time:

```cpp
const char* v = at25040b::GetDriverVersion();   // "0.1.0"
```

## Next

[Quickstart](quickstart.md) runs the driver on your PC, then on your MCU; [Porting](porting.md) writes the port for
your MCU.
