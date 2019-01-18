/*-------------------------------------------------------------

keyboard.c -- keyboard event system

Copyright (C) 2008, 2009
DAVY Guillaume davyg2@gmail.com
Brian Johnson brijohn@gmail.com
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/iosupport.h>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/lwp_queue.h>

#include <wiikeyboard/usbkeyboard.h>
#include <wiikeyboard/keyboard.h>

#include "wsksymvar.h"

#define KBD_THREAD_STACKSIZE (1024 * 4)
#define KBD_THREAD_PRIO 64
#define KBD_THREAD_UDELAY (1000 * 10)
#define KBD_THREAD_KBD_SCAN_INTERVAL (3 * 100)

static lwp_queue _queue;
static lwp_t _kbd_thread = LWP_THREAD_NULL;
static lwp_t _kbd_buf_thread = LWP_THREAD_NULL;
static bool _kbd_thread_running = false;
static bool _kbd_thread_quit = false;

keysym_t ksym_upcase(keysym_t);

extern const struct wscons_keydesc ukbd_keydesctab[];

static struct wskbd_mapdata _ukbd_keymapdata = {
	ukbd_keydesctab,
	KB_NONE
};

struct nameint {
	int value;
	char *name;
};

static struct nameint kbdenc_tab[] = {
	KB_ENCTAB
};

static struct nameint kbdvar_tab[] = {
	KB_VARTAB
};

static int _sc_maplen = 0;					/* number of entries in sc_map */
static struct wscons_keymap *_sc_map = 0;	/* current translation map */

static u16 _modifiers;

static int _composelen;		/* remaining entries in _composebuf */
static keysym_t _composebuf[2];

typedef struct {
	u8 keycode;
	u16 symbol;
} _keyheld;

#define MAXHELD 8
static _keyheld _held[MAXHELD];

typedef struct {
	lwp_node node;
	keyboard_event event;
} _node;

static keyPressCallback _readKey_cb = NULL;

static u8 *_kbd_stack[KBD_THREAD_STACKSIZE] ATTRIBUTE_ALIGN(8);
static u8 *_kbd_buf_stack[KBD_THREAD_STACKSIZE] ATTRIBUTE_ALIGN(8);

static kbd_t _get_keymap_by_name(const char *identifier) {
	char name[64];
	u8 i, j;
	kbd_t encoding, variant;

	kbd_t res = KB_NONE;

	if (!identifier || (strlen(identifier) < 2))
		return res;

	i = 0;
	for (i = 0; ukbd_keydesctab[i].name != 0; ++i) {
		if (ukbd_keydesctab[i].name & KB_HANDLEDBYWSKBD)
			continue;

		encoding = KB_ENCODING(ukbd_keydesctab[i].name);
		variant = KB_VARIANT(ukbd_keydesctab[i].name);

		name[0] = 0;
		for (j = 0; j < sizeof(kbdenc_tab) / sizeof(struct nameint); ++j)
			if (encoding == kbdenc_tab[j].value) {
				strcpy(name, kbdenc_tab[j].name);
				break;
			}

		if (strlen(name) < 1)
			continue;

		for (j = 0; j < sizeof(kbdvar_tab) / sizeof(struct nameint); ++j)
			if (variant & kbdvar_tab[j].value) {
				strcat(name, "-");
				strcat(name,  kbdvar_tab[j].name);
			}

		if (!strcmp(identifier, name)) {
			res = ukbd_keydesctab[i].name;
			break;
		}
	}

	return res;
}

//Add an event to the event queue
static s32 _kbd_addEvent(const keyboard_event *event) {
	_node *n = malloc(sizeof(_node));
	n->event = *event;

	__lwp_queue_append(&_queue, (lwp_node*) n);

	return 1;
}

void update_modifier(u_int type, int toggle, int mask) {
	if (toggle) {
		if (type == KEYBOARD_PRESSED)
			_modifiers ^= mask;
	} else {
		if (type == KEYBOARD_RELEASED)
			_modifiers &= ~mask;
		else
			_modifiers |= mask;
	}
}

