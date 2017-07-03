// 30 march 2014
#import "uipriv_darwin.h"

/*
Mac OS X uses its own set of hardware key codes that are different from PC keyboard scancodes, but are positional (like PC keyboard scancodes). These are defined in <HIToolbox/Events.h>, a Carbon header. As far as I can tell, there's no way to include this header without either using an absolute path or linking Carbon into the program, so the constant values are used here instead.

The Cocoa docs do guarantee that -[NSEvent keyCode] results in key codes that are the same as those returned by Carbon; that is, these codes.
*/

// use uintptr_t to be safe
static const struct {
	uintptr_t keycode;
	char equiv;
} keycodeKeys[] = {
	{ 0x00, 'a' },
	{ 0x01, 's' },
	{ 0x02, 'd' },
	{ 0x03, 'f' },
	{ 0x04, 'h' },
	{ 0x05, 'g' },
	{ 0x06, 'z' },
	{ 0x07, 'x' },
	{ 0x08, 'c' },
	{ 0x09, 'v' },
	{ 0x0B, 'b' },
	{ 0x0C, 'q' },
	{ 0x0D, 'w' },
	{ 0x0E, 'e' },
	{ 0x0F, 'r' },
	{ 0x10, 'y' },
	{ 0x11, 't' },
	{ 0x12, '1' },
	{ 0x13, '2' },
	{ 0x14, '3' },
	{ 0x15, '4' },
	{ 0x16, '6' },
	{ 0x17, '5' },
	{ 0x18, '=' },
	{ 0x19, '9' },
	{ 0x1A, '7' },
	{ 0x1B, '-' },
	{ 0x1C, '8' },
	{ 0x1D, '0' },
	{ 0x1E, ']' },
	{ 0x1F, 'o' },
	{ 0x20, 'u' },
	{ 0x21, '[' },
	{ 0x22, 'i' },
	{ 0x23, 'p' },
	{ 0x25, 'l' },
	{ 0x26, 'j' },
	{ 0x27, '\'' },
	{ 0x28, 'k' },
	{ 0x29, ';' },
	{ 0x2A, '\\' },
	{ 0x2B, ',' },
	{ 0x2C, '/' },
	{ 0x2D, 'n' },
	{ 0x2E, 'm' },
	{ 0x2F, '.' },
	{ 0x32, '`' },
	{ 0x24, '\n' },
	{ 0x30, '\t' },
	{ 0x31, ' ' },
	{ 0x33, '\b' },
	{ 0xFFFF, 0 },
};

static const struct {
	uintptr_t keycode;
	uiExtKey equiv;
} keycodeExtKeys[] = {
	{ 0x41, uiExtKeyNDot },
	{ 0x43, uiExtKeyNMultiply },
	{ 0x45, uiExtKeyNAdd },
	{ 0x4B, uiExtKeyNDivide },
	{ 0x4C, uiExtKeyNEnter },
	{ 0x4E, uiExtKeyNSubtract },
	{ 0x52, uiExtKeyN0 },
	{ 0x53, uiExtKeyN1 },
	{ 0x54, uiExtKeyN2 },
	{ 0x55, uiExtKeyN3 },
	{ 0x56, uiExtKeyN4 },
	{ 0x57, uiExtKeyN5 },
	{ 0x58, uiExtKeyN6 },
	{ 0x59, uiExtKeyN7 },
	{ 0x5B, uiExtKeyN8 },
	{ 0x5C, uiExtKeyN9 },
	{ 0x35, uiExtKeyEscape },
	{ 0x60, uiExtKeyF5 },
	{ 0x61, uiExtKeyF6 },
	{ 0x62, uiExtKeyF7 },
	{ 0x63, uiExtKeyF3 },
	{ 0x64, uiExtKeyF8 },
	{ 0x65, uiExtKeyF9 },
	{ 0x67, uiExtKeyF11 },
	{ 0x6D, uiExtKeyF10 },
	{ 0x6F, uiExtKeyF12 },
	{ 0x72, uiExtKeyInsert },		// listed as the Help key but it's in the same position on an Apple keyboard as the Insert key on a Windows keyboard; thanks to SeanieB from irc.badnik.net and Psy in irc.freenode.net/#macdev for confirming they have the same code
	{ 0x73, uiExtKeyHome },
	{ 0x74, uiExtKeyPageUp },
	{ 0x75, uiExtKeyDelete },
	{ 0x76, uiExtKeyF4 },
	{ 0x77, uiExtKeyEnd },
	{ 0x78, uiExtKeyF2 },
	{ 0x79, uiExtKeyPageDown },
	{ 0x7A, uiExtKeyF1 },
	{ 0x7B, uiExtKeyLeft },
	{ 0x7C, uiExtKeyRight },
	{ 0x7D, uiExtKeyDown },
	{ 0x7E, uiExtKeyUp },
	{ 0xFFFF, 0 },
};

static const struct {
	uintptr_t keycode;
	uiModifiers equiv;
} keycodeModifiers[] = {
	{ 0x37, uiModifierSuper },		// left command
	{ 0x38, uiModifierShift },		// left shift
	{ 0x3A, uiModifierAlt },		// left option
	{ 0x3B, uiModifierCtrl },		// left control
	{ 0x3C, uiModifierShift },		// right shift
	{ 0x3D, uiModifierAlt },		// right alt
	{ 0x3E, uiModifierCtrl },		// right control
	// the following is not in Events.h for some reason
	// thanks to Nicole and jedivulcan from irc.badnik.net
	{ 0x36, uiModifierSuper },		// right command
	{ 0xFFFF, 0 },
};

BOOL fromKeycode(unsigned short keycode, uiAreaKeyEvent *ke)
{
	int i;

	for (i = 0; keycodeKeys[i].keycode != 0xFFFF; i++)
		if (keycodeKeys[i].keycode == keycode) {
			ke->Key = keycodeKeys[i].equiv;
			return YES;
		}
	for (i = 0; keycodeExtKeys[i].keycode != 0xFFFF; i++)
		if (keycodeExtKeys[i].keycode == keycode) {
			ke->ExtKey = keycodeExtKeys[i].equiv;
			return YES;
		}
	return NO;
}

BOOL keycodeModifier(unsigned short keycode, uiModifiers *mod)
{
	int i;

	for (i = 0; keycodeModifiers[i].keycode != 0xFFFF; i++)
		if (keycodeModifiers[i].keycode == keycode) {
			*mod = keycodeModifiers[i].equiv;
			return YES;
		}
	return NO;
}
