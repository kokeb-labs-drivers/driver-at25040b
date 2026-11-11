"""The six instructions as the driver sends them: one transfer each, byte by byte, with A8 in the opcode.

Reads: Each row is one transfer, CS low from the first byte to the last, drawn left to right in the order the bytes
are clocked; the top line of a byte is what the driver sends on MOSI, the bottom line what it keeps from MISO.
Every opcode is 0000 followed by four bits, written out in binary. For READ and WRITE, bit 3 of the opcode carries
address bit A8 on the AT25040B and the next byte holds A7 to A0; the driver sets A8 for addresses 0x100 to 0x1FF and
never on the smaller parts. RDSR returns the status in its second byte; READ returns data from the third byte on,
up to kReadChunkBytes per transfer; WRITE carries 1 to kPageSizeBytes data bytes inside one page. WREN, WRDI, WRSR
and WRITE need no reply, and the driver passes no receive buffer for them. The opcodes come from
inc/at25040b_registers.hpp and the byte layouts from detail::EepromBus in inc/at25040b_spi_interface.hpp.
"""

from _source import REGISTERS, SPI, TYPES, body, constants, names, need, read

from figkit import Fig


def data():
    reg = constants(REGISTERS)
    types = constants(TYPES)
    nm = names(REGISTERS)
    spi = read(SPI)
    need(r"static_cast<uint32_t>\(reg::kAddressA8\)\) != 0U\)?\s*\?\s*static_cast<uint8_t>\(static_cast<uint32_t>\(opcode\) \|"
         r"\s*static_cast<uint32_t>\(reg::kOpcodeA8\)\)", spi,
         "AddressedOpcode puts A8 into the opcode")
    a8_bit = reg["kOpcodeA8"].bit_length() - 1
    if reg["kAddressA8"] != 0x100:
        raise SystemExit("figure source drift: kAddressA8 is not address bit 8")
    rows = []
    instr = body(spi, "Error Instruction(uint8_t opcode)")
    need(r"tx_\[0\] = opcode;.*Transfer\(tx_\.data\(\), nullptr, 1U\)", instr, "one-byte instruction, no reply")
    for key in ("kWren", "kWrdi"):
        rows.append({"name": nm[key], "opcode": reg[key], "bytes": [("op", "")], "reply": None})
    rdsr = body(spi, "Error ReadStatus(uint8_t& status)")
    need(r"tx_\[0\] = reg::kRdsr;\s*tx_\[1\] = 0x00U;.*Transfer\(tx_\.data\(\), rx_\.data\(\), 2U\)", rdsr,
         "RDSR then a dummy byte")
    need(r"status = rx_\[1\]", rdsr, "status in the second byte")
    rows.append({"name": nm["kRdsr"], "opcode": reg["kRdsr"], "bytes": [("op", ""), ("status", "")], "reply": 1})
    wrsr = body(spi, "Error WriteStatus(uint8_t value)")
    need(r"tx_\[0\] = reg::kWrsr;\s*tx_\[1\] = value;", wrsr, "WRSR then the value")
    rows.append({"name": nm["kWrsr"], "opcode": reg["kWrsr"], "bytes": [("op", ""), ("value", "")], "reply": None})
    rd = body(spi, "Error Read(uint16_t address, std::span<uint8_t> out)")
    need(r"out\.size\(\) > kReadChunkBytes", rd, "READ length bounded by the buffer")
    need(r"tx_\[0\] = AddressedOpcode\(reg::kRead, address\)", rd, "READ opcode with A8")
    need(r"tx_\[1\] = AddressByte\(address\)", rd, "READ address byte")
    need(r"out\[i\] = rx_\[kCommandBytes \+ i\]", rd, "READ data from the third byte")
    rows.append({"name": nm["kRead"], "opcode": reg["kRead"], "bytes": [("op", "a8"), ("addr", ""), ("data", "")],
                 "reply": 2, "max": types["kReadChunkBytes"]})
    wr = body(spi, "Error Write(uint16_t address, std::span<const uint8_t> data)")
    need(r"data\.size\(\) > \(?kPageSizeBytes - offset\)?", wr, "WRITE bounded by its page")
    need(r"tx_\[0\] = AddressedOpcode\(reg::kWrite, address\)", wr, "WRITE opcode with A8")
    need(r"Transfer\(tx_\.data\(\), nullptr, kCommandBytes \+ data\.size\(\)\)", wr, "WRITE without a reply")
    rows.append({"name": nm["kWrite"], "opcode": reg["kWrite"], "bytes": [("op", "a8"), ("addr", ""), ("data", "")],
                 "reply": None, "max": types["kPageSizeBytes"]})
    return {"rows": rows, "a8_bit": a8_bit, "max_transfer": types["kMaxTransferBytes"],
            "sources": [f"{REGISTERS}:opcodes", f"{SPI}:detail::EepromBus", f"{TYPES}:kReadChunkBytes"]}


