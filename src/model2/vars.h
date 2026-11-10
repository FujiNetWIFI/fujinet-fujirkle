#ifdef __TRS80M2__

#ifndef KEYMAP_H
#define KEYMAP_H

/*
  TRS-80 Model II under Pickles & Trout CP/M: 80x24, drawn with the P&T video
  driver's line characters and reverse video. The board is the 40-column one
  spread across 80, with 7x3 dice eight columns apart.
*/

#define WIDTH 80
#define HEIGHT 24

// 24 rows, as on the Apple II: the panel gives up its blank row
#define PANEL_H  8
#define DICE_Y   17
#define PROMPT_Y 14

// The strip: two 6x3 buttons, a wall, then six 7x3 dice
#define BTN_ROLL_X   4
#define BTN_BANK_X   11
#define STRIP_LEFT   2
#define STRIP_WALL   18
#define DICE_X       24
#define STRIP_RIGHT  77
#define DIE_STEP     8
// The [ ] cursor boxes the buttons as well; a button darkens only when pressed
#define BUTTON_CURSOR

#define ROLL_SOUND_MOD 1
#define ROLL_FRAMES 8
// waitvsync is timed off P&T's clock, so loop passes are frames
#define PACE_WITH_VSYNC 1

#define QUERY_SUFFIX "" // Z80 is little endian - no &be=1 needed

#undef ESCAPE
#define ESCAPE "ESC"

// Icons: cell codes (src/model2/console.h), bit 7 reverse video
#define ICON_TEXT_CURSOR  0xA0 // reverse space
#define ICON_MARK         '>'
#define ICON_MARK_ALT     ' '
#define ICON_PLAYER       '*'

// P&T CP/M key codes. F1 and F2 are spare up and down keys, BREAK is
// KEY_ESCAPE_ALT; the left and right _2 codes are unreachable placeholders.
#define KEY_LEFT_ARROW      0x1C
#define KEY_LEFT_ARROW_2    0xF1
#define KEY_LEFT_ARROW_3    0x2C // ,

#define KEY_RIGHT_ARROW     0x1D
#define KEY_RIGHT_ARROW_2   0xF2
#define KEY_RIGHT_ARROW_3   0x2E // .

#define KEY_UP_ARROW        0x1E
#define KEY_UP_ARROW_2      0x01 // F1
#define KEY_UP_ARROW_3      0x2D // -

#define KEY_DOWN_ARROW      0x1F
#define KEY_DOWN_ARROW_2    0x02 // F2
#define KEY_DOWN_ARROW_3    0x3D // =

#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x1B
#define KEY_ESCAPE_ALT   0x03 // BREAK
#define KEY_SPACEBAR     0x20
#define KEY_BACKSPACE    0x08

// No joystick
#define JOY_UP(v)    ((v) & 0)
#define JOY_DOWN(v)  ((v) & 0)
#define JOY_LEFT(v)  ((v) & 0)
#define JOY_RIGHT(v) ((v) & 0)
#define JOY_BTN_1(v) ((v) & 0)
#define JOY_BTN_2(v) ((v) & 0)

#endif /* KEYMAP_H */

#endif /* __TRS80M2__ */
