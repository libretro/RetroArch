// 29 march 2014
#include "../ui.h"
#include "uipriv.h"

/*
Windows and GTK+ have a limit of 2 and 3 clicks, respectively, natively supported. Fortunately, we can simulate the double/triple-click behavior to build higher-order clicks. We can use the same algorithm Windows uses on both:
	http://blogs.msdn.com/b/oldnewthing/archive/2004/10/18/243925.aspx
For GTK+, we pull the double-click time and double-click distance, which work the same as the equivalents on Windows (so the distance is in all directions), from the GtkSettings system.

On GTK+ this will also allow us to discard the GDK_BUTTON_2PRESS and GDK_BUTTON_3PRESS events, so the button press stream will be just like on other platforms.

Thanks to mclasen, garnacho_, halfline, and tristan in irc.gimp.net/#gtk+.
*/

// x, y, xdist, ydist, and c.rect must have the same units
// so must time, maxTime, and c.prevTime
int clickCounterClick(clickCounter *c, int button, int x, int y, uintptr_t time, uintptr_t maxTime, int32_t xdist, int32_t ydist)
{
	// different button than before? if so, don't count
	if (button != c->curButton)
		c->count = 0;

	// (x, y) in the allowed region for a double-click? if not, don't count
	if (x < c->rectX0)
		c->count = 0;
	if (y < c->rectY0)
		c->count = 0;
	if (x >= c->rectX1)
		c->count = 0;
	if (y >= c->rectY1)
		c->count = 0;

	// too slow? if so, don't count
	// note the below expression; time > (c.prevTime + maxTime) can overflow!
	if ((time - c->prevTime) > maxTime)	// too slow; don't count
		c->count = 0;

	c->count++;		// if either of the above ifs happened, this will make the click count 1; otherwise it will make the click count 2, 3, 4, 5, ...

	// now we need to update the internal structures for the next test
	c->curButton = button;
	c->prevTime = time;
	c->rectX0 = x - xdist;
	c->rectY0 = y - ydist;
	c->rectX1 = x + xdist;
	c->rectY1 = y + ydist;

	return c->count;
}

void clickCounterReset(clickCounter *c)
{
	c->curButton = 0;
	c->rectX0 = 0;
	c->rectY0 = 0;
	c->rectX1 = 0;
	c->rectY1 = 0;
	c->prevTime = 0;
	c->count = 0;
}

/*
For position independence across international keyboard layouts, typewriter keys are read using scancodes (which are always set 1).
Windows provides the scancodes directly in the LPARAM.
GTK+ provides the scancodes directly from the underlying window system via GdkEventKey.hardware_keycode.
On X11, this is scancode + 8 (because X11 keyboard codes have a range of [8,255]).
Wayland is guaranteed to give the same result (thanks ebassi in irc.gimp.net/#gtk+).
On Linux, where evdev is used instead of polling scancodes directly from the keyboard, evdev's typewriter section key code constants are the same as scancodes anyway, so the rules above apply.
Typewriter section scancodes are the same across international keyboards with some exceptions that have been accounted for (see KeyEvent's documentation); see http://www.quadibloc.com/comp/scan.htm for details.
Non-typewriter keys can be handled safely using constants provided by the respective backend API.

Because GTK+ keysyms may or may not obey Num Lock, we also handle the 0-9 and . keys on the numeric keypad with scancodes (they match too).
*/

// use uintptr_t to be safe; the size of the scancode/hardware key code field on each platform is different
static const struct {
	uintptr_t scancode;
	char equiv;
} scancodeKeys[] = {
	{ 0x02, '1' },
	{ 0x03, '2' },
	{ 0x04, '3' },
	{ 0x05, '4' },
	{ 0x06, '5' },
	{ 0x07, '6' },
	{ 0x08, '7' },
	{ 0x09, '8' },
	{ 0x0A, '9' },
	{ 0x0B, '0' },
	{ 0x0C, '-' },
	{ 0x0D, '=' },
	{ 0x0E, '\b' },
	{ 0x0F, '\t' },
	{ 0x10, 'q' },
	{ 0x11, 'w' },
	{ 0x12, 'e' },
	{ 0x13, 'r' },
	{ 0x14, 't' },
	{ 0x15, 'y' },
	{ 0x16, 'u' },
	{ 0x17, 'i' },
	{ 0x18, 'o' },
	{ 0x19, 'p' },
	{ 0x1A, '[' },
	{ 0x1B, ']' },
	{ 0x1C, '\n' },
	{ 0x1E, 'a' },
	{ 0x1F, 's' },
	{ 0x20, 'd' },
	{ 0x21, 'f' },
	{ 0x22, 'g' },
	{ 0x23, 'h' },
	{ 0x24, 'j' },
	{ 0x25, 'k' },
	{ 0x26, 'l' },
	{ 0x27, ';' },
	{ 0x28, '\'' },
	{ 0x29, '`' },
	{ 0x2B, '\\' },
	{ 0x2C, 'z' },
	{ 0x2D, 'x' },
	{ 0x2E, 'c' },
	{ 0x2F, 'v' },
	{ 0x30, 'b' },
	{ 0x31, 'n' },
	{ 0x32, 'm' },
	{ 0x33, ',' },
	{ 0x34, '.' },
	{ 0x35, '/' },
	{ 0x39, ' ' },
	{ 0xFFFF, 0 },
};

static const struct {
	uintptr_t scancode;
	uiExtKey equiv;
} scancodeExtKeys[] = {
	{ 0x47, uiExtKeyN7 },
	{ 0x48, uiExtKeyN8 },
	{ 0x49, uiExtKeyN9 },
	{ 0x4B, uiExtKeyN4 },
	{ 0x4C, uiExtKeyN5 },
	{ 0x4D, uiExtKeyN6 },
	{ 0x4F, uiExtKeyN1 },
	{ 0x50, uiExtKeyN2 },
	{ 0x51, uiExtKeyN3 },
	{ 0x52, uiExtKeyN0 },
	{ 0x53, uiExtKeyNDot },
	{ 0xFFFF, 0 },
};

int fromScancode(uintptr_t scancode, uiAreaKeyEvent *ke)
{
	int i;

	for (i = 0; scancodeKeys[i].scancode != 0xFFFF; i++)
		if (scancodeKeys[i].scancode == scancode) {
			ke->Key = scancodeKeys[i].equiv;
			return 1;
		}
	for (i = 0; scancodeExtKeys[i].scancode != 0xFFFF; i++)
		if (scancodeExtKeys[i].scancode == scancode) {
			ke->ExtKey = scancodeExtKeys[i].equiv;
			return 1;
		}
	return 0;
}
