---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# AT25040B Driver

<p>A C++20 driver for the Microchip (formerly Atmel) AT25010B, AT25020B and AT25040B SPI serial EEPROMs. It checks
that a chip really answers before trusting it, reads any range, writes page by page with the write enable latch,
the write cycle and a read-back checked at every step, drives the WP pin and sets the block protection, on any
MCU.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-at25040b){ .md-button }
</div>

The driver has no platform code. You give it a small class that brings the bus up, moves bytes and waits; it does
the rest: the identity check, the instruction framing with A8, the page split, the write cycle, the WP pin and the
checks that catch a dead or noisy bus.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">SPI serial EEPROM</span>
<span class="kb-chip__name">AT25010B / AT25020B / AT25040B</span>
<span class="kb-chip__vendor">Microchip (Atmel)</span>
</div>

Memory
:   128, 256 or 512 bytes; 8-byte pages; 1,000,000 write cycles, 100 years retention

Interface
:   SPI mode 0 or 3; 20 MHz from 4.5 V, 10 MHz from 2.5 V, 5 MHz from 1.8 V

Write cycle
:   Self-timed, 5 ms at most; no erase before a write

Protection
:   WP pin, write enable latch (WREN, WRDI), non-volatile block protection of a quarter, a half or all of the array

Supply
:   1.8 V to 5.5 V
</div>

## What you get

- **One object per chip.** `AT25040B<YourPort>` takes a reference to your port and does no I/O until
  `EnsureInitialized()`. See [API guide](api-guide.md).
- **A probe that a dead bus cannot pass.** The start-up check reads the status twice and toggles the write enable
  latch, so a floating, stuck or echoing MISO fails it. See [Contract](contract.md#the-identity-check).
- **Writes you can trust.** Page split, WREN per page, WEN checked, the write cycle waited for, each page read back.
  See [API guide](api-guide.md#write).
- **Errors you can act on.** Every call returns `Result<T>` with an `Error` such as `NoDevice`, `BusNoise` or
  `WriteProtected`. See [API guide](api-guide.md#handle-errors).
- **No heap, no exceptions, no RTTI.** The host tests build with `-fno-exceptions -fno-rtti`. See
  [Contract](contract.md).
- **Tested without hardware.** Host tests and a fuzzer run the driver against a protocol model of the chip. See
  [Testing and CI](testing.md). Not yet verified on hardware: [Status and limitations](limitations.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: AT25010B, AT25020B, AT25040B SPI Serial EEPROM (Microchip (Atmel) Atmel-8707) | F, 2015-01 | [PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8707-SEEPROM-AT25010B-020B-040B-Datasheet.pdf) · [vendor page](https://www.microchip.com/en-us/product/AT25040B) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, read the first bytes in about 15 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, instructions, timing, WP and HOLD, block protection.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs, threading and errors.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Probe, read, write, protect, and use it from a handler.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the port for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