//Event callback, gets called when an event occurs in usbkeyboard
static void _kbd_event_cb(USBKeyboard_event kevent)
{
	keyboard_event event;
	struct wscons_keymap kp;
	keysym_t *group;
	int gindex;
	keysym_t ksym;
	int i;

	switch (kevent.type) {
	case USBKEYBOARD_DISCONNECTED:
		event.type = KEYBOARD_DISCONNECTED;
		event.modifiers = 0;
		event.keycode = 0;
		event.symbol = 0;

		_kbd_addEvent(&event);

		return;

	case USBKEYBOARD_PRESSED:
		event.type = KEYBOARD_PRESSED;
		break;

	case USBKEYBOARD_RELEASED:
		event.type = KEYBOARD_RELEASED;
		break;

	default:
		return;
	}

	event.keycode = kevent.keyCode;

	wskbd_get_mapentry(&_ukbd_keymapdata, event.keycode, &kp);

	/* Now update modifiers */
	switch (kp.group1[0]) {
	case KS_Shift_L:
		update_modifier(event.type, 0, MOD_SHIFT_L);
		break;

	case KS_Shift_R:
		update_modifier(event.type, 0, MOD_SHIFT_R);
		break;

	case KS_Shift_Lock:
		update_modifier(event.type, 1, MOD_SHIFTLOCK);
		break;

	case KS_Caps_Lock:
		update_modifier(event.type, 1, MOD_CAPSLOCK);
		USBKeyboard_SetLed(USBKEYBOARD_LEDCAPS,
							MOD_ONESET(_modifiers, MOD_CAPSLOCK));
		break;

	case KS_Control_L:
		update_modifier(event.type, 0, MOD_CONTROL_L);
		break;

	case KS_Control_R:
		update_modifier(event.type, 0, MOD_CONTROL_R);
		break;

	case KS_Alt_L:
		update_modifier(event.type, 0, MOD_META_L);
		break;

	case KS_Alt_R:
		update_modifier(event.type, 0, MOD_META_R);
		break;

	case KS_Mode_switch:
		update_modifier(event.type, 0, MOD_MODESHIFT);
		break;

	case KS_Mode_Lock:
		update_modifier(event.type, 1, MOD_MODELOCK);
		break;

	case KS_Num_Lock:
		update_modifier(event.type, 1, MOD_NUMLOCK);
		USBKeyboard_SetLed(USBKEYBOARD_LEDNUM,
							MOD_ONESET(_modifiers, MOD_NUMLOCK));
		break;

	case KS_Hold_Screen:
		update_modifier(event.type, 1, MOD_HOLDSCREEN);
		USBKeyboard_SetLed(USBKEYBOARD_LEDSCROLL,
							MOD_ONESET(_modifiers, MOD_HOLDSCREEN));
		break;
	}

	/* Get the keysym */
	if (_modifiers & (MOD_MODESHIFT|MOD_MODELOCK) &&
	    !MOD_ONESET(_modifiers, MOD_ANYCONTROL))
		group = &kp.group2[0];
	else
		group = &kp.group1[0];

	if ((_modifiers & MOD_NUMLOCK) &&
	    KS_GROUP(group[1]) == KS_GROUP_Keypad) {
		gindex = !MOD_ONESET(_modifiers, MOD_ANYSHIFT);
		ksym = group[gindex];
	} else {
		/* CAPS alone should only affect letter keys */
		if ((_modifiers & (MOD_CAPSLOCK | MOD_ANYSHIFT)) ==
		    MOD_CAPSLOCK) {
			gindex = 0;
			ksym = ksym_upcase(group[0]);
		} else {
			gindex = MOD_ONESET(_modifiers, MOD_ANYSHIFT);
			ksym = group[gindex];
		}
	}

	/* Process compose sequence and dead accents */
	switch (KS_GROUP(ksym)) {
	case KS_GROUP_Mod:
		if (ksym == KS_Multi_key) {
			update_modifier(KEYBOARD_PRESSED, 0, MOD_COMPOSE);
			_composelen = 2;
		}
		break;

	case KS_GROUP_Dead:
		if (event.type != KEYBOARD_PRESSED)
			return;

		if (_composelen == 0) {
			update_modifier(KEYBOARD_PRESSED, 0, MOD_COMPOSE);
			_composelen = 1;
			_composebuf[0] = ksym;

			return;
		}
		break;
	}

	if ((event.type == KEYBOARD_PRESSED) && (_composelen > 0)) {
		/*
		 * If the compose key also serves as AltGr (i.e. set to both
		 * KS_Multi_key and KS_Mode_switch), and would provide a valid,
		 * distinct combination as AltGr, leave compose mode.
		 */
		if (_composelen == 2 && group == &kp.group2[0]) {
			if (kp.group1[gindex] != kp.group2[gindex])
				_composelen = 0;
		}

		if (_composelen != 0) {
			_composebuf[2 - _composelen] = ksym;
			if (--_composelen == 0) {
				ksym = wskbd_compose_value(_composebuf);
				update_modifier(KEYBOARD_RELEASED, 0, MOD_COMPOSE);
			} else {
				return;
			}
		}
	}

	// store up to MAXHELD pressed events to match the symbol for release
	switch (KS_GROUP(ksym)) {
	case KS_GROUP_Ascii:
	case KS_GROUP_Keypad:
	case KS_GROUP_Function:
		if (event.type == KEYBOARD_PRESSED) {
			for (i = 0; i < MAXHELD; ++i) {
				if (_held[i].keycode == 0) {
					_held[i].keycode = event.keycode;
					_held[i].symbol = ksym;

					break;
				}
			}
		} else {
			for (i = 0; i < MAXHELD; ++i) {
				if (_held[i].keycode == event.keycode) {
					ksym = _held[i].symbol;
					_held[i].keycode = 0;
					_held[i].symbol = 0;

					break;
				}
			}
		}

		break;
	}

	event.symbol = ksym;
	event.modifiers = _modifiers;

	_kbd_addEvent(&event);

	return;
}

