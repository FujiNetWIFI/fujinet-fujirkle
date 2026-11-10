#ifdef __WATCOMC__

#include <i86.h>
#include <dos.h>
#include <conio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../platform-specific/graphics.h"
#include "../misc.h"
#include "vars.h"

extern unsigned char charset[256][16];

#define VIDEO_RAM_ADDR     ((unsigned char far *)0xB8000000UL)
#define VIDEO_LINE_BYTES   80
#define VIDEO_ODD_OFFSET   0x2000

unsigned char far *video = VIDEO_RAM_ADDR;
unsigned char prevVideoMode;
unsigned char colorMode = 0;

static unsigned char screenBuffer[0x4000];
static bool          haveSavedScreen = false;

#define MASK_WHITE   0xFF
#define MASK_CYAN    0x55
/* MASK_ALT converts color-3 (white) tile pixels to a highlight color when
 * rendering text/cursors. With the new charset and 4-color palette
 * (0=blue, 1=cyan, 2=black, 3=white), alt text and cursor highlights
 * should be cyan, so MASK_ALT routes through color 1 (mask=0x55) — not
 * color 2 (which is reserved for the black dice dots). */
#define MASK_ALT     0x55

// Shared with the CoCo 3 build - the charsets carry the same game tiles at
// the same slots. Highlighting is a plot mask, not a second set of glyphs.
static const unsigned char diceChars[] = {
    /* Normal dice (s 1-6) */
    0x41,0x40,0x42, 0x40,0x45,0x40, 0x43,0x40,0x44,
    0x41,0x40,0x47, 0x40,0x40,0x40, 0x48,0x40,0x44,
    0x41,0x40,0x47, 0x40,0x45,0x40, 0x48,0x40,0x44,
    0x46,0x40,0x47, 0x40,0x40,0x40, 0x48,0x40,0x49,
    0x46,0x40,0x47, 0x40,0x45,0x40, 0x48,0x40,0x49,
    0x46,0x40,0x47, 0x4A,0x40,0x4B, 0x48,0x40,0x49,

    /* Kept dice (s 1-6 with isSelected) */
    0x21,0x4C,0x22, 0x20,0x25,0x20, 0x23,0x4D,0x24,
    0x21,0x4C,0x27, 0x20,0x20,0x20, 0x28,0x4D,0x24,
    0x21,0x4C,0x27, 0x20,0x25,0x20, 0x28,0x4D,0x24,
    0x26,0x4C,0x27, 0x20,0x20,0x20, 0x28,0x4D,0x29,
    0x26,0x4C,0x27, 0x20,0x25,0x20, 0x28,0x4D,0x29,
    0x26,0x4C,0x27, 0x2A,0x20,0x2B, 0x28,0x4D,0x29,

    /* DIE_BANK */
    0x41,0x40,0x42, 0x7D,0x7E,0x7F, 0x43,0x40,0x44,

    /* DIE_ROLL */
    0x41,0x40,0x42, 0x2C,0x2D,0x2E, 0x43,0x40,0x44,
};

static void plot_raw(unsigned char x, unsigned char y, const unsigned char *tile)
{
    unsigned char i;
    unsigned char col = x << 1;
    unsigned char ybase = y << 3;
    for (i = 0; i < 8; i++) {
        unsigned char r  = ybase + i;
        unsigned char rh = r >> 1;
        unsigned int  ro = (unsigned int)rh * VIDEO_LINE_BYTES + col;
        if (r & 1) ro += VIDEO_ODD_OFFSET;
        video[ro]     = tile[i*2];
        video[ro + 1] = tile[i*2 + 1];
    }

}

/* Plot a charset glyph through a color mask: MASK_WHITE draws it as stored,
 * 0x55 turns its white pixels cyan. */
static void plot_glyph(unsigned char x, unsigned char y, unsigned char idx, unsigned char mask)
{
    unsigned char tile[16];
    unsigned char i;
    if (mask == MASK_WHITE) {
        plot_raw(x, y, charset[idx]);
        return;
    }
    for (i = 0; i < 16; i++)
        tile[i] = charset[idx][i] & mask;
    plot_raw(x, y, tile);
}

/* PIT based frame pacing. The CGA retrace poll is unreliable under qemu, which
 * does not emulate the status port duty cycle, so animations freeze or run
 * wild. Counter 0 is dependable on both. */
unsigned int pitTicksPerMs = 0;

unsigned int pitRead(void)
{
    unsigned int t;

    outp(0x43, 0x00);                /* latch counter 0 */
    t  = (unsigned int)inp(0x40);
    t |= (unsigned int)inp(0x40) << 8;
    return t;
}