def _bits(op: int, a8: bool) -> str:
    s = f"{op:08b}"
    s = s[:4] + " " + s[4:]
    return s[:5] + "A" + s[6:] if a8 else s


def build(d):
    f = Fig(
        "instruction-frames",
        "Six instructions, one transfer each",
        alt=("Byte layouts of WREN, WRDI, RDSR, WRSR, READ and WRITE: a one-byte opcode, then for RDSR the status "
             "byte, for WRSR the value, for READ and WRITE an address byte and data; bit 3 of the READ and WRITE "
             "opcodes carries address bit A8 on the AT25040B."),
        width=1200,
        kicker="Hardware",
        subtitle="CS low for the whole row, bytes in clock order, most significant bit first.",
        legend=[("brand", "opcode"), ("blue", "address A7 to A0"), ("teal", "data or value"),
                ("amber", "reply the driver keeps")],
    )
    k = f.k
    lab = 120
    bw, bh, gap = 190, 54, 10
    y = 0.0
    for r in d["rows"]:
        f.text(0, y + bh / 2 + k["card_title"] * 0.36, r["name"], size=k["card_title"], weight=700)
        x = lab
        for i, (kind, flag) in enumerate(r["bytes"]):
            if kind == "op":
                tone, top, bottom = "brand", _bits(r["opcode"], flag == "a8"), f"0x{r['opcode']:02X}"
                if flag == "a8":
                    bottom = f"0x{r['opcode']:02X}, 0x{r['opcode'] | (1 << d['a8_bit']):02X} with A8"
            elif kind == "addr":
                tone, top, bottom = "blue", "A7 ... A0", "address byte"
            elif kind == "status":
                tone, top, bottom = "amber", "status", "MISO: the reply"
            elif kind == "value":
                tone, top, bottom = "teal", "0000 BP1 BP0 00", "the new level"
            else:
                n = r["max"]
                tone = "amber" if r["reply"] is not None else "teal"
                top = f"data, 1 to {n} bytes"
                bottom = "MISO: the bytes read" if r["reply"] is not None else "inside one 8-byte page"
            w = bw * (1.6 if kind == "data" else 1.0)
            f.rect(x, y, w, bh, fill=f"{tone}.tint", stroke=f"{tone}.strong", rx=6, sw=1.2)
            f.text(x + w / 2, y + 22, top, size=k["card_body"], weight=600, color=f"{tone}.text", bg=f"{tone}.tint",
                   anchor="middle", mono=kind == "op")
            f.text(x + w / 2, y + 42, bottom, size=k["small"], color=f"{tone}.text", bg=f"{tone}.tint",
                   anchor="middle")
            x += w + gap
            del i
        y += bh + 22
    f.note(0, y + 6, f.body_w,
           f"Bit {d['a8_bit']} of the READ and WRITE opcodes is address bit A8 on the AT25040B and a don't-care on "
           f"the smaller parts, where the driver never sets it. A transfer is at most {d['max_transfer']} bytes. "
           "During a write cycle the chip answers only RDSR, with every bit 1.", tone="brand", title="A8 and limits")
    return f
