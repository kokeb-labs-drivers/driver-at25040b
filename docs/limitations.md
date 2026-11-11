# Status and limitations

What this driver has and has not been shown to do, so you can decide whether it is ready for your use.

## Status

| | |
| --- | --- |
| **Version** | 0.1.0, not released |
| **On hardware** | **Not verified on hardware.** No board has run this driver yet |
| **Tested against** | A protocol model of the three parts ([Testing and CI](testing.md)), a fuzzer and the quality gates |
| **Assurance level** | `high` (Kokeb Labs handbook): not qualified for use in a safety function |

## Bus faults the design handles

The chip gives the driver little to prove that it is there: it has no ID register, and its idle status is 0x00
(bits 7 to 4 read 0 outside a write cycle, Table 6-3). When no chip drives it, SO is in high impedance (section 5,
"Chip Select"), so MISO carries whatever the board's pull, a short or a loop-back puts on it. The driver is built so
that none of these can pass for a chip:

- **A MISO pulled or stuck low reads 0x00, an idle status.** So the start-up check does not stop at the status: it
  toggles the write enable latch. WEN is set only by a WREN the chip received with WP held high and cleared by WRDI
  (section 6, "Write Enable (WREN)", "Write Disable (WRDI)"; Table 6-2), which a stuck line cannot follow
  ([Contract](contract.md#the-identity-check)). A `Read()` with `Verify::ReadBack` reads every chunk twice. The model
  has the fault (`Miso::StuckLow`); the tests and the fuzzer cover it.
- **0xFF is a busy chip before it is an absent one.** Every status bit reads 1 during a write cycle (Table 6-3), and a
  cycle started before an MCU reset still runs for up to t<sub>WC</sub> (5 ms, Table 4-3). The driver therefore waits
  for up to `Config::write_timeout_us` before it reports `NoDevice`, and it tests for the busy byte before it looks at
  RDY.
- **Probe on attach, not at boot.** An unplugged module leaves MISO to its pull, so a probe at boot can only fail. The
  constructor does no I/O and nothing but `EnsureInitialized()` probes, so a handler probes when it knows the module
  is there.
- **HOLD strapped to CS makes the status read fail.** HOLD low while SCK is low pauses the chip and keeps SO in high
  impedance (section 5, "Hold"), so a HOLD tied to CS is low for every instruction. Tie HOLD high; the driver reports
  the strapped case as `NoDevice`, and the model has the fault (`hold_tied_to_cs`).
- **WP stays low except while it is needed.** WP low inhibits every write, and WP going low while CS is low interrupts
  a write that has not started its cycle (section 5, "Write Protect"). The driver releases WP only for a write, a
  block-protection change and the identity check (WREN needs WP high), and parks it after each, on failure too.

## What the model cannot show

The model is written from the datasheet, by the same reading as the driver. It cannot show:

- whether WEN clears at the end of a WRSR cycle. The datasheet says so for WRITE; the driver does not depend on it
  for WRSR (it reads BP1:BP0 back and sends WRDI if WEN is still set);
- what a real chip does with a WRITE into a protected range (the model ignores it; the driver never sends one);
- electrical behaviour: the clock limits, CS timing, a slow MISO edge, the real write cycle time (the model uses 3 ms);
- what a partly clocked WRITE does when a transfer fails half way: the driver treats that page as unknown.

## Before you depend on it

Run these on your board behind the handler, and record the result below:

1. Module plugged: `EnsureInitialized()` returns `Ok`; `GetLastStatusByte()` is 0x00 (or 0x0C etc. with block
   protection set).
2. Module unplugged: `NoDevice` (status 0xFF) or `DeviceIdMismatch` (status 0x00), never `Ok`.
3. WP held low by hand, module plugged: `DeviceIdMismatch` with `check_write_enable` on.
4. Read the whole 512-byte image twice with `Read(0, image, Verify::ReadBack)` (`image` a `std::array<uint8_t, 512>`):
   identical both times. Once more with the default (`Verify::None`): the same bytes.
5. On a scrap module, the whole array: `Write(0, image, Verify::ReadBack)` returns `Ok` in about 64 write cycles; read
   it back with `Verify::ReadBack`.
6. Watch WP on a scope or logic analyser during the probe and the write: high only during WREN/WRDI and the write.
7. In SPI mode 3, write a page and read it back; on a logic analyser, CS must rise while SCK is low after the last
   data bit (section 6, "Write Sequence").
8. At the port's highest clock, measure t<sub>CSS</sub>, t<sub>CSH</sub> and t<sub>CS</sub> against Table 4-3 for the
   board's supply.

## Hardware verification record (this driver)

| Board | Driver commit | Procedure | Result | Evidence |
| --- | --- | --- | --- | --- |
| (none yet) | | | | |
