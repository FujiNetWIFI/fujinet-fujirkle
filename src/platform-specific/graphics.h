/*
 Platform specific graphics commands
*/

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include "../misc.h"


// Call to clear the screen, passing true to preserve borders
void resetScreen(bool forBorderScreen);

void drawText(unsigned char x, unsigned char y, char* s);
void drawTextAlt(unsigned char x, unsigned char y, char* s);

void drawChar(unsigned char x, unsigned char y, char c, unsigned char alt);

void drawIcon(unsigned char x, unsigned char y, unsigned char icon);

// s is a die face 1-6 or one of these buttons
enum { DIE_BANK = 13, DIE_ROLL };
void drawDie(unsigned char x, unsigned char y, unsigned char s, bool isSelected, bool isHighlighted);

void drawConnectionIcon(unsigned char x, unsigned char y);
void drawBlank(unsigned char x, unsigned char y);
void drawSpace(unsigned char x, unsigned char y, unsigned char w);
// Erase one die cell. Dice are not on the text baseline on every model, so
// this mirrors drawDie's geometry rather than blanking three text rows.
void drawDieSpace(unsigned char x, unsigned char y);
void drawLine(unsigned char x, unsigned char y, unsigned char w);
void drawBox(unsigned char x, unsigned char y, unsigned char w, unsigned char h);
// Vertical divider inside a box drawn by drawBox(_, y, _, h), welded to the
// top and bottom rules.
void drawBoxDivider(unsigned char x, unsigned char y, unsigned char h);
// Divider for a box's major split, heavier across columns x-1 and x where the
// platform draws it so. Widths a single cell cannot express are what let the
// compartments come out even.
void drawBoxDividerWide(unsigned char x, unsigned char y, unsigned char h);
void drawDiceCursor(unsigned char x);
void hideDiceCursor(unsigned char x);

// Call to save screen buffer for later restore. Returns false
// if screen buffer not supported
bool saveScreenBuffer();
void restoreScreenBuffer();

void initGraphics();
void resetGraphics();
void waitvsync();

uint8_t cycleNextColor();
void setColorMode();
extern unsigned char colorMode;

#endif /* GRAPHICS_H */
