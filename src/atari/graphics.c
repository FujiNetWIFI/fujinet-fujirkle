#ifdef __ATARI__

/*
  Graphics functionality
*/


#include <peekpoke.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <atari.h>
#include "../platform-specific/graphics.h"

extern unsigned char charset[];

// I'm using the space XL/XE allocates to BASIC for the charset and screen buffer/backup
// On graphics initialization, I turn off BASIC, freeing up this space.

// There are two startup scenarios for XL/XE that must be considered:
// 1. If BASIC is disabled, the cc65 Stack will grow down from $BC1F, so to leave 512 bytes for stack,
//    so, I'm allowing $A000-$BA1F for charset and screen/buf.

// 2. If BASIC is enabled, the stack grows down from $9C1F. So, again allowing for 512 byte stack,
//    $9A1F is as high as we want to allow the program to reach without needing custom cc65 configs/segments

// Bottom line, program+vars needs to stay below $9A1F

#define CHARSET_LOC 0xB000
#define SCREEN_LOC ((uint8_t*)0xB400)
#define SCREEN_BAK ((uint8_t*)0xAB00)


#define xypos(x,y) (SCREEN_LOC + x + (y)*WIDTH)

unsigned char colorMode=0, oldChbas=0, colIndex=0;

// 26 lines
void DisplayList =
{
    DL_BLK8,DL_BLK8, // 2 blank lines
    DL_LMS(DL_CHR40x8x4),SCREEN_LOC, // 1 Line
    DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4, // 5 Lines
    DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4, // 5 Lines
    DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4, // 5 Lines
    DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4, // 5 Lines
    DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4,DL_CHR40x8x4, // 5 Lines
    DL_JVB  /* JVB Destination will be updated at the real display list location*/
};

// COLOR0-4: Shadow, Highlights, White, Alt White, Background
const unsigned char colors[] = {
 0x00, 0x2A, 0x0E, 0xCC, 0x84, // NTSC
 0x00, 0xFA, 0x0E, 0xBC, 0x74  // PAL
};

const unsigned char diceChars[] = {
  // Normal dice
  0x41, 0x40, 0x42, 0x40, 0x45, 0x40, 0x43, 0x40, 0x44, // 1
  0x41, 0x40, 0x47, 0x40, 0x40, 0x40, 0x48, 0x40, 0x44, // 2
  0x41, 0x40, 0x47, 0x40, 0x45, 0x40, 0x48, 0x40, 0x44, // 3
  0x46, 0x40, 0x47, 0x40, 0x40, 0x40, 0x48, 0x40, 0x49, // 4
  0x46, 0x40, 0x47, 0x40, 0x45, 0x40, 0x48, 0x40, 0x49, // 5
  0x46, 0x40, 0x47, 0x4A, 0x40, 0x4B, 0x48, 0x40, 0x49, // 6

  // Kept dice
  0x21, 0x4C, 0x22, 0x20, 0x25, 0x20, 0x23, 0x4D, 0x24, // 1
  0x21, 0x4C, 0x27, 0x20, 0x20, 0x20, 0x28, 0x4D, 0x24, // 2
  0x21, 0x4C, 0x27, 0x20, 0x25, 0x20, 0x28, 0x4D, 0x24, // 3
  0x26, 0x4C, 0x27, 0x20, 0x20, 0x20, 0x28, 0x4D, 0x29, // 4
  0x26, 0x4C, 0x27, 0x20, 0x25, 0x20, 0x28, 0x4D, 0x29, // 5
  0x26, 0x4C, 0x27, 0x2A, 0x20, 0x2B, 0x28, 0x4D, 0x29, // 6

  // Buttons; lit is the same tile inverted, +0x80 per byte
  0x41, 0x40, 0x42, 0x2C, 0x2D, 0x2E, 0x43, 0x40, 0x44, // roll
  0xC1, 0xC0, 0xC2, 0xAC, 0xAD, 0xAE, 0xC3, 0xC0, 0xC4, // roll lit
  0x41, 0x40, 0x42, 0x7D, 0x7E, 0x7F, 0x43, 0x40, 0x44, // bank
  0xC1, 0xC0, 0xC2, 0xFD, 0xFE, 0xFF, 0xC3, 0xC0, 0xC4  // bank lit
};

// Zero based indices into the table above
#define TILE_ROLL      12
#define TILE_ROLL_LIT  13
#define TILE_BANK      14
#define TILE_BANK_LIT  15

uint8_t cursorUpper[] = {177,182,182,182,178,0,0,0,0,0};
uint8_t cursorLower[] = {179,182,182,182,180,0,0,0,0,0};

unsigned char cycleNextColor() {
  return 0;
}

void setColorMode(unsigned char mode) {
  colorMode = mode; 
  memcpy(&OS.color0, &colors[colIndex], 5);
}


void initGraphics() {
  // Check if BASIC is enabled
  if (PEEKW(0x2E5)<0xBC1F && !(PEEK(0xD301)&2) ) {
    waitvsync();
    
    // Disable BASIC by setting bit 1 of 0xD301
    POKE(0xD301, PEEK(0xD301)|2);

    // Disable BASIC at shadow location
    POKE(0x3F8, 1);
  }

  resetScreen(false);
  if (PEEK(0xD014)==1) {
    colIndex=5;
  }
  
  // Set the displaylist end JVB instruction to point to the start of the display list
  POKEW(PEEKW(0x230)+31, (unsigned)OS.sdlst);

  // Overwrite current Display list with custom
  waitvsync();
  memcpy(OS.sdlst,&DisplayList,sizeof(DisplayList));

  // Load custom charset
  memcpy((void*)CHARSET_LOC,&charset,1024);
  oldChbas = OS.chbas;
  OS.chbas = CHARSET_LOC/256;

  setColorMode(0);
}

