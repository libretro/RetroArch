/* Stand-in for libnx's switch.h: the types and constants the Switch
 * joypad driver uses, values as libnx defines them; the test defines
 * the calls (the pad helpers are inline in libnx, functions here). */
#ifndef SWITCH_JOYPAD_TEST_SWITCH_H
#define SWITCH_JOYPAD_TEST_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;
typedef u32      Result;
#ifndef __GLIBC__
typedef unsigned int uint;
#endif

#define R_SUCCEEDED(res) ((res) == 0)

typedef enum
{
   HidNpadIdType_No1      = 0,
   HidNpadIdType_Handheld = 0x20
} HidNpadIdType;

enum
{
   HidNpadStyleTag_NpadFullKey  = 1u << 0,
   HidNpadStyleTag_NpadHandheld = 1u << 1,
   HidNpadStyleTag_NpadJoyDual  = 1u << 2,
   HidNpadStyleTag_NpadJoyLeft  = 1u << 3,
   HidNpadStyleTag_NpadJoyRight = 1u << 4
};
#define HidNpadStyleSet_NpadStandard (HidNpadStyleTag_NpadFullKey \
   | HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual   \
   | HidNpadStyleTag_NpadJoyLeft  | HidNpadStyleTag_NpadJoyRight)

#define HidNpadButton_A       (1ull << 0)
#define HidNpadButton_B       (1ull << 1)
#define HidNpadButton_X       (1ull << 2)
#define HidNpadButton_Y       (1ull << 3)
#define HidNpadButton_StickL  (1ull << 4)
#define HidNpadButton_StickR  (1ull << 5)
#define HidNpadButton_L       (1ull << 6)
#define HidNpadButton_R       (1ull << 7)
#define HidNpadButton_Plus    (1ull << 10)
#define HidNpadButton_Minus   (1ull << 11)
#define HidNpadButton_Left    (1ull << 12)
#define HidNpadButton_Up      (1ull << 13)
#define HidNpadButton_Right   (1ull << 14)
#define HidNpadButton_Down    (1ull << 15)
#define HidNpadButton_LeftSL  (1ull << 24)
#define HidNpadButton_LeftSR  (1ull << 25)
#define HidNpadButton_RightSL (1ull << 26)
#define HidNpadButton_RightSR (1ull << 27)

#define HidNpadJoyHoldType_Horizontal 1
#define HidNpadAttribute_IsConnected  (1u << 0)
#define HidDeviceTypeBits_JoyLeft     (1u << 4)
#define HidDeviceTypeBits_JoyRight    (1u << 5)

typedef struct HidAnalogStickState { s32 x; s32 y; } HidAnalogStickState;
typedef struct HidVibrationDeviceHandle { u32 type_value; } HidVibrationDeviceHandle;
typedef struct HidVibrationValue
{
   float amp_low;
   float freq_low;
   float amp_high;
   float freq_high;
} HidVibrationValue;

typedef struct
{
   u8                  id_mask;
   bool                read_handheld;
   u32                 attributes;
   u64                 buttons_cur;
   HidAnalogStickState sticks[2];
} PadState;

void padConfigureInput(u32 max_players, u32 style_set);
void padInitialize(PadState *pad, int id);
void padInitializeDefault(PadState *pad);
void padUpdate(PadState *pad);
bool padIsHandheld(const PadState *pad);
bool padIsNpadActive(const PadState *pad, HidNpadIdType id);
bool padIsConnected(const PadState *pad);
u64  padGetButtons(const PadState *pad);
HidAnalogStickState padGetStickPos(const PadState *pad, unsigned i);

void   hidSetNpadJoyHoldType(int type);
u32    hidGetNpadDeviceType(HidNpadIdType id);
Result hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id);
Result hidSetNpadJoyAssignmentModeDual(HidNpadIdType id);
Result hidMergeSingleJoyAsDualJoy(HidNpadIdType id1, HidNpadIdType id2);
Result hidInitializeVibrationDevices(HidVibrationDeviceHandle *handles,
      s32 total, HidNpadIdType id, u32 style);
Result hidSendVibrationValues(const HidVibrationDeviceHandle *handles,
      const HidVibrationValue *values, s32 count);

#endif
