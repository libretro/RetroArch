/*-------------------------------------------------------------

keyboard.h -- keyboard event system

Copyright (C) 2008, 2009
DAVY Guillaume davyg2@gmail.com
Brian Johnson brijohn@gmail.com
dhewg

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "wsksymdef.h"

#define MOD_SHIFT_L		(1 << 0)
#define MOD_SHIFT_R		(1 << 1)
#define MOD_SHIFTLOCK		(1 << 2)
#define MOD_CAPSLOCK		(1 << 3)
#define MOD_CONTROL_L		(1 << 4)
#define MOD_CONTROL_R		(1 << 5)
#define MOD_META_L		(1 << 6)
#define MOD_META_R		(1 << 7)
#define MOD_MODESHIFT		(1 << 8)
#define MOD_NUMLOCK		(1 << 9)
#define MOD_COMPOSE		(1 << 10)
#define MOD_HOLDSCREEN		(1 << 11)
#define MOD_COMMAND		(1 << 12)
#define MOD_COMMAND1		(1 << 13)
#define MOD_COMMAND2		(1 << 14)
#define MOD_MODELOCK		(1 << 15)

#define MOD_ANYSHIFT		(MOD_SHIFT_L | MOD_SHIFT_R | MOD_SHIFTLOCK)
#define MOD_ANYCONTROL		(MOD_CONTROL_L | MOD_CONTROL_R)
#define MOD_ANYMETA		(MOD_META_L | MOD_META_R)

#define MOD_ONESET(val, mask)	(((val) & (mask)) != 0)
#define MOD_ALLSET(val, mask)	(((val) & (mask)) == (mask))

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef enum {
	KEYBOARD_CONNECTED,
	KEYBOARD_DISCONNECTED,
	KEYBOARD_PRESSED,
	KEYBOARD_RELEASED
} keyboard_event_type;

typedef struct {
	keyboard_event_type type;
	u16 modifiers;
	u8 keycode;
	u16 symbol;
} keyboard_event;

typedef void (*keyPressCallback)(char symbol);

s32 KEYBOARD_Init(keyPressCallback keypress_cb);
s32 KEYBOARD_Deinit(void);

s32 KEYBOARD_GetEvent(keyboard_event *event);
s32 KEYBOARD_FlushEvents(void);


#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* __KEYBOARD_H__ */

