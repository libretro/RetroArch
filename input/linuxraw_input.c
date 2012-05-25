/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../driver.h"

#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>
#include <unistd.h>
#include "../general.h"
#include "linuxraw_input.h"
#include "rarch_sdl_input.h"

static long oldKbmd = 0xFFFF;
static struct termios oldTerm, newTerm;

struct key_bind
{
	uint8_t x;
	enum rarch_key sk;
};

static unsigned keysym_lut[SK_LAST];
static const struct key_bind lut_binds[] = {
	{ 1, SK_ESCAPE },
	{ 2, SK_1 },
	{ 3, SK_2 },
	{ 4, SK_3},
	{ 5, SK_4 },
	{ 6, SK_5 },
	{ 7, SK_6 },
	{ 8, SK_7 },
	{ 9, SK_8 },
	{ 10, SK_9 },
	{ 11, SK_0 },
	{ 12, SK_MINUS },
	{ 13, SK_EQUALS },
	{ 14, SK_BACKSPACE },
	{ 15, SK_TAB },
	{ 16, SK_q },
	{ 17, SK_w },
	{ 18, SK_e },
	{ 19, SK_r },
	{ 20, SK_t },
	{ 21, SK_y },
	{ 22, SK_u },
	{ 23, SK_i },
	{ 24, SK_o },
	{ 25, SK_p },
	{ 26, SK_LEFTBRACKET },
	{ 27, SK_RIGHTBRACKET },
	{ 28, SK_RETURN },
	{ 29, SK_LCTRL },
	{ 30, SK_a },
	{ 31, SK_s },
	{ 32, SK_d },
	{ 33, SK_f },
	{ 34, SK_g },
	{ 35, SK_h },
	{ 36, SK_j },
	{ 37, SK_k },
	{ 38, SK_l },
	{ 39, SK_SEMICOLON },
	{ 40, SK_COMMA },
	{ 41, SK_BACKQUOTE },
	{ 42, SK_LSHIFT },
	{ 43, SK_BACKSLASH },
	{ 44, SK_z },
	{ 45, SK_x },
	{ 46, SK_c },
	{ 47, SK_v },
	{ 48, SK_b },
	{ 49, SK_n },
	{ 50, SK_m },
	{ 51, SK_COMMA },
	{ 52, SK_PERIOD },
	{ 53, SK_SLASH },
	{ 54, SK_RSHIFT },
	{ 55, SK_KP_MULTIPLY },
	{ 56, SK_LALT },
	{ 57, SK_SPACE },
	{ 58, SK_CAPSLOCK },
	{ 59, SK_F1 },
	{ 60, SK_F2 },
	{ 61, SK_F3 },
	{ 62, SK_F4 },
	{ 63, SK_F5 },
	{ 64, SK_F6 },
	{ 65, SK_F7 },
	{ 66, SK_F8 },
	{ 67, SK_F9 },
	{ 68, SK_F10 },
	{ 69, SK_NUMLOCK },
	{ 70, SK_SCROLLOCK },
	{ 71, SK_KP7 },
	{ 72, SK_KP8 },
	{ 73, SK_KP9 },
	{ 74, SK_KP_MINUS },
	{ 75, SK_KP4 },
	{ 76, SK_KP5 },
	{ 77, SK_KP6 },
	{ 78, SK_KP_PLUS },
	{ 79, SK_KP1 },
	{ 80, SK_KP2 },
	{ 81, SK_KP3 },
	{ 82, SK_KP0 },
	{ 83, SK_KP_PERIOD },

	{ 87, SK_F11 },
	{ 88, SK_F12 },

	{ 96, SK_KP_ENTER },
	{ 97, SK_RCTRL },
	{ 98, SK_KP_DIVIDE },
	{ 99, SK_PRINT },
	{ 100, SK_RALT },

	{ 102, SK_HOME },
	{ 103, SK_UP },
	{ 104, SK_PAGEUP },
	{ 105, SK_LEFT },
	{ 106, SK_RIGHT },
	{ 107, SK_END },
	{ 108, SK_DOWN },
	{ 109, SK_PAGEDOWN },
	{ 110, SK_INSERT },
	{ 111, SK_DELETE },

	{ 119, SK_PAUSE },
};

