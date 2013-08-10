#ifndef _FRONTENDQNX_H_
#define _FRONTENDQNX_H_

#define MAX_PADS 8

//Internal helper functions
typedef struct {
    // Static device info.
#ifdef HAVE_BB10
    screen_device_t handle;
#endif
    int type;
    int analogCount;
    int buttonCount;
    char id[64];
    char vendor[64];
    char product[64];

    char device_name[64];
    int device;
    int port;
    int index;

    // Current state.
    int buttons;
    int analog0[3];
    int analog1[3];
} input_device_t;

//Device struct to port mapping
extern input_device_t *port_device[MAX_PADS];

extern unsigned pads_connected;

#ifdef HAVE_BB10
const struct platform_bind platform_keys[] = {
   { SCREEN_A_GAME_BUTTON, "A button" },
   { SCREEN_B_GAME_BUTTON, "B button" },
   { SCREEN_C_GAME_BUTTON, "C button" },
   { SCREEN_X_GAME_BUTTON, "X button" },
   { SCREEN_Y_GAME_BUTTON, "Y button" },
   { SCREEN_Z_GAME_BUTTON, "Z button" },
   { SCREEN_MENU1_GAME_BUTTON, "Menu1 button" },
   { SCREEN_MENU2_GAME_BUTTON, "Menu2 button" },
   { SCREEN_MENU3_GAME_BUTTON, "Menu3 button" },
   { SCREEN_MENU4_GAME_BUTTON, "Menu4 button" },
   { SCREEN_L1_GAME_BUTTON, "L1 Button" },
   { SCREEN_L2_GAME_BUTTON, "L2 Button" },
   { SCREEN_L3_GAME_BUTTON, "L3 Button" },
   { SCREEN_R1_GAME_BUTTON, "R1 Button" },
   { SCREEN_R2_GAME_BUTTON, "R2 Button" },
   { SCREEN_R3_GAME_BUTTON, "R3 Button" },
   { SCREEN_DPAD_UP_GAME_BUTTON, "Up D-pad" },
   { SCREEN_DPAD_DOWN_GAME_BUTTON, "Down D-pad" },
   { SCREEN_DPAD_LEFT_GAME_BUTTON, "Left D-pad" },
   { SCREEN_DPAD_RIGHT_GAME_BUTTON, "Right D-pad" },
};
#endif

extern input_device_t devices[MAX_PADS];

#endif
