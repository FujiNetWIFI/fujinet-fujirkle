#ifdef __TRS80M2__

/*
  Graphics - TRS-80 Model II

  Fujitzee's Model II console drawing: the heavy line set, reverse video, 7x3
  dice. Drawing goes into console.c's back buffer and goes to the screen at
  waitvsync(), when input is read, and before every FujiNet call.
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "console.h"
#include "../platform-specific/graphics.h"

// Line directions, for line()
#define LN_U 1
#define LN_D 2
#define LN_L 4
#define LN_R 8

#define DIE_W 7
#define DIE_H 3
#define BTN_W 6

// The bordered screens' corner dice
#define CORNER_RIGHT (WIDTH - DIE_W)
#define CORNER_BOTTOM (HEIGHT - DIE_H)

#define PIP 'o'

unsigned char colorMode = 0;

// Pips on a 3x3 grid, bit r*3+c, shown in columns 1, 3 and 5
static const uint16_t pipMask[6] = {
    0x010, // 1: center
    0x044, // 2: top right, bottom left
    0x054, // 3
    0x145, // 4: the corners
    0x155, // 5
    0x16D, // 6: two columns of three
};

// The line drawing character for each combination of directions
static const uint8_t lineGlyph[16] = {
    ' ',         G_VLINE,     G_VLINE,    G_VLINE,
    G_HLINE,     G_CORNER_LR, G_CORNER_UR, G_TEE_LEFT,
    G_HLINE,     G_CORNER_LL, G_CORNER_UL, G_TEE_RIGHT,
    G_HLINE,     G_TEE_UP,    G_TEE_DOWN, G_CROSS};

// The directions of each graphics character (screen codes 0x00-0x1F), 0 for
// those that are not line drawing
static const uint8_t glyphBits[32] = {
    LN_D | LN_R,               // G_CORNER_UL
    LN_D | LN_L,               // G_CORNER_UR
    LN_U | LN_L,               // G_CORNER_LR
    LN_U | LN_R,               // G_CORNER_LL
    LN_D | LN_L | LN_R,        // G_TEE_DOWN
    LN_U | LN_D | LN_L,        // G_TEE_LEFT
    LN_U | LN_L | LN_R,        // G_TEE_UP
    LN_U | LN_D | LN_R,        // G_TEE_RIGHT
    0, 0, 0, 0,
    LN_U | LN_D | LN_L | LN_R, // G_CROSS
    0, 0, 0, 0, 0, 0, 0,
    LN_U | LN_D,               // G_VLINE
    0,
    LN_L | LN_R,               // G_HLINE
    0, 0, 0, 0, 0, 0, 0, 0, 0};

// The dice cursor, and the width it boxes
static bool cursorShown, savedCursorShown;
static uint8_t cursorX, savedCursorX, cursorW, savedCursorW;

/// @brief Draw a piece of line, joining any line already in the cell
static void line(uint8_t x, uint8_t y, uint8_t bits)
{
    static uint8_t c;

    c = peek(x, y);
    if (c < 0x20)
        bits |= glyphBits[c];
    put(x, y, lineGlyph[bits]);
}

/*
  Lines are drawn as a set of straight segments at once, a row at a time:
  every segment through a cell adds its directions, so corners and junctions
  come out whole. (A line end stored on its own would be a plain line, and
  read back as one.) Only what was on screen before is joined cell by cell.
*/
#define SEG_MAX 4

typedef struct
{
    uint8_t from, to, at; // across: x from-to on row at; down: y from-to in column at
} Seg;

static Seg acrossSeg[SEG_MAX], downSeg[SEG_MAX];
static uint8_t acrossCount, downCount;
static uint8_t rowBits[WIDTH];
static uint8_t segLeft = WIDTH - 1, segRight; // the columns the segments span

static void segSpan(uint8_t x0, uint8_t x1)
{
    if (x0 < segLeft)
        segLeft = x0;
    if (x1 > segRight)
        segRight = x1;
}

static void across(uint8_t x0, uint8_t x1, uint8_t y)
{
    if (x1 >= WIDTH)
        x1 = WIDTH - 1;
    acrossSeg[acrossCount].from = x0;
    acrossSeg[acrossCount].to = x1;
    acrossSeg[acrossCount].at = y;
    acrossCount++;
    segSpan(x0, x1);
}

static void down(uint8_t x, uint8_t y0, uint8_t y1)
{
    if (x >= WIDTH)
        return;
    downSeg[downCount].from = y0;
    downSeg[downCount].to = y1;
    downSeg[downCount].at = x;
    downCount++;
    segSpan(x, x);
}

