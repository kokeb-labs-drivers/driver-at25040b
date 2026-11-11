"""EnsureInitialized(): the identity check, strongest test first, and what each bus fault looks like to it.

Reads: The left column is the start-up as the driver runs it, top to bottom; a pill names the error a step returns,
and any step that sends a transfer also returns Bus when it fails. The status is read until it is not busy, for at
most Config::write_timeout_us, and an idle status counts only when a second read gives the same byte. With
Config::check_write_enable (the default) WP is released, the latch cleared first if it was set, then WREN must show
WEN = 1 and WRDI WEN = 0, and WP is parked again; nothing is programmed. Without it, for a board whose WP is tied low,
the first kIdentityReadBytes are read twice and must match, a weaker test. The right column lists the bus and board
faults the host tests put on the chip model, each with the status byte the driver saw and the error EnsureInitialized()
returned, read from the test cases in tests/test_at25040b.cpp. The steps come from initialize(),
checkWriteEnableToggle(), checkStableHead() and waitIdle() in src/at25040b.ipp.
"""

import re

from _source import IMPL, TESTS, TYPES, body, constants, errors_in, need, read, test_cases

from figkit import Fig

FAULTS = {
    "Miso::FloatHigh": ("MISO floating high: module unplugged", "Status 0xFF for the whole timeout"),
    "Miso::StuckLow": ("MISO stuck low or pulled low", "Status 0x00, but WEN never reads 1"),
    "Miso::EchoMosi": ("MOSI echoed on MISO", "Status 0x05, the RDSR opcode: RDY set"),
    "hold_tied_to_cs": ("HOLD strapped to CS", "SO stays high impedance: status 0xFF"),
    "wp_tied_low": ("WP held low on the board", "Status 0x00, WREN ignored: WEN stays 0"),
}


def _faults():
    out = []
    for name, text in test_cases():
        if "check_write_enable = false" in text.split("EnsureInitialized()")[0]:
            continue
        m = re.search(r"EnsureInitialized\(\)\.error, Error::(\w+)\)", text)
        if not m:
            continue
        key = next((k for k in FAULTS if re.search(rf"(miso = {re.escape(k)}|{k} = true);", text)), None)
        if key is None:
            continue
        st = re.search(r"GetLastStatusByte\(\), (0x[0-9A-Fa-f]+|reg::kRdsr)\)", text)
        status = {"reg::kRdsr": "0x05"}.get(st.group(1), st.group(1)) if st else ""
        old = next((f for f in out if f["key"] == key), None)
        if old is not None and (old["status"] or not status):
            continue  # keep the test that also checks the status byte
        if old is not None:
            out.remove(old)
        out.append({"key": key, "fault": FAULTS[key][0], "shows": FAULTS[key][1], "error": m.group(1),
                    "status": status, "test": name})
    out.sort(key=lambda f: list(FAULTS).index(f["key"]))
    if len(out) != len(FAULTS):
        missing = sorted(set(FAULTS) - {f["key"] for f in out})
        raise SystemExit(f"figure source drift: no start-up test for {missing}")
    return out


def data():
    impl = read(IMPL)
    types = constants(TYPES)
    init = body(impl, "AT25040B<Port>::initialize()")
    toggle = body(impl, "AT25040B<Port>::checkWriteEnableToggle(uint8_t idle)")
    head = body(impl, "AT25040B<Port>::checkStableHead()")
    wait = body(impl, "AT25040B<Port>::waitIdle(bool started)")
    need(r"Validate\(config_\).*EnsureInitialized\(\).*parkWriteProtect\(\).*waitIdle\(false\)"
         r".*config_\.check_write_enable.*withWriteAccess.*checkStableHead", init, "start-up order")
    need(r"kStatusWen\)\) != 0U\) \{.*expectAfter\(reg::kWrdi, base\).*expectAfter\(reg::kWren.*expectAfter\(reg::kWrdi, base\)",
         toggle, "WRDI if set, WREN, WRDI")
    need(r"again\.value == status\.value", wait, "idle confirmed by a second read")
    wait_errors = [e for e in errors_in(wait) if not e.startswith("Bus") or e == "BusNoise"]
    start_errors = [e for e in wait_errors if e in ("DeviceIdMismatch", "NoDevice", "BusNoise")]
    steps = [
        {"title": "Check the Config, bring the bus up, park WP", "detail": "No transfer before Validate() passes",
         "errors": ["InvalidArgument"]},
        {"title": "(a) Status until idle, confirmed", "detail": "0xFF waits; bits 7 to 4 or RDY set: not a status",
         "errors": start_errors},
        {"title": "(b) WEN must follow WREN and WRDI", "detail": "WP released for it, parked after; nothing written",
         "errors": [e for e in ("DeviceIdMismatch", "BusNoise") if e in read(IMPL)]},
        {"title": f"or: the first {types['kIdentityReadBytes']} bytes, twice", "detail":
         "Only with check_write_enable = false (WP tied low)", "errors": errors_in(head)},
    ]
    return {"steps": steps, "faults": _faults(), "sources": [f"{IMPL}:initialize", f"{IMPL}:checkWriteEnableToggle",
                                                             f"{IMPL}:checkStableHead", f"{IMPL}:waitIdle",
                                                             f"{TESTS}:start-up fault tests"]}


def build(d):
    f = Fig(
        "identity-check",
        "The identity check, and what a broken bus looks like",
        alt=("Left: the steps of EnsureInitialized, the status read until idle and confirmed, the write enable "
             "toggle or the double read of the first bytes, each with its errors. Right: each bus fault from the "
             "tests with the status byte it shows and the error it returns."),
        width=1200,
        kicker="Contract",
        subtitle="Left: what EnsureInitialized() does. Right: the faults the tests inject, and what the driver says.",
        legend=[("brand", "start-up step"), ("amber", "fault on the bus or board"), ("rose", "error returned"),
                ("~solid", "next step")],
    )
    W = f.body_w
    lw = 460
    cards = []
    y = 0.0
    for s in d["steps"]:
        c = f.card(0, y, lw, s["title"], [s["detail"]], tone="brand", accent=True)
        cards.append(c)
        px = 0.0
        py = c.b + 8
        for e in s["errors"]:
            p = f.pill(px, py, e, tone="rose")
            px = p.r + 8
        y = py + f.k["pill"] + 10 + 34
    for a, b in zip(cards[:2], cards[1:3]):
        f.connect(a, b, "bottom", "top", at=0.92, bt=0.92)
    vx = lw + 100
    f.edge([(cards[1].r + 4, cards[1].cy), (vx, cards[1].cy), (vx, cards[3].cy), (cards[3].r + 4, cards[3].cy)],
           label="check_write_enable off", dashed=True)
    rx = lw + 210
    rw = W - rx
    ry = 0.0
    for fl in d["faults"]:
        if fl["status"] and fl["status"] not in fl["shows"]:
            raise SystemExit(f"figure source drift: test {fl['test']!r} sees {fl['status']}, not {fl['shows']!r}")
        c = f.card(rx, ry, rw, fl["fault"], [fl["shows"]], tone="amber", accent=True)
        f.pill(c.r - f.pill_width(fl["error"]) - 12, c.b - f.k["pill"] - 18, fl["error"], tone="rose")
        ry = c.b + 16
    f.note(0, max(y, ry) + 4, W,
           "A floating-low MISO reads status 0x00, which an idle chip also returns: only the write enable toggle "
           "tells them apart, which is why it is the default. With check_write_enable off, a dead bus passes.",
           tone="amber", title="Why the toggle")
    return f
