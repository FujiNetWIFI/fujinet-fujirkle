#ifdef _CMOC_VERSION_

#include <stdint.h>
#include <conio.h>
#include <string.h>
#include "../platform-specific/sound.h"
#include "hires.h"

#ifdef COCO3

#include <coco.h>
#include "charset_coco3.h"

// Maps the 4 source colors of the 2bpp font onto hardware palette registers.
extern const unsigned char text_palettes[PAL_COUNT][4];

// Per-palette 2bpp->4bpp expansion table: maps a source font nibble (2 2bpp
// pixels) to the 4bpp screen byte it becomes. Built once at init so
// hires_Draw is a table lookup per nibble instead of eight palette lookups.
static uint8_t expandNib[PAL_COUNT][16];

void buildExpandTables(void)
{
  uint8_t p, n;
  for (p = 0; p < PAL_COUNT; p++) {
    const unsigned char *pal = text_palettes[p];
    for (n = 0; n < 16; n++)
      expandNib[p][n] = (pal[(n >> 2) & 3] << 4) | pal[n & 3];
  }
}

/*-----------------------------------------------------------------------*/
// Expand one 8x8 2bpp glyph into the 4bpp screen, recoloring through the
// selected palette. x/y are character cells; xlen/ylen are unused (kept
// for interface compatibility - a glyph is always 8x8).
void hires_Draw(uint8_t x, uint8_t y, uint8_t xlen, uint8_t ylen, uint8_t rop, const uint8_t *src)
{
  const uint8_t *nib = expandNib[rop & 3];
  uint8_t *pos = (uint8_t *)SCREEN + (uint16_t)y * 1280 + (uint16_t)x * 4;
  uint8_t row, hi, lo;
  (void)xlen; (void)ylen;

  BEGIN_GFX
  for (row = 0; row < 8; row++) {
    hi = (uint8_t)src[row * 2];      // pixels 0-3
    lo = (uint8_t)src[row * 2 + 1];  // pixels 4-7
    pos[0] = nib[hi >> 4];
    pos[1] = nib[hi & 15];
    pos[2] = nib[lo >> 4];
    pos[3] = nib[lo & 15];
    pos += 160;
  }
  END_GFX
}

/*-----------------------------------------------------------------------*/
void hires_putc(uint8_t x, uint8_t y, uint8_t rop, uint8_t c)
{
  // Bit 7 of a glyph index selects the alt palette (the MSDOS charset
  // stores recolored copies there; here we recolor at render time).
  if ((c & 0x80) && rop == PAL_NORMAL) {
    rop = PAL_ALT;
  }
  hires_Draw(x, y, 1, 8, rop, (const uint8_t *)charset_coco3[c & 0x7F]);
}

/*-----------------------------------------------------------------------*/
void hires_putcc(uint8_t x, uint8_t y, uint8_t rop, uint16_t cc)
{
  hires_putc(x, y, rop, (uint8_t)(cc >> 8));
  hires_putc(++x, y, rop, (uint8_t)cc);
}

// Fill a rectangle of cells with a packed 4bpp byte (both nibbles a color).
// x/y/xlen are cells; ylen is cells.
void hires_Mask(uint8_t x, uint8_t y, uint8_t xlen, uint8_t ylen, uint8_t c)
{
  uint8_t *pos = (uint8_t *)SCREEN + (uint16_t)y * 1280 + (uint16_t)x * 4;
  uint16_t rows = (uint16_t)ylen * 8;

  BEGIN_GFX
  while (rows--) {
    memset(pos, c, (uint16_t)xlen * 4);
    pos += 160;
  }
  END_GFX
}

#else /* CoCo 1/2 */

extern uint8_t charset[];

/*-----------------------------------------------------------------------*/
void hires_putc(uint8_t x, uint8_t y, uint8_t rop, uint8_t c)
{
  hires_Draw(x,y,1,8,rop,&charset[(uint16_t)c<<3]);
}

/*-----------------------------------------------------------------------*/
void hires_putcc(uint8_t x, uint8_t y,uint8_t rop, uint16_t cc)
{
  hires_putc(x,y,rop,(uint8_t)(cc>>8));
  hires_putc(++x,y,rop,(uint8_t)cc);
}

void hires_Mask(uint8_t x, uint8_t y, uint8_t xlen, uint8_t ylen, uint8_t c)
{
  uint8_t *pos = SCREEN+(uint16_t)y*32+x;
  ylen++;
  while (--ylen) {
    memset(pos,c,xlen);
    pos+=32;
  }
}

void hires_Draw(uint8_t x, uint8_t y, uint8_t xlen, uint8_t ylen, uint8_t rop, const uint8_t *src)
{
  uint8_t *pos = SCREEN+(uint16_t)y*32+x;
  ylen++;
  while (--ylen) {
    *pos = *(src++) & rop;
    pos += 32;
  }
}

#endif /* COCO3 */

#endif
