#pragma once
#include <wut.h>

/**
 * \defgroup vpad_input VPAD Input
 * \ingroup vpad
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VPADVec2D VPADVec2D;
typedef struct VPADVec3D VPADVec3D;
typedef struct VPADTouchData VPADTouchData;
typedef struct VPADAccStatus VPADAccStatus;
typedef struct VPADGyroStatus VPADGyroStatus;
typedef struct VPADStatus VPADStatus;

typedef enum VPADButtons
{
   VPAD_BUTTON_A        = 0x8000,
   VPAD_BUTTON_B        = 0x4000,
   VPAD_BUTTON_X        = 0x2000,
   VPAD_BUTTON_Y        = 0x1000,
   VPAD_BUTTON_LEFT     = 0x0800,
   VPAD_BUTTON_RIGHT    = 0x0400,
   VPAD_BUTTON_UP       = 0x0200,
   VPAD_BUTTON_DOWN     = 0x0100,
   VPAD_BUTTON_ZL       = 0x0080,
   VPAD_BUTTON_ZR       = 0x0040,
   VPAD_BUTTON_L        = 0x0020,
   VPAD_BUTTON_R        = 0x0010,
   VPAD_BUTTON_PLUS     = 0x0008,
   VPAD_BUTTON_MINUS    = 0x0004,
   VPAD_BUTTON_HOME     = 0x0002,
   VPAD_BUTTON_SYNC     = 0x0001,
   VPAD_BUTTON_STICK_R  = 0x00020000,
   VPAD_BUTTON_STICK_L  = 0x00040000,
   VPAD_BUTTON_TV       = 0x00010000,
} VPADButtons;

typedef enum VPADTouchPadValidity
{
   //! Both X and Y touchpad positions are accurate
   VPAD_VALID           = 0x0,

   //! X position is inaccurate
   VPAD_INVALID_X       = 0x1,

   //! Y position is inaccurate
   VPAD_INVALID_Y       = 0x2,
} VPADTouchPadValidity;


typedef enum VPADReadError
{
   VPAD_READ_SUCCESS             = 0,
   VPAD_READ_NO_SAMPLES          = -1,
   VPAD_READ_INVALID_CONTROLLER  = -2,
} VPADReadError;

struct VPADVec2D
{
   float x;
   float y;
};
CHECK_OFFSET(VPADVec2D, 0x00, x);
CHECK_OFFSET(VPADVec2D, 0x04, y);
CHECK_SIZE(VPADVec2D, 0x08);

struct VPADVec3D
{
   float x;
   float y;
   float z;
};
CHECK_OFFSET(VPADVec3D, 0x00, x);
CHECK_OFFSET(VPADVec3D, 0x04, y);
CHECK_OFFSET(VPADVec3D, 0x08, z);
CHECK_SIZE(VPADVec3D, 0x0C);

struct VPADTouchData
{
   uint16_t x;
   uint16_t y;

   //! 0 if screen is not currently being touched
   uint16_t touched;

   //! Bitfield of VPADTouchPadValidity to indicate how touch sample accuracy
   uint16_t validity;
};
CHECK_OFFSET(VPADTouchData, 0x00, x);
CHECK_OFFSET(VPADTouchData, 0x02, y);
CHECK_OFFSET(VPADTouchData, 0x04, touched);
CHECK_OFFSET(VPADTouchData, 0x06, validity);
CHECK_SIZE(VPADTouchData, 0x08);

struct VPADAccStatus
{
   float unk1;
   float unk2;
   float unk3;
   float unk4;
   float unk5;
   VPADVec2D vertical;
};
CHECK_OFFSET(VPADAccStatus, 0x14, vertical);
CHECK_SIZE(VPADAccStatus, 0x1c);

struct VPADGyroStatus
{
   float unk1;
   float unk2;
   float unk3;
   float unk4;
   float unk5;
   float unk6;
};
CHECK_SIZE(VPADGyroStatus, 0x18);

struct VPADStatus
{
   //! Indicates what VPADButtons are held down
   uint32_t hold;

   //! Indicates what VPADButtons have been pressed since last sample
   uint32_t trigger;

   //! Indicates what VPADButtons have been released since last sample
   uint32_t release;

   //! Position of left analog stick
   VPADVec2D leftStick;

   //! Position of right analog stick
   VPADVec2D rightStick;

   //! Status of DRC accelorometer
   VPADAccStatus accelorometer;

   //! Status of DRC gyro
   VPADGyroStatus gyro;

   UNKNOWN(2);

   //! Current touch position on DRC
   VPADTouchData tpNormal;

   //! Filtered touch position, first level of smoothing
   VPADTouchData tpFiltered1;

   //! Filtered touch position, second level of smoothing
   VPADTouchData tpFiltered2;

   UNKNOWN(0x28);

   //! Status of DRC magnetometer
   VPADVec3D mag;

   //! Current volume set by the slide control
   uint8_t slideVolume;

   //! Battery level of controller
   uint8_t battery;

   //! Status of DRC microphone
   uint8_t micStatus;

   //! Unknown volume related value
   uint8_t slideVolumeEx;

   UNKNOWN(0x7);
};
CHECK_OFFSET(VPADStatus, 0x00, hold);
CHECK_OFFSET(VPADStatus, 0x04, trigger);
CHECK_OFFSET(VPADStatus, 0x08, release);
CHECK_OFFSET(VPADStatus, 0x0C, leftStick);
CHECK_OFFSET(VPADStatus, 0x14, rightStick);
CHECK_OFFSET(VPADStatus, 0x1C, accelorometer);
CHECK_OFFSET(VPADStatus, 0x38, gyro);
CHECK_OFFSET(VPADStatus, 0x52, tpNormal);
CHECK_OFFSET(VPADStatus, 0x5A, tpFiltered1);
CHECK_OFFSET(VPADStatus, 0x62, tpFiltered2);
CHECK_OFFSET(VPADStatus, 0x94, mag);
CHECK_OFFSET(VPADStatus, 0xA0, slideVolume);
CHECK_OFFSET(VPADStatus, 0xA1, battery);
CHECK_OFFSET(VPADStatus, 0xA2, micStatus);
CHECK_OFFSET(VPADStatus, 0xA3, slideVolumeEx);
CHECK_SIZE(VPADStatus, 0xAC);

//! Deprecated
void
VPADInit();

int32_t
VPADRead(uint32_t chan,
         VPADStatus *buffers,
         uint32_t count,
         VPADReadError *error);

void
VPADGetTPCalibratedPoint(uint32_t chan,
                         VPADTouchData *calibratedData,
                         VPADTouchData *uncalibratedData);

#ifdef __cplusplus
}
#endif

/** @} */
