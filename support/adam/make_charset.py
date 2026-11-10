#!/usr/bin/env python3
"""Generate src/adam/charset.c for the Coleco Adam (TMS9918) port.

Sources:
  - src/coco/charset.c: the CoCo 1/2 1bpp charset (97 glyphs, 0x00-0x60,
    8 bytes each) - dice tiles 0x00-0x1A, ROLL counters 0x0E-0x10/0x15/0x1A/
    0x1F, icons in the punctuation slots, box glyphs 0x3B-0x3F, connection
    icon 0x5C/0x5D, BANK 0x5E-0x60. TMS mode 2 adds a per-row color byte.
  - src/msdos/charset.c: the CGA 2bpp namco font (256 glyphs, 16 bytes
    each; ASCII 32-64 live at index-32, A-Z at their ASCII codes). The
    text glyphs - digits, A-Z and font punctuation - come from here,
    crisper than the CoCo block font; pattern bit = any nonzero pixel.

The kept-dice clamp rows (0x55/0x54 stripes - artifact blue on the CoCo)
are replaced by solid bars colored cyan, a contrasting clamp around a
held die instead of a white dither.

Glyphs 0x61-0x67 are synthesized board chrome that replaces the CoCo's
pixel-level hires_Mask line drawing (see src/adam/graphics.c):
  0x61  vertical divider (CoCo mask 0x20)
  0x62  thick horizontal rule, top of cell (drawLine / box top)
  0x63  box bottom edge (stroke on rows 3-4, joining corners 0x3D/0x3E)
  0x64  divider meeting the box top: the top tee
  0x65  divider ending in the box bottom edge: the bottom tee
  0x66  dice cursor bar, left of die
  0x67  dice cursor bar, right of die

The accent bank at glyph 0x80+ reuses the same patterns (initGraphics
uploads the pattern array twice) with light-blue colors - the Adam
equivalent of the CoCo's ROP_BLUE, selected by adding 0x80 to a glyph
(drawTextAlt, highlighted buttons).

Run from the repo root:  python3 support/adam/make_charset.py
The generated src/adam/charset.c is committed; tweak this generator and
regenerate rather than hand-editing the output.
"""

import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
COCO_CHARSET = os.path.join(REPO, "src", "coco", "charset.c")
MSDOS_CHARSET = os.path.join(REPO, "src", "msdos", "charset.c")
OUT = os.path.join(REPO, "src", "adam", "charset.c")

# TMS9918 colors
BLACK = 1
LBLUE = 5
CYAN = 7
DYELLOW = 10
WHITE = 15

COCO_GLYPHS = 0x61
GLYPH_COUNT = 0x68

# Gold like the CoCo's artifact-orange box lines (and the CoCo3 gold grid)
GOLD_GLYPHS = {0x3B, 0x3C, 0x3D, 0x3E, 0x3F} | set(range(0x61, 0x66))
BLUE_GLYPHS = {0x66, 0x67}

# Text glyphs replaced with the MS-DOS namco font: adam glyph -> msdos index.
# The letterforms live at the msdos *lowercase* slots (its caps slots hold
# dice art; ASCII case only selects color there). '-', '/' and ':' are
# blank in the msdos font, so those stay CoCo.
MSDOS_TEXT = {g: g - 32 for g in [0x21, 0x27, 0x2C, 0x2E] + list(range(0x30, 0x3A))}
MSDOS_TEXT.update({g: g + 0x20 for g in range(0x41, 0x5B)})

# Kept-dice glyphs whose striped clamp rows become solid cyan bars
KEPT_GLYPHS = {0x0C, 0x0D, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x19}
STRIPE_ROWS = {0x55, 0x54}

V_LINE = [0x20] * 8         # CoCo vertical divider mask
H_THICK = [0xFF, 0xFF, 0, 0, 0, 0, 0, 0]
H_BOTTOM = [0, 0, 0, 0xFF, 0xFF, 0, 0, 0]


def union(a, b):
    return [x | y for x, y in zip(a, b)]