//This function call usb function to check if a new keyboard is connected
static s32 _kbd_scan_for_keyboard(void)
{
	s32 ret;
	keyboard_event event;

	ret = USBKeyboard_Open(&_kbd_event_cb);

	if (ret < 0)
		return ret;

	_modifiers = 0;
	_composelen = 0;
	memset(_held, 0, sizeof(_held));

	USBKeyboard_SetLed(USBKEYBOARD_LEDNUM, true);
	USBKeyboard_SetLed(USBKEYBOARD_LEDCAPS, true);
	USBKeyboard_SetLed(USBKEYBOARD_LEDSCROLL, true);
	usleep(200 * 1000);
	USBKeyboard_SetLed(USBKEYBOARD_LEDNUM, false);
	USBKeyboard_SetLed(USBKEYBOARD_LEDCAPS, false);
	USBKeyboard_SetLed(USBKEYBOARD_LEDSCROLL, false);

	event.type = KEYBOARD_CONNECTED;
	event.modifiers = 0;
	event.keycode = 0;

	_kbd_addEvent(&event);

	return ret;
}

static void * _kbd_thread_func(void *arg) {
	u32 turns = 0;

	while (!_kbd_thread_quit) {
		// scan for new attached keyboards
		if ((turns % KBD_THREAD_KBD_SCAN_INTERVAL) == 0) {
			if (!USBKeyboard_IsConnected())
				_kbd_scan_for_keyboard();

			turns = 0;
		}
		turns++;

		USBKeyboard_Scan();
		usleep(KBD_THREAD_UDELAY);
	}

	return NULL;
}

struct {
	vu8 head;
	vu8 tail;
	char buf[256];
} _keyBuffer;

static void * _kbd_buf_thread_func(void *arg) {
	keyboard_event event;
	while (!_kbd_thread_quit) {
		if (((_keyBuffer.tail+1)&255) != _keyBuffer.head) {
			if ( KEYBOARD_GetEvent(&event)) {
				if (event.type == KEYBOARD_PRESSED) {
					_keyBuffer.buf[_keyBuffer.tail] = event.symbol;
					_keyBuffer.tail++;
				}
			}
		}
		usleep(KBD_THREAD_UDELAY);
	}
	return NULL;
}

static ssize_t _keyboardRead(struct _reent *r, void *unused, char *ptr, size_t len)
{
	ssize_t count = len;
	while ( count > 0 ) {
		if (_keyBuffer.head != _keyBuffer.tail) {
			char key = _keyBuffer.buf[_keyBuffer.head];
			*ptr++ = key;
			if (_readKey_cb != NULL) _readKey_cb(key);
			_keyBuffer.head++;
			count--;
		}
	}
	return len;
}

