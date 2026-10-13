/* Compile-only stand-in for orbisdev's <orbis/orbisPad.h>: the pad
 * configuration ps4_joypad.c reads, as the toolchain declares it. */
#ifndef STUB_ORBIS_ORBISPAD_H
#define STUB_ORBIS_ORBISPAD_H

#include <orbis/libScePad.h>

typedef struct OrbisPadConfig
{
   SceUserServiceUserId userId;
   ScePadData          *padDataCurrent;
   ScePadData          *padDataLast;
   unsigned int         buttonsPressed;
   unsigned int         buttonsReleased;
   unsigned int         buttonsHold;
   int                  padHandle;
   int                  orbispad_initialized;
} OrbisPadConfig;

OrbisPadConfig *orbisPadGetConf(void);

#endif
