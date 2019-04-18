/*-------------------------------------------------------------

usbkeyboard.h -- Usb keyboard support(boot protocol)

Copyright (C) 2008, 2009
DAVY Guillaume davyg2@gmail.com
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

#ifndef __USBKEYBOARD_H__
#define __USBKEYBOARD_H__

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef enum
{
	USBKEYBOARD_PRESSED = 0,
	USBKEYBOARD_RELEASED,
	USBKEYBOARD_DISCONNECTED
} USBKeyboard_eventType;

typedef enum
{
	USBKEYBOARD_LEDNUM = 0,
	USBKEYBOARD_LEDCAPS,
	USBKEYBOARD_LEDSCROLL
} USBKeyboard_led;

typedef struct
{
	USBKeyboard_eventType type;
	u8 keyCode;
} USBKeyboard_event;

typedef void (*eventcallback) (USBKeyboard_event event);

s32 USBKeyboard_Initialize(void);
s32 USBKeyboard_Deinitialize(void);

s32 USBKeyboard_Open(const eventcallback cb);
void USBKeyboard_Close(void);

bool USBKeyboard_IsConnected(void);
s32 USBKeyboard_Scan(void);

s32 USBKeyboard_SetLed(const USBKeyboard_led led, bool on);
s32 USBKeyboard_ToggleLed(const USBKeyboard_led led);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* __USBKEYBOARD_H__ */
