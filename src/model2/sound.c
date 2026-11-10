#ifdef __TRS80M2__

/*
  The console bell is the only sound, so it marks the moments that need the
  player's attention and the rest stay quiet.
*/

#include <stdint.h>
#include <stdbool.h>
#include "console.h"
#include "../misc.h"
#include "../platform-specific/sound.h"

static void bell(void)
{
    if (!prefs.disableSound)
        termBell();
}

void initSound() {}
void disableKeySounds() {}
void enableKeySounds() {}
void soundStop() {}

void soundJoinGame()
{
    bell();
}

void soundMyTurn()
{
    bell();
}

void soundHotDice()
{
    bell();
    pause(10);
    bell();
}

void soundNoScore()
{
    bell();
}

void soundGameDone()
{
    bell();
}

void soundRollDice() {}
void soundRollButton() {}
void soundTick() {}
void soundCursor() {}
void soundScoreCursor() {}
void soundKeep() {}
void soundRelease() {}
void soundScore() {}

#endif /* __TRS80M2__ */