bool saveScreenBuffer() {
  memcpy(SCREEN_BAK, SCREEN_LOC, WIDTH*HEIGHT);
  return true;
}

void restoreScreenBuffer() {
  waitvsync();
  memcpy(SCREEN_LOC, SCREEN_BAK, WIDTH*HEIGHT);
}

void drawText(unsigned char x, unsigned char y, char* s) {
  static unsigned char c;
  static unsigned char* pos;

  pos = xypos(x,y);

  while(c=*s++) {
    if (c<65 && c>=32) c-=32;
    *pos++ = c;
  }  
}

void drawChar(unsigned char x, unsigned char y, char c, unsigned char alt) {
  if (c<65 && c>=32) c-=32;
  if (alt) {
    if (c<65 || c> 90)
      c+=128;
    else      
      c+=32;
  }
  POKE(xypos(x,y), c);
}

void drawTextAlt(unsigned char x, unsigned char y, char* s) {
  static unsigned char c;
  static unsigned char* pos;

  pos = xypos(x,y);

  while(c=*s++) {
    if (c<65 && c>=32) c-=32;
    if (c<65 || c> 90)
      c+=128;
    else      
      c+=32;

    *pos++ = c;
  }  
}

void resetScreen(bool forBorderScreen) { 
  static uint8_t i;
  waitvsync();
  setColorMode(colorMode);
  // Clear screen memory
  if (!forBorderScreen) {
    memset((void*)SCREEN_LOC,0,WIDTH*HEIGHT);
  } else {
    memset(xypos(0,3),0,40*(HEIGHT-6));
    for(i=0;i<3;i++) {
      memset(xypos(3,i),0,WIDTH-6);
      memset(xypos(3,HEIGHT-i),0,WIDTH-6);
    }
  }
}

void drawDie(unsigned char x, unsigned char y, unsigned char s, bool isSelected, bool isHighlighted) {
  static const unsigned char *source;
  static unsigned char *dest;
  static unsigned char idx;

  if (s>=1 && s<=6) {
    idx = s-1;
    if (isSelected)
      idx += 6;          // kept
  } else if (s==DIE_ROLL) {
    idx = isHighlighted ? TILE_ROLL_LIT : TILE_ROLL;
  } else if (s==DIE_BANK) {
    idx = isHighlighted ? TILE_BANK_LIT : TILE_BANK;
  } else {
    return;
  }

  source = diceChars + idx*9;

  // Draw the dice to the screen
  dest = xypos(x,y);
  memcpy(dest, source, 3);
  memcpy(dest+40, source+3, 3);
  memcpy(dest+80, source+6, 3);
}

void drawIcon(unsigned char x, unsigned char y, unsigned char icon) {
  POKE(xypos(x,y),icon);
}

void drawBlank(unsigned char x, unsigned char y) {
  POKE(xypos(x,y),0);
}

void drawSpace(unsigned char x, unsigned char y, unsigned char w) {
  memset(xypos(x,y),0,w);
}

void drawConnectionIcon(unsigned char x, unsigned char y) {
  POKEW(xypos(x,y), 0x0403);
}

void drawLine(unsigned char x, unsigned char y, unsigned char w) {
  memset(xypos(x,y),82,w);
}

// Blank the 3x3 cell a die occupies, leaving the strip box around it alone.
void drawDieSpace(unsigned char x, unsigned char y) {
  static unsigned char* pos;

  pos = xypos(x,y);
  memset(pos,0,3);
  memset(pos+40,0,3);
  memset(pos+80,0,3);
}

// Verticals are the same 124 the box sides use; the tees weld it to the rules.
void drawBoxDivider(unsigned char x, unsigned char y, unsigned char h) {
  static unsigned char i;
  static unsigned char* pos;

  pos = xypos(x,y);
  *pos = ICON_TEE_TOP;

  for(i=0;i<h;i++) {
    pos+=40;
    *pos = 124;
  }

  *(pos+40) = ICON_TEE_BOTTOM;
}

// 40 columns spaces the tiles far enough apart to need no extra weight
void drawBoxDividerWide(unsigned char x, unsigned char y, unsigned char h) {
  drawBoxDivider(x,y,h);
}


void drawBox(unsigned char x, unsigned char y, unsigned char w, unsigned char h) {
  static unsigned char i;
  static unsigned char* pos;

  pos = xypos(x,y);

  // Top row
  *pos=81;
  memset(pos+1,82,w);
  *(pos+w+1)=85;

  // Sides
  for(i=0;i<h;i++) {
    pos+=40;
    *pos=*(pos+w+1)=124;
  }
  
  // Bottom row
  *(pos+=40)=87;
  memset(pos+1,82,w);
  *(pos+w+1)=90;
}

// The frame sits one row above the die and spans the three it occupies.
#define DICE_CURSOR_Y (DICE_Y - 1)

void drawDiceCursorInternal(unsigned char x, unsigned char offset) {
  static unsigned char i;
  static unsigned char* pos;

  pos = xypos(x-1,DICE_CURSOR_Y);

  // Top
  memcpy(pos, cursorUpper+offset, 5);
  
  // Sides
  for(i=0;i<3;i++) {
    pos+=40;
    *pos=*(pos+4)=offset? 0 :181;
  }

  // Bottom
  memcpy(pos+40, cursorLower+offset, 5);
}

void drawDiceCursor(unsigned char x) {
  drawDiceCursorInternal(x,0);
}

void hideDiceCursor(unsigned char x) {
  drawDiceCursorInternal(x,5);
}

void resetGraphics() {
  OS.color4=2;
  OS.chbas = oldChbas;
  waitvsync();
}

#endif /* __ATARI__ */
