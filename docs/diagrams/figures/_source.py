"""Shared helpers for the AT25040B figures: read constants, function bodies and tests from the repository.

Every helper raises when the code no longer looks the way a figure expects, so a figure fails to build instead of
drawing something the code does not do.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
REGISTERS = "inc/at25040b_registers.hpp"
SPI = "inc/at25040b_spi_interface.hpp"
TYPES = "inc/at25040b_types.hpp"
UMBRELLA = "inc/at25040b.hpp"
IMPL = "src/at25040b.ipp"
TESTS = "tests/test_at25040b.cpp"


def read(rel: str) -> str:
    return (ROOT / rel).read_text()


def need(pattern: str, text: str, what: str, flags: int = re.S) -> re.Match:
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"figure source drift: {what} not found (pattern {pattern!r})")
    return m


def _value(expr: str, known: dict[str, int]) -> int:
    expr = expr.strip()
    expr = re.sub(r"(0x[0-9A-Fa-f']+|\d+)U", r"\1", expr).replace("'", "")
    for name in sorted(known, key=len, reverse=True):
        expr = re.sub(rf"\b{name}\b", str(known[name]), expr)
    if not re.fullmatch(r"[0-9xA-Fa-f<>|&~() *+-]+", expr):
        raise SystemExit(f"figure source drift: cannot evaluate constant {expr!r}")
    return int(eval(expr))  # noqa: S307 - digits and operators only, checked above


def constants(rel: str) -> dict[str, int]:
    """``inline constexpr`` integer constants of a header, by name (``kRdsr`` -> 5)."""
    out: dict[str, int] = {}
    for name, expr in re.findall(r"inline constexpr (?:u?int\d+_t|std::size_t) (k\w+) =\s*([^;]+);", read(rel)):
        out[name] = _value(expr, out)
    return out


def names(rel: str) -> dict[str, str]:
    """The datasheet name at the start of each constant's comment (``kRdsr`` -> ``RDSR``)."""
    return dict(re.findall(r"inline constexpr \w+ (k\w+) = [^;]+;\s*///< ([A-Z][A-Za-z0-9_]*):", read(rel)))


def body(text: str, signature: str) -> str:
    """The body of the function whose definition contains ``signature``, braces matched."""
    start = need(re.escape(signature) + r"[^{;]*\{", text, signature).end()
    depth, i = 1, start
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[start : i - 1]


def errors_in(text: str) -> list[str]:
    """``Error::`` values a piece of code returns, in order, without repeats."""
    out: list[str] = []
    for e in re.findall(r"Error::(\w+)", text):
        if e not in out and e != "Ok":
            out.append(e)
    return out


def test_cases(rel: str = TESTS) -> list[tuple[str, str]]:
    """(name, body) of every ``TEST_CASE`` in a test file."""
    text = read(rel)
    out = []
    for m in re.finditer(r'TEST_CASE\("((?:[^"\\]|\\.)*)"\)\s*\{', text):
        out.append((m.group(1), body(text[m.start():], m.group(0)[:-1])))
    return out
