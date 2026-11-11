"""Write() step by step: the status check, block protection, WP, then per page WREN, WRITE, the write cycle and the
read-back, with the error each step can return.

Reads: Top to bottom, one card per step of Write(); a pill on a card names the error that step can return, and any
step that sends a transfer also returns Bus when the transfer fails. The first two steps read the status (until no
write cycle runs, confirmed by a second read) and refuse a range that overlaps the block-protected part before
anything else is sent. WP is then released, if the port drives it. The band in the middle repeats once per 8-byte
page: WREN and a status read that must show WEN = 1, the WRITE, a wait for the write cycle polled every
kPollIntervalUs up to Config::write_timeout_us, a status that must show WEN = 0 again (still 1 means the chip
ignored the WRITE), and only when the call passes Verify::ReadBack a read-back of the page. After the last page, or the first
failure, WP is parked; a failure after WREN also sends WRDI. The steps and errors are read from writeRange(),
writePage(), enableWrite(), finishWriteCycle(), waitIdle() and withWriteAccess() in src/at25040b.ipp.
"""

from _source import IMPL, TYPES, body, constants, errors_in, need, read

from figkit import Fig


def data():
    impl = read(IMPL)
    types = constants(TYPES)
    top = body(impl, "AT25040B<Port>::Write(uint16_t address, std::span<const uint8_t> data,")
    rng = body(impl, "AT25040B<Port>::writeRange(uint16_t address, std::span<const uint8_t> data,")
    page = body(impl, "AT25040B<Port>::writePage(uint16_t address, std::span<const uint8_t> data,")
    enable = body(impl, "AT25040B<Port>::enableWrite()")
    finish = body(impl, "AT25040B<Port>::finishWriteCycle()")
    wait = body(impl, "AT25040B<Port>::waitIdle(bool started)")
    access = body(impl, "AT25040B<Port>::withWriteAccess(Op op)")
    release = body(impl, "AT25040B<Port>::releaseWriteProtect()")
    park = body(impl, "AT25040B<Port>::parkWriteProtect()")
    need(r"!initialized_\)\s*\{\s*return Result<void>::Failure\(Error::NotInitialized\)", top, "NotInitialized")
    need(r"Error::InvalidArgument", top, "range check")
    need(r"waitIdle\(true\).*ProtectedFrom\(config_\.variant, level\).*Error::BlockProtected.*withWriteAccess", rng,
         "status, then block protection, then WP")
    need(r"kPageSizeBytes - \(static_cast<std::size_t>\(at\) % kPageSizeBytes\)", rng, "page split")
    need(r"enableWrite\(\).*bus_\.Write\(address, data\).*finishWriteCycle\(\).*verify == Verify::ReadBack", page,
         "WREN, WRITE, cycle, verify")
    need(r"releaseWriteProtect\(\).*op\(\).*parkWriteProtect\(\)", access, "release, op, park")
    need(r"bus_\.Instruction\(reg::kWren\)", enable, "WREN")
    wait_errors = [e for e in errors_in(wait) if e in ("Timeout", "BusNoise")]
    steps_top = [
        {"title": "Check the range", "detail": "Inside the variant's array, length at least 1",
         "errors": ["NotInitialized", "InvalidArgument"], "tone": "slate"},
        {"title": "Read the status until idle", "detail": "Confirmed by a second read; waits out a running cycle",
         "errors": wait_errors, "tone": "slate"},
        {"title": "Refuse a protected range", "detail": "From BP1:BP0 (Table 6-4); nothing else is sent",
         "errors": errors_in(rng)[:1], "tone": "slate"},
        {"title": "Release WP", "detail": "GpioSet(WriteProtect, INACTIVE), when the port has it",
         "errors": [e for e in errors_in(release) if e == "Pin"], "tone": "slate"},
    ]
    verify = [e for e in errors_in(page) if e == "WriteMismatch"]
    steps_page = [
        {"title": "WREN, then read the status", "detail": "WEN must read 1",
         "errors": [e for e in errors_in(enable) if e in ("WriteProtected", "BusNoise")], "tone": "teal"},
        {"title": "WRITE", "detail": "Opcode with A8, address byte, 1 to 8 bytes", "errors": [], "tone": "teal"},
        {"title": "Wait for the write cycle", "detail":
         f"Status every {types['kPollIntervalUs']} us, up to write_timeout_us", "errors": [e for e in wait_errors if e == "Timeout"],
         "tone": "teal"},
        {"title": "WEN must be 0 again", "detail": "Still 1: the chip ignored the WRITE",
         "errors": errors_in(finish)[-1:], "tone": "teal"},
        {"title": "Read the page back", "detail": "Only with Verify::ReadBack", "errors": verify, "tone": "teal"},
    ]
    steps_end = [{"title": "Park WP", "detail": "After the last page or the first failure; WRDI after a failure",
                  "errors": [e for e in errors_in(park) if e == "Pin"], "tone": "slate"}]
    return {"top": steps_top, "page": steps_page, "end": steps_end, "page_bytes": types["kPageSizeBytes"],
            "sources": [f"{IMPL}:Write", f"{IMPL}:writeRange", f"{IMPL}:writePage", f"{IMPL}:enableWrite",
                        f"{IMPL}:finishWriteCycle", f"{IMPL}:waitIdle", f"{IMPL}:withWriteAccess"]}


def build(d):
    f = Fig(
        "write-path",
        "Write(), step by step",
        alt=("Flowchart of Write: check the range, read the status until idle, refuse a block-protected range, "
             "release WP, then per 8-byte page WREN and a WEN check, WRITE, the write cycle wait, a WEN check and a "
             "read-back; WP is parked at the end, and each step names the error it can return."),
        width=1200,
        kicker="API guide",
        subtitle="One card per step. A pill names the error the step adds; every transfer can also fail with Bus.",
        legend=[("slate", "once per call"), ("teal", "once per page"), ("rose", "error this step adds"),
                ("~solid", "next step")],
    )
    k = f.k
    W = f.body_w
    cw = 560
    x = 30.0
    cards = []
    y = 0.0
    for s in d["top"]:
        c = f.card(x, y, cw, s["title"], [s["detail"]], tone=s["tone"], accent=True)
        cards.append((c, s))
        y = c.b + 30
    ry = y
    inner = f.region_inner_top(ry, sub_below=True)
    y = inner + 4
    for s in d["page"]:
        c = f.card(x, y, cw, s["title"], [s["detail"]], tone=s["tone"], accent=True)
        cards.append((c, s))
        y = c.b + 30
    f.region(x - 30, ry, cw + 60, y - 12 - ry, f"For each {d['page_bytes']}-byte page", tone="teal",
             sub="A WRITE never crosses a page: the address wraps inside it", sub_below=True)
    y += 10
    for s in d["end"]:
        c = f.card(x, y, cw, s["title"], [s["detail"]], tone=s["tone"], accent=True)
        cards.append((c, s))
        y = c.b + 30
    for (a, _), (b, _) in zip(cards, cards[1:]):
        f.connect(a, b, at=0.9, bt=0.9)
    for c, s in cards:
        px = x + cw + 30 + 20
        for e in s["errors"]:
            p = f.pill(px, c.y + 10, e, tone="rose")
            px = p.r + 8
    f.note(0, y, W, "A failed call has written the pages before the failing one (and verified them); the failing page "
           "may hold old, new or partly written data; nothing after it and nothing outside the range changes.",
           tone="slate", title="After a failure")
    del k
    return f