/// @brief Draw the segments given since the last call, rows y0-y1
static void drawSegs(uint8_t y0, uint8_t y1)
{
    static uint8_t x, y, i, n;
    static Seg *g;

    n = segRight - segLeft + 1;
    for (y = y0; y <= y1; y++)
    {
        memset(rowBits + segLeft, 0, n);
        for (i = 0; i < acrossCount; i++)
        {
            g = &acrossSeg[i];
            if (g->at != y)
                continue;
            for (x = g->from; x <= g->to; x++)
                rowBits[x] |= (x > g->from ? LN_L : 0) | (x < g->to ? LN_R : 0);
        }
        for (i = 0; i < downCount; i++)
        {
            g = &downSeg[i];
            if (y < g->from || y > g->to)
                continue;
            rowBits[g->at] |= (y > g->from ? LN_U : 0) | (y < g->to ? LN_D : 0);
        }
        for (x = segLeft; x <= segRight; x++)
            if (rowBits[x])
                line(x, y, rowBits[x]);
    }
    acrossCount = downCount = 0;
    segLeft = WIDTH - 1;
    segRight = 0;
}

/*
 * Text
 */

/// @brief The screen code for a character of text: capitals, and a space
/// for anything the screen can't show
static uint8_t textCode(uint8_t c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c < 0x20 || c > 0x7E)
        return ' ';
    return c;
}