static const devoptab_t std_in =
{
	"stdin",
	0,
	NULL,
	NULL,
	NULL,
	_keyboardRead,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

//Initialize USB and USB_KEYBOARD and the event queue
s32 KEYBOARD_Init(keyPressCallback keypress_cb)
{
	int fd;
	struct stat st;
	char keymap[64];
	size_t i;

	if (USB_Initialize() != IPC_OK)
		return -1;

	if (USBKeyboard_Initialize() != IPC_OK) {
		return -2;
	}

	if (_ukbd_keymapdata.layout == KB_NONE) {
		keymap[0] = 0;
		fd = open("/wiikbd.map", O_RDONLY);

		if ((fd > 0) && !fstat(fd, &st)) {
			if ((st.st_size > 0) && (st.st_size < 64) &&
				(st.st_size == read(fd, keymap, st.st_size))) {
				keymap[63] = 0;
				for (i = 0; i < 64; ++i) {
					if ((keymap[i] != '-') && (isalpha((int)keymap[i]) == 0)) {
						keymap[i] = 0;
						break;
					}
				}
			}

			close(fd);
		}

		_ukbd_keymapdata.layout = _get_keymap_by_name(keymap);
	}

	if (_ukbd_keymapdata.layout == KB_NONE) {
		switch (CONF_GetLanguage()) {
		case CONF_LANG_GERMAN:
			_ukbd_keymapdata.layout = KB_DE;
			break;

		case CONF_LANG_JAPANESE:
			_ukbd_keymapdata.layout = KB_JP;
			break;

		case CONF_LANG_FRENCH:
			_ukbd_keymapdata.layout = KB_FR;
			break;

		case CONF_LANG_SPANISH:
			_ukbd_keymapdata.layout = KB_ES;
			break;

		case CONF_LANG_ITALIAN:
			_ukbd_keymapdata.layout = KB_IT;
			break;

		case CONF_LANG_DUTCH:
			_ukbd_keymapdata.layout = KB_NL;
			break;

		case CONF_LANG_SIMP_CHINESE:
		case CONF_LANG_TRAD_CHINESE:
		case CONF_LANG_KOREAN:
		default:
			_ukbd_keymapdata.layout = KB_US;
			break;
		}
	}

	if (wskbd_load_keymap(&_ukbd_keymapdata, &_sc_map, &_sc_maplen) < 0) {
		_ukbd_keymapdata.layout = KB_NONE;

		return -4;
	}

	__lwp_queue_init_empty(&_queue);

	if (!_kbd_thread_running) {
		// start the keyboard thread
		_kbd_thread_quit = false;

		memset(_kbd_stack, 0, KBD_THREAD_STACKSIZE);

		s32 res = LWP_CreateThread(&_kbd_thread, _kbd_thread_func, NULL,
									_kbd_stack, KBD_THREAD_STACKSIZE,
									KBD_THREAD_PRIO);

		if (res) {
			USBKeyboard_Close();

			return -6;
		}

		if(keypress_cb)
		{
			_keyBuffer.head = 0;
			_keyBuffer.tail = 0;

			res = LWP_CreateThread(&_kbd_buf_thread, _kbd_buf_thread_func, NULL,
									_kbd_buf_stack, KBD_THREAD_STACKSIZE,
									KBD_THREAD_PRIO);
			if(res) {
				_kbd_thread_quit = true;

				LWP_JoinThread(_kbd_thread, NULL);

				USBKeyboard_Close();
				KEYBOARD_FlushEvents();
				USBKeyboard_Deinitialize();
				_kbd_thread_running = false;
				return -6;
			}

			devoptab_list[STD_IN] = &std_in;
			setvbuf(stdin, NULL , _IONBF, 0);
			_readKey_cb = keypress_cb;
		}
		_kbd_thread_running = true;
	}
	return 0;
}

//Deinitialize USB and USB_KEYBOARD and the event queue
s32 KEYBOARD_Deinit(void)
{
	if (_kbd_thread_running) {
		_kbd_thread_quit = true;

		if(_kbd_thread != LWP_THREAD_NULL)
			LWP_JoinThread(_kbd_thread, NULL);
		if(_kbd_buf_thread != LWP_THREAD_NULL)
			LWP_JoinThread(_kbd_buf_thread, NULL);

		_kbd_thread_running = false;
	}

	USBKeyboard_Close();
	KEYBOARD_FlushEvents();
	USBKeyboard_Deinitialize();

	if (_sc_map) {
		free(_sc_map);
		_sc_map = NULL;
		_sc_maplen = 0;
	}

	return 1;
}

//Get the first event of the event queue
s32 KEYBOARD_GetEvent(keyboard_event *event)
{
	_node *n = (_node *) __lwp_queue_get(&_queue);

	if (!n)
		return 0;

	*event = n->event;

	free(n);

	return 1;
}

//Flush all pending events
s32 KEYBOARD_FlushEvents(void)
{
	s32 res;
	_node *n;

	res = 0;
	while (true) {
		n = (_node *) __lwp_queue_get(&_queue);

		if (!n)
			break;

		free(n);
		res++;
	}

	return res;
}
