# AT25040B host example

The driver on your PC, with no hardware: the host port in [`host_at25040b_port.hpp`](host_at25040b_port.hpp)
connects the real driver to a simulated AT25040B (the protocol model the host tests use), and [`main.cpp`](main.cpp)
checks the chip, writes 12 bytes across a page boundary, reads them back, then unplugs the simulated module and
probes again.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/at25040b_host_example
```

## Expected output

```text
AT25040B driver 0.1.0 on the host
at25040b: initialised
wrote 12 bytes at 0x0FC in 2 write cycles
read back: 6B 6F 6B 65 62 2D 61 74 32 35 00 01
at25040b: status reads 0xFF: nothing drives MISO
after unplugging: NoDevice, status byte 0xFF
```

The write from 0x0FC crosses a page boundary (0x100), so it takes two write cycles, and the second page needs A8
in the opcode. The lines that start with `at25040b:` come from the driver through the port's `Log()` hook.

The host tests build this folder too and run the example (CTest `at25040b_host_example`), so CI fails if it stops
compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: probe, write, read back, unplug, probe again |
| [`host_at25040b_port.hpp`](host_at25040b_port.hpp) | The host port: bus hooks over the simulated chip, `DelayUs()`, the WP pin and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