static void init_lut(void)
{
	memset(keysym_lut, 0, sizeof(keysym_lut));
	for (unsigned i = 0; i < sizeof(lut_binds) / sizeof(lut_binds[0]); i++)
		keysym_lut[lut_binds[i].sk] = lut_binds[i].x;
}

static void linuxraw_resetKbmd()
{
	if (oldKbmd != 0xFFFF)
	{
		ioctl(0, KDSKBMODE, oldKbmd);
		tcsetattr(0, TCSAFLUSH, &oldTerm);
		oldKbmd = 0xFFFF;
	}
}

static void *linuxraw_input_init(void)
{
	linuxraw_input_t *linuxraw = (linuxraw_input_t*)calloc(1, sizeof(*linuxraw));
	if (!linuxraw)
		return NULL;

	if (oldKbmd == 0xFFFF)
	{
		tcgetattr(0, &oldTerm);
		newTerm = oldTerm;
		newTerm.c_lflag &= ~(ECHO | ICANON | ISIG);
		newTerm.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
		newTerm.c_cc[VMIN] = 0;
		newTerm.c_cc[VTIME] = 0;

		if (ioctl(0, KDGKBMODE, &oldKbmd) != 0)
			return NULL;
	}

	tcsetattr(0, TCSAFLUSH, &newTerm);

	if (ioctl(0, KDSKBMODE, K_MEDIUMRAW) != 0)
	{
		linuxraw_resetKbmd();
		return NULL;
	}

	atexit(linuxraw_resetKbmd);

	linuxraw->sdl = (sdl_input_t*)input_sdl.init();
	if (!linuxraw->sdl)
	{
		free(linuxraw);
		return NULL;
	}

	init_lut();

	linuxraw->sdl->use_keyboard = false;
	return linuxraw;
}

static bool linuxraw_key_pressed(linuxraw_input_t *linuxraw, int key)
{
	return linuxraw->state[keysym_lut[key]];
}

static bool linuxraw_is_pressed(linuxraw_input_t *linuxraw, const struct snes_keybind *binds, unsigned id)
{
	if (id < RARCH_BIND_LIST_END)
	{
		const struct snes_keybind *bind = &binds[id];
		return bind->valid && linuxraw_key_pressed(linuxraw, binds[id].key);
	}
	else
		return false;
}

static bool linuxraw_bind_button_pressed(void *data, int key)
{
	linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
	return linuxraw_is_pressed(linuxraw, g_settings.input.binds[0], key) ||
		input_sdl.key_pressed(linuxraw->sdl, key);
}

static int16_t linuxraw_input_state(void *data, const struct snes_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
	linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

	switch (device)
	{
		case RETRO_DEVICE_JOYPAD:
			return linuxraw_is_pressed(linuxraw, binds[port], id) ||
				input_sdl.input_state(linuxraw->sdl, binds, port, device, index, id);

		default:
			return 0;
	}
}

static void linuxraw_input_free(void *data)
{
	linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
	input_sdl.free(linuxraw->sdl);
	linuxraw_resetKbmd();
	free(data);
}

static void linuxraw_input_poll(void *data)
{
	linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
	uint8_t c;

	while (read(0, &c, 1))
	{
		bool pressed = !(c & 0x80);
		c &= ~0x80;
		linuxraw->state[c] = pressed;
	}

	input_sdl.poll(linuxraw->sdl);
}

const input_driver_t input_linuxraw = {
	linuxraw_input_init,
	linuxraw_input_poll,
	linuxraw_input_state,
	linuxraw_bind_button_pressed,
	linuxraw_input_free,
	"linuxraw"
};