def bar_until(bar, stroke):
    """Vertical bar from the top that stops where the stroke starts."""
    out = list(stroke)
    for r in range(8):
        if stroke[r]:
            break
        out[r] |= bar[r]
    return out


def parse_hex_array(path, marker):
    body = open(path).read().split(marker, 1)[1].split("};", 1)[0]
    # Comments in the CoCo charset name glyph ranges ("0x5e-0x60"), which the
    # byte pattern would otherwise pick up as data
    body = re.sub(r"//[^\n]*", "", body)
    return [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]{2}", body)]


def decode_msdos_glyph(data, idx):
    """CGA 2bpp 8x8 -> 1bpp pattern (bit = nonzero pixel)."""
    pattern = []
    for r in range(8):
        b0, b1 = data[idx * 16 + r * 2], data[idx * 16 + r * 2 + 1]
        bits = 0
        for p in range(4):
            if (b0 >> (6 - 2 * p)) & 3:
                bits |= 0x80 >> p
            if (b1 >> (6 - 2 * p)) & 3:
                bits |= 0x08 >> p
        pattern.append(bits)
    return pattern


def main():
    data = parse_hex_array(COCO_CHARSET, "charset[] = {")
    assert len(data) == COCO_GLYPHS * 8, "expected 97 CoCo glyphs, got %d bytes" % len(data)
    font = parse_hex_array(MSDOS_CHARSET, "charset[256][16] =")
    assert len(font) == 256 * 16, "expected 256 MS-DOS glyphs, got %d bytes" % len(font)

    patterns = [data[g * 8:g * 8 + 8] for g in range(COCO_GLYPHS)]

    for g, idx in MSDOS_TEXT.items():
        patterns[g] = decode_msdos_glyph(font, idx)

    patterns.append(V_LINE)                        # 0x61
    patterns.append(H_THICK)                       # 0x62
    patterns.append(H_BOTTOM)                      # 0x63
    patterns.append(union(V_LINE, H_THICK))        # 0x64
    patterns.append(bar_until(V_LINE, H_BOTTOM))   # 0x65
    patterns.append([0x03] * 8)                    # 0x66
    patterns.append([0xC0] * 8)                    # 0x67
    assert len(patterns) == GLYPH_COUNT

    def fg(g):
        if g in BLUE_GLYPHS:
            return LBLUE
        if g in GOLD_GLYPHS:
            return DYELLOW
        return WHITE

    colors = [[(fg(g) << 4) | BLACK] * 8 for g in range(GLYPH_COUNT)]
    alt_colors = [[(LBLUE << 4) | BLACK] * 8 for _ in range(GLYPH_COUNT)]

    # Kept-dice clamps: solid cyan bars instead of white stripes
    for g in KEPT_GLYPHS:
        for r in range(8):
            if patterns[g][r] in STRIPE_ROWS:
                patterns[g][r] = 0xFF
                colors[g][r] = (CYAN << 4) | BLACK

    def emit(name, rows):
        flat = [b for glyph in rows for b in glyph]
        lines = []
        for i in range(0, len(flat), 8):
            lines.append("    " + ", ".join("0x%02X" % b for b in flat[i:i + 8]) + ",")
        return "const uint8_t %s[%d] = {\n%s\n};\n" % (name, len(flat), "\n".join(lines))

    with open(OUT, "w") as f:
        f.write("/* GENERATED by support/adam/make_charset.py - do not hand-edit\n"
                "   wholesale; tweak the generator and regenerate. */\n\n"
                "#include <stdint.h>\n\n")
        f.write("/* Glyphs 0x00-0x67: CoCo 1/2 charset + synthesized board chrome.\n"
                "   The same patterns are loaded again at 0x80 with altColors. */\n")
        f.write(emit("charsetPatterns", patterns))
        f.write("\n")
        f.write(emit("charsetColors", colors))
        f.write("\n/* Light-blue accent colors for the 0x80+ bank */\n")
        f.write(emit("altColors", alt_colors))
    print("wrote", OUT, "(%d glyphs)" % GLYPH_COUNT)


if __name__ == "__main__":
    main()