/* Counter 0 is crystal driven, so waits off it need no cpu speed scaling. The
 * BIOS mode does vary though: mode 3 drops the count by two per clock, halving
 * every interval. QEMU leaves mode 2, real BIOSes usually mode 3. Measured
 * against the BIOS tick at 0040:006C, which is 18.2Hz whatever the mode. */
void calibratePit(void)
{
    volatile unsigned long far *biosTick;
    unsigned long units, mark;
    unsigned int prev, now, perMs;

    biosTick = (volatile unsigned long far *)MK_FP(0x40, 0x6C);

    mark = *biosTick;
    while (*biosTick == mark)        /* start on a fresh tick */
        ;

    mark = *biosTick;
    units = 0;
    prev = pitRead();
    while (*biosTick == mark) {
        now = pitRead();
        units += (unsigned int)(prev - now);
        prev = now;
    }

    perMs = (unsigned int)((units * 1000UL) / 54925UL);

    /* ~1193 in mode 2, ~2386 in mode 3; anything else means it went wrong */
    pitTicksPerMs = (perMs >= 900 && perMs <= 3000) ? perMs : 1193;
}

void waitvsync()
{
    static unsigned int last_pit = 0;
    unsigned int now, elapsed, frame;

    if (!pitTicksPerMs)
        calibratePit();

    frame = (unsigned int)(((unsigned long)pitTicksPerMs * 50UL) / 3UL);  /* 16.67ms */

    do {
        now = pitRead();
        elapsed = (unsigned int)(last_pit - now);
    } while (elapsed < frame);
    last_pit = now;
}

void initGraphics()
{
    union REGS r;

    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    prevVideoMode = r.h.al;

    /* CGA mode 5 320x200, colors set by the VGA remap below. */
    r.h.ah = 0x00;
    r.h.al = 0x05;
    int86(0x10, &r, &r);

    /* Bright CGA palette 1; the VGA remap below overrides its colors. */
    outp(0x3D9, 0x30);

    /* VGA palette remap (color 0 and 2 swapped from the original Atari
     * mapping): table background is now blue, blue-shadow elements are
     * black. Dice pips/text and Roll-button text get re-encoded to use
     * color 2 in the tile data so they still render BLACK. */
    r.h.ah = 0x10; r.h.al = 0x00;
    r.h.bl = 0x00; r.h.bh = 0x01;     /* color 0 -> blue (table bg) */
    int86(0x10, &r, &r);

    r.h.ah = 0x10; r.h.al = 0x00;
    r.h.bl = 0x01; r.h.bh = 0x0B;     /* color 1 -> cyan */
    int86(0x10, &r, &r);

    r.h.ah = 0x10; r.h.al = 0x00;
    r.h.bl = 0x02; r.h.bh = 0x00;     /* color 2 -> black (was blue) */
    int86(0x10, &r, &r);

    r.h.ah = 0x10; r.h.al = 0x00;
    r.h.bl = 0x03; r.h.bh = 0x0F;     /* color 3 -> white */
    int86(0x10, &r, &r);
}

void resetGraphics()
{
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = prevVideoMode ? prevVideoMode : 0x03;
    int86(0x10, &r, &r);
}

void resetScreen(bool forBorderScreen)
{
    waitvsync();
    if (!forBorderScreen) {
        _fmemset(&video[0x0000], 0, 0x2000);
        _fmemset(&video[0x2000], 0, 0x2000);
    } else {
        /* Clear interior, leave dice corners drawn at the four edges. */
        unsigned char r;
        for (r = 3; r < HEIGHT - 3; r++) {
            unsigned char rh = (r << 3) >> 1;
            unsigned int  o  = (unsigned int)rh * VIDEO_LINE_BYTES;
            unsigned char i;
            for (i = 0; i < 8; i++) {
                _fmemset(&video[o + (i >> 1) * VIDEO_LINE_BYTES + ((i & 1) ? VIDEO_ODD_OFFSET : 0)],
                         0, VIDEO_LINE_BYTES);
            }
        }
        for (r = 0; r < 3; r++) {
            unsigned char x;
            for (x = 3; x < WIDTH - 3; x++) {
                plot_glyph(x, r,            0x00, MASK_WHITE);
                plot_glyph(x, HEIGHT-1-r,   0x00, MASK_WHITE);
            }
        }
    }
}

/* drawText: lowercase strings render in white. Atari shifts '0'-'9' down
 * by 32 to land on the digit slots at 0x10-0x19 - we follow that. */
