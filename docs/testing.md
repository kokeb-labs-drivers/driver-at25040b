# Testing and CI

How the driver is checked: host tests against a model of the chip, a fuzzer that corrupts what the chip sends
back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality
gates, on every pull request.

## Host tests

The tests build the real driver against a protocol model of the chip (`tests/mock_at25040b.hpp`). The model is written
from the datasheet on its own: it includes none of the driver's headers and carries its own opcodes, status bits, array
sizes and Table 6-4, each with its datasheet page, and `tests/test_model.cpp` cross-checks the two maps with
`static_assert` (handbook KB-TST-030). It models the opcodes with
A8 on the AT25040B, invalid opcodes that leave SO in high impedance, the write enable latch (set by WREN only with WP
high, cleared by WRDI whatever WP does and at the end of every write cycle), sequential READ with roll-over, WRITE
that wraps inside its page, a self-timed write cycle in simulated time during which only RDSR works and reads 0xFF,
block protection that refuses writes into the protected top and survives a power cycle. The faults are the ones
a board can produce or the datasheet names: MISO floating high, stuck low, echoing MOSI or random; HOLD strapped to CS;
WP tied low; a WRITE the chip silently ignores; a cell that does not take its value; a write cycle that never ends;
failed, lost and replaced transfers; a failing WP pin. They build with the firmware's flags,
`-fno-exceptions -fno-rtti`, `-pedantic-errors` and warnings as errors, as ISO C++20 (`CMAKE_CXX_EXTENSIONS OFF`), on
GCC and Clang, with the standard library's own bounds checks on (`_GLIBCXX_ASSERTIONS`, libc++ hardening).
`at25040b_host_tests` runs 74 test cases; with the Kconfig, version, install, Doxygen, host-example, compile-fail and
fuzz-corpus tests CTest runs 16 tests. The target `at25040b_header_checks` compiles each public header alone as
`-std=c++20 -pedantic-errors` and asserts the dialect; the Cortex-M cross-build compiles the same units, and the ESP32
example compiles `at25040b_cxx20_check.cpp` the same way inside ESP-IDF, which otherwise builds in a newer GNU dialect.

