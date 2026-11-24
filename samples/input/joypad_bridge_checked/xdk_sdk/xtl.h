/* What xdk_joypad.c names of the original Xbox's XDK, for
 * xdk_checked_test alone: the SDK's headers are not public, and the
 * test runs the driver's reads over a pad state it fills itself, never
 * these calls, so the layout and values are the test's own. */
#ifndef XDK_CHECKED_XTL_H
#define XDK_CHECKED_XTL_H
#include <stdint.h>
typedef void          *HANDLE;
typedef unsigned long  DWORD;
typedef uint16_t       WORD;
typedef uint8_t        BYTE;
typedef int16_t        SHORT;
typedef DWORD         *PDWORD;
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

typedef struct
{
   WORD  wButtons;
   BYTE  bAnalogButtons[8];
   SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} XINPUT_GAMEPAD;
typedef struct { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; } XINPUT_STATE;
typedef struct { BYTE fAutoPoll, fInterruptOut, bInputInterval,
   bOutputInterval, ReservedMBZ1, ReservedMBZ2; } XINPUT_POLLING_PARAMETERS;
typedef struct { DWORD dwDeviceType; DWORD dwPreallocCount; }
   XDEVICE_PREALLOC_TYPE;

#define XINPUT_GAMEPAD_DPAD_UP       0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN     0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT     0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT    0x0008
#define XINPUT_GAMEPAD_START         0x0010
#define XINPUT_GAMEPAD_BACK          0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB    0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB   0x0080
#define XINPUT_GAMEPAD_A             0
#define XINPUT_GAMEPAD_B             1
#define XINPUT_GAMEPAD_X             2
#define XINPUT_GAMEPAD_Y             3
#define XINPUT_GAMEPAD_BLACK         4
#define XINPUT_GAMEPAD_WHITE         5
#define XINPUT_GAMEPAD_LEFT_TRIGGER  6
#define XINPUT_GAMEPAD_RIGHT_TRIGGER 7
#define XINPUT_GAMEPAD_MAX_CROSSTALK 30
#define XDEVICE_NO_SLOT              0
#define XDEVICE_TYPE_GAMEPAD         ((void*)0)
#define ERROR_SUCCESS                0

void   XInitDevices(DWORD count, const XDEVICE_PREALLOC_TYPE *types);
int    XGetDeviceChanges(void *type, DWORD *inserted, DWORD *removed);
HANDLE XInputOpen(void *type, DWORD port, DWORD slot,
      XINPUT_POLLING_PARAMETERS *params);
void   XInputClose(HANDLE h);
DWORD  XInputGetState(HANDLE h, XINPUT_STATE *state);
DWORD  XInputPoll(HANDLE h);
#endif
