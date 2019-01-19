#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VPADButtonBits {
   VPAD_BUTTON_SYNC_BIT             = 0,
   VPAD_BUTTON_HOME_BIT             = 1,
   VPAD_BUTTON_MINUS_BIT            = 2,
   VPAD_BUTTON_PLUS_BIT             = 3,
   VPAD_BUTTON_R_BIT                = 4,
   VPAD_BUTTON_L_BIT                = 5,
   VPAD_BUTTON_ZR_BIT               = 6,
   VPAD_BUTTON_ZL_BIT               = 7,
   VPAD_BUTTON_DOWN_BIT             = 8,
   VPAD_BUTTON_UP_BIT               = 9,
   VPAD_BUTTON_RIGHT_BIT            = 10,
   VPAD_BUTTON_LEFT_BIT             = 11,
   VPAD_BUTTON_Y_BIT                = 12,
   VPAD_BUTTON_X_BIT                = 13,
   VPAD_BUTTON_B_BIT                = 14,
   VPAD_BUTTON_A_BIT                = 15,
   VPAD_BUTTON_TV_BIT               = 16,
   VPAD_BUTTON_STICK_R_BIT          = 17,
   VPAD_BUTTON_STICK_L_BIT          = 18,
   VPAD_BUTTON_TOUCH_BIT            = 19,
   VPAD_BUTTON_UNUSED1_BIT          = 20,
   VPAD_BUTTON_UNUSED2_BIT          = 21,
   VPAD_BUTTON_UNUSED3_BIT          = 22,
   VPAD_STICK_R_EMULATION_DOWN_BIT  = 23,
   VPAD_STICK_R_EMULATION_UP_BIT    = 24,
   VPAD_STICK_R_EMULATION_RIGHT_BIT = 25,
   VPAD_STICK_R_EMULATION_LEFT_BIT  = 26,
   VPAD_STICK_L_EMULATION_DOWN_BIT  = 27,
   VPAD_STICK_L_EMULATION_UP_BIT    = 28,
   VPAD_STICK_L_EMULATION_RIGHT_BIT = 29,
   VPAD_STICK_L_EMULATION_LEFT_BIT  = 30,
} VPADButtonBits;

typedef enum VPADButtons
{
   VPAD_BUTTON_SYNC             = 0x00000001,
   VPAD_BUTTON_HOME             = 0x00000002,
   VPAD_BUTTON_MINUS            = 0x00000004,
   VPAD_BUTTON_PLUS             = 0x00000008,
   VPAD_BUTTON_R                = 0x00000010,
   VPAD_BUTTON_L                = 0x00000020,
   VPAD_BUTTON_ZR               = 0x00000040,
   VPAD_BUTTON_ZL               = 0x00000080,
   VPAD_BUTTON_DOWN             = 0x00000100,
   VPAD_BUTTON_UP               = 0x00000200,
   VPAD_BUTTON_RIGHT            = 0x00000400,
   VPAD_BUTTON_LEFT             = 0x00000800,
   VPAD_BUTTON_Y                = 0x00001000,
   VPAD_BUTTON_X                = 0x00002000,
   VPAD_BUTTON_B                = 0x00004000,
   VPAD_BUTTON_A                = 0x00008000,
   VPAD_BUTTON_TV               = 0x00010000,
   VPAD_BUTTON_STICK_R          = 0x00020000,
   VPAD_BUTTON_STICK_L          = 0x00040000,
   VPAD_BUTTON_TOUCH            = 0x00080000,
   VPAD_BUTTON_UNUSED1          = 0x00100000,
   VPAD_BUTTON_UNUSED2          = 0x00200000,
   VPAD_BUTTON_UNUSED3          = 0x00400000,
   VPAD_STICK_R_EMULATION_DOWN  = 0x00800000,
   VPAD_STICK_R_EMULATION_UP    = 0x01000000,
   VPAD_STICK_R_EMULATION_RIGHT = 0x02000000,
   VPAD_STICK_R_EMULATION_LEFT  = 0x04000000,
   VPAD_STICK_L_EMULATION_DOWN  = 0x08000000,
   VPAD_STICK_L_EMULATION_UP    = 0x10000000,
   VPAD_STICK_L_EMULATION_RIGHT = 0x20000000,
   VPAD_STICK_L_EMULATION_LEFT  = 0x40000000,
} VPADButtons;

#define VPAD_MASK_EMULATED_STICKS       (VPAD_STICK_R_EMULATION_LEFT  | \
                                         VPAD_STICK_R_EMULATION_RIGHT | \
                                         VPAD_STICK_R_EMULATION_UP    | \
                                         VPAD_STICK_R_EMULATION_DOWN  | \
                                         VPAD_STICK_L_EMULATION_LEFT  | \
                                         VPAD_STICK_L_EMULATION_RIGHT | \
                                         VPAD_STICK_L_EMULATION_UP    | \
                                         VPAD_STICK_L_EMULATION_DOWN)
#define VPAD_MASK_BUTTONS               ~VPAD_MASK_EMULATED_STICKS

typedef enum VPADTouchPadValidity
{
   /*! Both X and Y touchpad positions are accurate */
   VPAD_VALID           = 0x0,

   /*! X position is inaccurate */
   VPAD_INVALID_X       = 0x1,

   /*! Y position is inaccurate */
   VPAD_INVALID_Y       = 0x2,
} VPADTouchPadValidity;

typedef enum VPADReadError
{
   VPAD_READ_SUCCESS             = 0,
   VPAD_READ_NO_SAMPLES          = -1,
   VPAD_READ_INVALID_CONTROLLER  = -2,
} VPADReadError;

typedef struct VPADVec2D
{
   float x;
   float y;
}VPADVec2D;

typedef struct VPADVec3D
{
   float x;
   float y;
   float z;
}VPADVec3D;

typedef struct VPADTouchData
{
   uint16_t x;
   uint16_t y;
   uint16_t touched;
   uint16_t validity;
}VPADTouchData;

typedef struct VPADAccStatus
{
   float unk1;
   float unk2;
   float unk3;
   float unk4;
   float unk5;
   VPADVec2D vertical;
}VPADAccStatus;

typedef struct VPADGyroStatus
{
   float unk1;
   float unk2;
   float unk3;
   float unk4;
   float unk5;
   float unk6;
}VPADGyroStatus;

typedef struct VPADStatus
{
   uint32_t hold;
   uint32_t trigger;
   uint32_t release;

   VPADVec2D leftStick;
   VPADVec2D rightStick;
   VPADAccStatus accelorometer;
   VPADGyroStatus gyro;

   uint16_t __unknown0;

   /*! Current touch position on DRC */
   VPADTouchData tpNormal;

   /*! Filtered touch position, first level of smoothing */
   VPADTouchData tpFiltered1;

   /*! Filtered touch position, second level of smoothing */
   VPADTouchData tpFiltered2;

   uint32_t __unknown1[0xA];

   VPADVec3D mag;
   uint8_t slideVolume;
   uint8_t battery;
   uint8_t micStatus;
   uint8_t slideVolumeEx;
   uint32_t __unknown2[0x2];
}VPADStatus;

/*! Deprecated */
void VPADInit();

int32_t VPADRead(uint32_t chan, VPADStatus *buffers, uint32_t count, VPADReadError *error);
void VPADGetTPCalibratedPoint(uint32_t chan, VPADTouchData *calibratedData, VPADTouchData *uncalibratedData);

#ifdef __cplusplus
}
#endif
