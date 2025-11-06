#pragma once
#include <vpad/input.h>

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

#define VPAD_BUTTON_TOUCH (0x00080000)

#define VPAD_MASK_EMULATED_STICKS       (VPAD_STICK_R_EMULATION_LEFT  | \
                                         VPAD_STICK_R_EMULATION_RIGHT | \
                                         VPAD_STICK_R_EMULATION_UP    | \
                                         VPAD_STICK_R_EMULATION_DOWN  | \
                                         VPAD_STICK_L_EMULATION_LEFT  | \
                                         VPAD_STICK_L_EMULATION_RIGHT | \
                                         VPAD_STICK_L_EMULATION_UP    | \
                                         VPAD_STICK_L_EMULATION_DOWN)
#define VPAD_MASK_BUTTONS               ~VPAD_MASK_EMULATED_STICKS

#ifdef __cplusplus
}
#endif