void drawText(unsigned char x, unsigned char y, char *s)
{
    unsigned char c;
    while ((c = (unsigned char)*s++)) {
        if (c >= 32 && c < 65) c -= 32;
        plot_glyph(x++, y, c, MASK_WHITE);
    }
}

/* drawTextAlt: matches the Atari semantics. Uppercase letters render as
 * plain lowercase glyphs in the normal (white) color; lowercase letters,
 * digits, and punctuation render in the alt (highlight) color. */
void drawTextAlt(unsigned char x, unsigned char y, char *s)
{
    unsigned char c;
    while ((c = (unsigned char)*s++)) {
        unsigned char mask = MASK_ALT;
        if (c >= 32 && c < 65) c -= 32;
        if (c >= 'A' && c <= 'Z') {
            c += 32;
            mask = MASK_WHITE;
        }
        plot_glyph(x++, y, c, mask);
    }
}

void drawChar(unsigned char x, unsigned char y, char c, unsigned char alt)
{
    unsigned char ch = (unsigned char)c;
    if (ch >= 32 && ch < 65) ch -= 32;
    if (alt && ch >= 'A' && ch <= 'Z') ch += 32;
    plot_glyph(x, y, ch, alt ? MASK_ALT : MASK_WHITE);
}

void drawIcon(unsigned char x, unsigned char y, unsigned char icon)
{
    plot_glyph(x, y, icon, MASK_WHITE);
}

void drawBlank(unsigned char x, unsigned char y)
{
    plot_glyph(x, y, 0x00, MASK_WHITE);
}

void drawSpace(unsigned char x, unsigned char y, unsigned char w)
{
    while (w--) drawBlank(x++, y);
}

void drawConnectionIcon(unsigned char x, unsigned char y)
{
    plot_glyph(x,   y, 0x03, MASK_WHITE);
    plot_glyph(x+1, y, 0x04, MASK_WHITE);
}

void drawLine(unsigned char x, unsigned char y, unsigned char w)
{
    while (w--) plot_glyph(x++, y, 0x52, MASK_WHITE);
}

void drawBox(unsigned char x, unsigned char y, unsigned char w, unsigned char h)
{
    unsigned char i;
    plot_glyph(x,         y, 0x51, MASK_WHITE);
    for (i = 0; i < w; i++) plot_glyph(x+1+i, y, 0x52, MASK_WHITE);
    plot_glyph(x+w+1,     y, 0x55, MASK_WHITE);

    for (i = 0; i < h; i++) {
        plot_glyph(x,       y+1+i, 0x7C, MASK_WHITE);
        plot_glyph(x+w+1,   y+1+i, 0x7C, MASK_WHITE);
    }

    plot_glyph(x,         y+h+1, 0x57, MASK_WHITE);
    for (i = 0; i < w; i++) plot_glyph(x+1+i, y+h+1, 0x52, MASK_WHITE);
    plot_glyph(x+w+1,     y+h+1, 0x5A, MASK_WHITE);
}

void drawDie(unsigned char x, unsigned char y, unsigned char s, bool isSelected, bool isHighlighted)
{
    const unsigned char *src;
    unsigned char r, c, mask;
    if (!s || s > DIE_ROLL) return;

    src = diceChars + (s - 1) * 9;
    // Only the six faces have a selected variant; the button faces sit past it
    if (isSelected && s <= 6) src += 54;
    mask = isHighlighted ? MASK_ALT : MASK_WHITE;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            plot_glyph(x + c, y + r, *src++, mask);
}

/* 2-pixel-thick checkerboard cyan(1)/black(2) frame around the die with
 * a 1-pixel gap. The 30x30 frame sits one pixel outside the die's 24x24
 * footprint on every side. Outer corners are 1-pixel rounded by
 * skipping the four corner pixels of the outermost row/col. Color at
 * each pixel is ((px + py) & 1) ? black : cyan, so the two layers of
 * each edge are phase-flipped. The 220 touched pixels' original 2-bit
 * colors are saved at draw time so hideDiceCursor can put the screen
 * back exactly. */
#define DICE_ROW          18
#define CKR(px, py)       ((((px) + (py)) & 1) ? 2 : 1)
static unsigned char diceCursorX = 0;
static bool diceCursorActive = false;

static void set_pixel_2bpp(unsigned int px, unsigned int py, unsigned char color)
{
    unsigned char shift = (3 - (px & 3)) << 1;
    unsigned char m     = 0x03 << shift;
    unsigned int  ro    = (unsigned int)(py >> 1) * VIDEO_LINE_BYTES + (px >> 2);
    if (py & 1) ro += VIDEO_ODD_OFFSET;
    video[ro] = (video[ro] & ~m) | ((color & 0x03) << shift);
}