/// @brief Is a letter or digit of alternate text in the alternate style?
/// Capitals are shown plain, the rest in reverse video.
static bool altStyle(uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

void drawText(unsigned char x, unsigned char y, char *s)
{
    static uint8_t c;

    while ((c = (uint8_t)*s++) && x < WIDTH)
        put(x++, y, textCode(c));
}

void drawTextAlt(unsigned char x, unsigned char y, char *s)
{
    static uint8_t c;
    static bool rev, prevRev;

    // A space is reversed only between two reversed letters or digits, so
    // headings become one bar, words keep their gaps and padding stays
    // blank; punctuation goes with what it follows
    prevRev = false;
    while ((c = (uint8_t)*s++) && x < WIDTH)
    {
        if (c == ' ')
            rev = prevRev && altStyle((uint8_t)*s);
        else if ((c >= 'A' && c <= 'Z') || altStyle(c))
            rev = altStyle(c);
        else
            rev = prevRev;
        put(x++, y, textCode(c) | (rev ? REV : 0));
        prevRev = rev;
    }
}

void drawChar(unsigned char x, unsigned char y, char c, unsigned char alt)
{
    put(x, y, textCode((uint8_t)c) | (alt ? REV : 0));
}

void drawIcon(unsigned char x, unsigned char y, unsigned char icon)
{
    put(x, y, icon);
}

void drawBlank(unsigned char x, unsigned char y)
{
    put(x, y, ' ');
}

void drawSpace(unsigned char x, unsigned char y, unsigned char w)
{
    fill(x, y, w, ' ');
}

void drawLine(unsigned char x, unsigned char y, unsigned char w)
{
    while (w--)
        line(x++, y, LN_L | LN_R);
}

void drawBox(unsigned char x, unsigned char y, unsigned char w, unsigned char h)
{
    static uint8_t r;

    across(x, x + w + 1, y);
    across(x, x + w + 1, y + h + 1);
    down(x, y, y + h + 1);
    down(x + w + 1, y, y + h + 1);
    drawSegs(y, y + h + 1);
    for (r = 1; r <= h; r++)
        fill(x + 1, y + r, w, ' ');
}

void drawBoxDivider(unsigned char x, unsigned char y, unsigned char h)
{
    down(x, y, y + h + 1);
    drawSegs(y, y + h + 1);
}

// One column is heavy enough on the 80-column strip
void drawBoxDividerWide(unsigned char x, unsigned char y, unsigned char h)
{
    drawBoxDivider(x, y, h);
}

void drawConnectionIcon(unsigned char x, unsigned char y)
{
    put(x, y, REV | 'X');
    put(x + 1, y, REV | '?');
}

/*
 * Dice
 */

/// @brief A 6x3 button: the word between blank rows, reversed until lit
static void buttonAt(uint8_t x, uint8_t y, const char *word, bool isHighlighted)
{
    static uint8_t r, c, code;

    for (r = 0; r < DIE_H; r++)
        for (c = 0; c < BTN_W; c++)
        {
            code = (r == 1 && c >= 1 && c <= 4) ? (uint8_t)word[c - 1] : ' ';
            put(x + c, y + r, code | (isHighlighted ? 0 : REV));
        }
}

/// @brief A 7x3 die: free dice solid, kept ones boxed so stacked ones stay apart
static void dieAt(uint8_t x, uint8_t y, uint8_t s, bool isSelected, bool isHighlighted)
{
    static uint8_t r, c, code, flip, rowPips, bit;
    static uint16_t mask;

    flip = isHighlighted ? REV : 0;
    mask = pipMask[s - 1];

    for (r = 0; r < DIE_H; r++)
    {
        // This row's three pip places, in bits 1, 3 and 5 for columns 1, 3, 5
        rowPips = (uint8_t)((mask & 7) << 1);
        rowPips = (rowPips & 2) | ((rowPips & 4) << 1) | ((rowPips & 8) << 2);
        mask >>= 3;
        bit = 1;
        for (c = 0; c < DIE_W; c++, bit <<= 1)
        {
            if (rowPips & bit)
                code = PIP;
            else if (!isSelected)
                code = ' ';
            else if (c == 0)
                code = r == 0 ? G_CORNER_UL : r == DIE_H - 1 ? G_CORNER_LL : G_VLINE;
            else if (c == DIE_W - 1)
                code = r == 0 ? G_CORNER_UR : r == DIE_H - 1 ? G_CORNER_LR : G_VLINE;
            else
                code = ' ';
            if (!isSelected)
                code |= REV;

            put(x + c, y + r, code ^ flip);
        }
    }
}

void drawDie(unsigned char x, unsigned char y, unsigned char s, bool isSelected, bool isHighlighted)
{
    if (s == DIE_ROLL)
    {
        buttonAt(x, y, "ROLL", isHighlighted);
        return;
    }
    if (s == DIE_BANK)
    {
        buttonAt(x, y, "BANK", isHighlighted);
        return;
    }
    if (s < 1 || s > 6)
        return;

    // The bordered screens' corner dice, placed for 3x3 ones
    if (x == WIDTH - 3)
        x = CORNER_RIGHT;
    dieAt(x, y, s, isSelected, isHighlighted);
}

void drawDieSpace(unsigned char x, unsigned char y)
{
    static uint8_t r;

    for (r = 0; r < DIE_H; r++)
        fill(x, y + r, DIE_W, ' ');
}

static void cursorCells(uint8_t x, uint8_t top, uint8_t mid, uint8_t bottom)
{
    put(x - 1, DICE_Y, top);
    put(x - 1, DICE_Y + 1, mid);
    put(x - 1, DICE_Y + 2, bottom);
}

void drawDiceCursor(unsigned char x)
{
    hideDiceCursor(x);

    // [ ] in the gap columns either side of the die or button
    cursorW = (x == BTN_ROLL_X || x == BTN_BANK_X) ? BTN_W : DIE_W;
    cursorCells(x, G_CORNER_UL, G_VLINE, G_CORNER_LL);
    cursorCells(x + cursorW + 1, G_CORNER_UR, G_VLINE, G_CORNER_LR);
    cursorX = x;
    cursorShown = true;
}

void hideDiceCursor(unsigned char x)
{
    // Where it was drawn, whatever the shared code asks
    (void)x;
    if (!cursorShown)
        return;
    cursorCells(cursorX, ' ', ' ', ' ');
    cursorCells(cursorX + cursorW + 1, ' ', ' ', ' ');
    cursorShown = false;
}

/*
 * Screen
 */

void resetScreen(bool forBorderScreen)
{
    static uint8_t y;

    cursorShown = false;
    if (!forBorderScreen)
    {
        blankRows(0, HEIGHT);
        return;
    }

    // Keep the corner dice
    for (y = 0; y < DIE_H; y++)
    {
        fill(DIE_W, y, WIDTH - 2 * DIE_W, ' ');
        fill(DIE_W, CORNER_BOTTOM + y, WIDTH - 2 * DIE_W, ' ');
    }
    blankRows(DIE_H, HEIGHT - 2 * DIE_H);
}

bool saveScreenBuffer()
{
    conSave();
    savedCursorShown = cursorShown;
    savedCursorX = cursorX;
    savedCursorW = cursorW;
    return true;
}

void restoreScreenBuffer()
{
    conRestore();
    cursorShown = savedCursorShown;
    cursorX = savedCursorX;
    cursorW = savedCursorW;
}

void initGraphics()
{
    termInit();
    resetScreen(false);
}

void resetGraphics()
{
    termExit();
}

uint8_t cycleNextColor()
{
    return 0;
}

void setColorMode()
{
}

#endif /* __TRS80M2__ */
