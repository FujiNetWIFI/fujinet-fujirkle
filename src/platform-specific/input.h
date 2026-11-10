/*
  Platform specific input. 
*/
#ifndef INPUT_H
#define INPUT_H

// Include platform specific vars
#include "vars.h"

// Platform specific implementations
unsigned char readJoystick();

#ifdef __WATCOMC__
#include <conio.h>
char cgetc(void);
#endif

#endif /* INPUT_H */