void drawDiceCursor(unsigned char x)
{
    unsigned int fL, fR, fT, fB, i;

    if (diceCursorActive) hideDiceCursor(diceCursorX);
    diceCursorX = x;

    fL = ((unsigned int)x << 3) - 3;
    fR = ((unsigned int)x << 3) + 26;
    fT = ((unsigned int)DICE_ROW << 3) - 3;
    fB = ((unsigned int)DICE_ROW << 3) + 26;

    /* Outer rows inset one pixel at each end, which rounds the corners. */
    for (i = 1; i < 29; i++) set_pixel_2bpp(fL + i, fT,     CKR(fL + i, fT));
    for (i = 0; i < 30; i++) set_pixel_2bpp(fL + i, fT + 1, CKR(fL + i, fT + 1));
    for (i = 0; i < 30; i++) set_pixel_2bpp(fL + i, fB - 1, CKR(fL + i, fB - 1));
    for (i = 1; i < 29; i++) set_pixel_2bpp(fL + i, fB,     CKR(fL + i, fB));
    for (i = 2; i < 28; i++) set_pixel_2bpp(fL,     fT + i, CKR(fL,     fT + i));
    for (i = 2; i < 28; i++) set_pixel_2bpp(fL + 1, fT + i, CKR(fL + 1, fT + i));
    for (i = 2; i < 28; i++) set_pixel_2bpp(fR - 1, fT + i, CKR(fR - 1, fT + i));
    for (i = 2; i < 28; i++) set_pixel_2bpp(fR,     fT + i, CKR(fR,     fT + i));

    diceCursorActive = true;
}

void hideDiceCursor(unsigned char x)
{
    unsigned int fL, fR, fT, fB, i;

    (void)x;
    if (!diceCursorActive) return;

    /* Erase where it was last drawn - the caller's x can drift. */
    fL = ((unsigned int)diceCursorX << 3) - 3;
    fR = ((unsigned int)diceCursorX << 3) + 26;
    fT = ((unsigned int)DICE_ROW << 3) - 3;
    fB = ((unsigned int)DICE_ROW << 3) + 26;

    for (i = 1; i < 29; i++) set_pixel_2bpp(fL + i, fT,     0);
    for (i = 0; i < 30; i++) set_pixel_2bpp(fL + i, fT + 1, 0);
    for (i = 0; i < 30; i++) set_pixel_2bpp(fL + i, fB - 1, 0);
    for (i = 1; i < 29; i++) set_pixel_2bpp(fL + i, fB,     0);
    for (i = 2; i < 28; i++) set_pixel_2bpp(fL,     fT + i, 0);
    for (i = 2; i < 28; i++) set_pixel_2bpp(fL + 1, fT + i, 0);
    for (i = 2; i < 28; i++) set_pixel_2bpp(fR - 1, fT + i, 0);
    for (i = 2; i < 28; i++) set_pixel_2bpp(fR,     fT + i, 0);

    diceCursorActive = false;
}

/* Cells all the way down here, so a die is exactly three text rows. */
void drawDieSpace(unsigned char x, unsigned char y)
{
    drawSpace(x, y,     3);
    drawSpace(x, y + 1, 3);
    drawSpace(x, y + 2, 3);
}

/* 0x5B and 0x58 are the top and bottom tees; the verticals are the same 0x7C
 * the box sides use. */
void drawBoxDivider(unsigned char x, unsigned char y, unsigned char h)
{
    unsigned char i;

    plot_glyph(x, y, 0x5B, MASK_WHITE);
    for (i = 0; i < h; i++) plot_glyph(x, y + 1 + i, 0x7C, MASK_WHITE);
    plot_glyph(x, y + h + 1, 0x58, MASK_WHITE);
}

/* 40 columns has room to space the tiles apart, so the major split needs no
 * extra weight. */
void drawBoxDividerWide(unsigned char x, unsigned char y, unsigned char h)
{
    drawBoxDivider(x, y, h);
}

bool saveScreenBuffer()
{
    _fmemcpy(screenBuffer, VIDEO_RAM_ADDR, 0x4000);
    haveSavedScreen = true;
    return true;
}

void restoreScreenBuffer()
{
    if (!haveSavedScreen) return;
    waitvsync();
    _fmemcpy(VIDEO_RAM_ADDR, screenBuffer, 0x4000);
    haveSavedScreen = false;
}

unsigned char cycleNextColor() { return 0; }
void          setColorMode()   { }

#endif /* __WATCOMC__ */
