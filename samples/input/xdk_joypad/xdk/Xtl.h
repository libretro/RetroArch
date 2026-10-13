/* Stand-in for the original Xbox XDK's Xtl.h: the types, constants and
 * input calls the xdk joypad driver uses, laid out as the XDK declares
 * them; the test defines the calls. */
#ifndef XDK_JOYPAD_TEST_XTL_H
#define XDK_JOYPAD_TEST_XTL_H

#include <stdint.h>

typedef void          *HANDLE;
typedef uint32_t       DWORD;
typedef DWORD         *PDWORD;
typedef uint16_t       WORD;
typedef uint8_t        BYTE;
typedef int16_t        SHORT;
typedef int            BOOL;
typedef void          *PXDEVICE_PREALLOC_TYPE;
typedef const void    *PXPP_DEVICE_TYPE;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define ERROR_SUCCESS              0L
#define ERROR_DEVICE_NOT_CONNECTED 1167L

#define XDEVICE_TYPE_GAMEPAD  ((PXPP_DEVICE_TYPE)1)
#define XDEVICE_NO_SLOT       0

#define XINPUT_GAMEPAD_DPAD_UP      0x00000001
#define XINPUT_GAMEPAD_DPAD_DOWN    0x00000002
#define XINPUT_GAMEPAD_DPAD_LEFT    0x00000004
#define XINPUT_GAMEPAD_DPAD_RIGHT   0x00000008
#define XINPUT_GAMEPAD_START        0x00000010
#define XINPUT_GAMEPAD_BACK         0x00000020
#define XINPUT_GAMEPAD_LEFT_THUMB   0x00000040
#define XINPUT_GAMEPAD_RIGHT_THUMB  0x00000080

#define XINPUT_GAMEPAD_A             0
#define XINPUT_GAMEPAD_B             1
#define XINPUT_GAMEPAD_X             2
#define XINPUT_GAMEPAD_Y             3
#define XINPUT_GAMEPAD_BLACK         4
#define XINPUT_GAMEPAD_WHITE         5
#define XINPUT_GAMEPAD_LEFT_TRIGGER  6
#define XINPUT_GAMEPAD_RIGHT_TRIGGER 7
#define XINPUT_GAMEPAD_MAX_CROSSTALK 30

typedef struct _XINPUT_GAMEPAD
{
   WORD  wButtons;
   BYTE  bAnalogButtons[8];
   SHORT sThumbLX;
   SHORT sThumbLY;
   SHORT sThumbRX;
   SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
   DWORD          dwPacketNumber;
   XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_POLLING_PARAMETERS
{
   BYTE fAutoPoll       : 1;
   BYTE fInterruptOut   : 1;
   BYTE ReservedMBZ1    : 6;
   BYTE bInputInterval;
   BYTE bOutputInterval;
   BYTE ReservedMBZ2;
} XINPUT_POLLING_PARAMETERS, *PXINPUT_POLLING_PARAMETERS;

void   XInitDevices(DWORD count, PXDEVICE_PREALLOC_TYPE types);
BOOL   XGetDeviceChanges(PXPP_DEVICE_TYPE type, PDWORD insertions,
         PDWORD removals);
HANDLE XInputOpen(PXPP_DEVICE_TYPE type, DWORD port, DWORD slot,
         PXINPUT_POLLING_PARAMETERS params);
void   XInputClose(HANDLE device);
DWORD  XInputPoll(HANDLE device);
DWORD  XInputGetState(HANDLE device, PXINPUT_STATE state);

#endif
