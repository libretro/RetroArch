#ifndef OPENXR_INPUT_H
#define OPENXR_INPUT_H

#include <stdint.h>

#include <boolean.h>
#include <openxr/openxr.h>

#include <libretro.h>

bool openxr_input_init(XrInstance instance);
bool openxr_input_attach(XrSession session);
void openxr_input_sync(XrSession session);
void openxr_input_deinit(void);
/* True while actions are attached to a live session; readable from the
 * main thread while sync runs on the video thread. */
bool openxr_input_session_active(void);
bool openxr_input_button(unsigned button);
int16_t openxr_input_axis(unsigned axis);
bool openxr_input_menu_long_press(void);

#endif