```bash
cmake -S . -B build -DAT25040B_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_at25040b.cpp`](../tests/test_at25040b.cpp) | The lifecycle (no I/O before the start, a retried start, re-probe), `Validate()`, the identity check against every bus fault, reads (chunks, A8, verification, waiting out a write cycle, a glitched status), writes (page split, a 512-byte whole-array write, WP, block protection, an ignored WRITE, a stuck cell, a cycle that never ends, a failing WP pin, the smaller parts), the status and block protection, logging |
| [`tests/test_bus_faults.cpp`](../tests/test_bus_faults.cpp) | Every transfer and every WP pin write of every operation, `EnsureInitialized()` included, fails in turn and each returns `Bus` (a transfer) or `Pin` (a pin write) with WP parked; every transfer of a write is lost in turn and none yields `Ok` without the data |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model's map against the driver's, at compile time: opcodes, status bits, sizes, Table 6-4 for every part and level |
| [`tests/fuzz/`](../tests/fuzz/fuzz_at25040b.cpp) | The fuzz target; CTest replays its corpus (`at25040b_fuzz_at25040b_corpus`), see below |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | Every chip-specific value (16 to 21) is returned by a real call |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a class that is not a port, a `Transfer()` with a narrower length or without `noexcept`, a `DelayUs(uint16_t)` |
| [`tests/mock_at25040b.hpp`](../tests/mock_at25040b.hpp) | The chip model; the mock ports over it are in `mock_at25040b_bus.hpp` |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`at25040b_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(at25040b)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

The model does not reproduce the chip's electrical behaviour, its real write cycle time (it uses 3 ms) or what a
partly clocked WRITE does: see [Status and limitations](limitations.md).

## Fuzz tests

Everything the driver receives comes from MISO, so MISO is what a noisy cable, an unplugged module or a broken chip
controls. [`tests/fuzz/fuzz_at25040b.cpp`](../tests/fuzz/fuzz_at25040b.cpp) lets libFuzzer choose a configuration,
then up to 64 driver calls, and on every transfer what the bus does: pass the model's reply, fail the transfer, lose
it (the chip never sees it, MISO floats), flip bits of the reply or replace it; on every WP pin write, pass or fail
it. Between calls it changes the board: MISO floating, stuck low, echoing or noisy, HOLD strapped to CS, WP tied
low, an ignored WRITE, a stuck cell, a slow or running write cycle, the block protection, a power cycle. A transfer
the bus rather than the chip answered counts as tampered. After every call the target checks the driver's promises
and aborts, naming the line, when one breaks:

- a failed call returns an `Error` the enum defines (never one documented as not returned) and `T{}`;
- before `EnsureInitialized()` no call makes a transfer or a pin write, and `InvalidArgument` means nothing was sent;
- nothing is ever written outside the requested range; only `Write()` changes the array and only
  `SetBlockProtection()` changes BP1:BP0;
- **a call never returns `Ok` for bytes the chip does not hold**: an `Ok` from `Read()` or `Write()` matches the chip
  unless two transfers of the call were tampered (one, with `Verify::None`); the input chooses `Verify::None` or
  `Verify::ReadBack` for the reads and writes;
- with `check_write_enable`, `EnsureInitialized()` never returns `Ok` on a dead bus or with WP tied low unless a
  transfer was tampered;
- **WP is always parked after a call**, unless the last pin write failed;
- an `Ok` call leaves the write enable latch clear if it found it clear, unless a transfer was tampered.

The fuzzer found two bugs before the first commit, both fixed with a unit test and a `regress-*` input each:

- With the write enable latch already set (a WREN before an MCU reset) and WP held low, the identity check's
  "WREN, then WEN = 1" passed although WP had blocked the WREN. The driver now clears the latch first
  (`regress-wen-preset-wp-low`).
- One status read that looked idle (a lost transfer reading 0x00) while a write cycle ran let `Read()` go ahead; the
  busy chip ignored both READs, both copies read 0xFF, and `Read()` returned `Ok` with wrong bytes. An idle status now
  counts only when a second read confirms it (`regress-idle-glitch-during-cycle`).

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DAT25040B_BUILD_TESTS=ON -DAT25040B_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_at25040b
./build-fuzz/tests/fuzz/fuzz_at25040b -max_total_time=300 tests/fuzz/corpus/fuzz_at25040b
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_at25040b`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_at25040b/`](../tests/fuzz/corpus/fuzz_at25040b) on GCC and Clang: the seeds, and every input
that ever broke a promise (`regress-*`), so a fixed bug stays fixed. CI fuzzes for 120 s on every pull request
(job `Host quality / fuzz`). After the fixes, ten minutes of local fuzzing (9.6 million runs, 451 coverage edges)
broke no promise; after the move to spans, the `Pin` error and `ReadRawStatus()`, 200 s more (2.28 million runs,
478 edges) broke none.

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` | This driver (local) |
| --- | --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 | highest 11 (`waitIdle()`) |
| Function length | lizard, lines of code per function | at most 60 | within the limit |
| Parameters | lizard, per function | at most 6 | within the limit |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % | 99.5 % |
| Branch coverage | same run | at least 75 % | 98.4 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) | 99.2 % |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report | clean in CI |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise | 2.28 million runs in 200 s locally, clean |

The local figures are GCC 15 and gcovr 8.7 with the CI's options; CI uses GCC 14. The reports are in the run's
`host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DAT25040B_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc/*.hpp src/*.ipp   # list the files: lizard skips .ipp in a directory

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DAT25040B_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `at25040b_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

The driver is **not verified on hardware** yet. The ESP32 example ([Examples](examples.md)) is a first bench test:
it checks the chip, reads the first bytes, counts boots in the last page and probes once a second; unplug the module
and plug it back while it runs. The checks that count, behind your handler on your board, are listed in
[Status and limitations](limitations.md#before-you-depend-on-it). Record the board, the driver commit and the result
there.
