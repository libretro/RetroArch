/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Gregor Richards
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

/* This is an X-include file which defines the mapping of keycodes used by
 * libretro to keycodes used by RetroArch Netplay. The keycodes are different
 * because the keycodes supported by libretro are in discontiguous blocks,
 * which would create gaps in the input data, requiring more space and more
 * network bandwidth to represent them.
 *
 * If you want to add a new keycode, make sure you add it to the END. The order
 * that the keys appear in this file defines their indices in the netplay
 * protocol, so adding a key in the middle will break backwards compatibility.
 * If you want to clean up the order and thereby break backwards compatibility,
 * make sure you bump the protocol version in netplay_private.h.
 */

K(BACKSPACE)
K(TAB)
KL(LINEFEED, 10)
K(CLEAR)
K(RETURN)
K(PAUSE)
K(ESCAPE)
K(SPACE)
K(EXCLAIM)
K(QUOTEDBL)
K(HASH)
K(DOLLAR)
K(AMPERSAND)
K(QUOTE)
K(LEFTPAREN)
K(RIGHTPAREN)
K(ASTERISK)
K(PLUS)
K(COMMA)
K(MINUS)
K(PERIOD)
K(SLASH)
K(0)
K(1)
K(2)
K(3)
K(4)
K(5)
K(6)
K(7)
K(8)
K(9)
K(COLON)
K(SEMICOLON)
K(LESS)
K(EQUALS)
K(GREATER)
K(QUESTION)
K(AT)
K(LEFTBRACKET)
K(BACKSLASH)
K(RIGHTBRACKET)
K(CARET)
K(UNDERSCORE)
K(BACKQUOTE)
K(a)
K(b)
K(c)
K(d)
K(e)
K(f)
K(g)
K(h)
K(i)
K(j)
K(k)
K(l)
K(m)
K(n)
K(o)
K(p)
K(q)
K(r)
K(s)
K(t)
K(u)
K(v)
K(w)
K(x)
K(y)
K(z)
K(DELETE)

K(KP0)
K(KP1)
K(KP2)
K(KP3)
K(KP4)
K(KP5)
K(KP6)
K(KP7)
K(KP8)
K(KP9)
K(KP_PERIOD)
K(KP_DIVIDE)
K(KP_MULTIPLY)
K(KP_MINUS)
K(KP_PLUS)
K(KP_ENTER)
K(KP_EQUALS)

K(UP)
K(DOWN)
K(RIGHT)
K(LEFT)
K(INSERT)
K(HOME)
K(END)
K(PAGEUP)
K(PAGEDOWN)

K(F1)
K(F2)
K(F3)
K(F4)
K(F5)
K(F6)
K(F7)
K(F8)
K(F9)
K(F10)
K(F11)
K(F12)
K(F13)
K(F14)
K(F15)

K(NUMLOCK)
K(CAPSLOCK)
K(SCROLLOCK)
K(RSHIFT)
K(LSHIFT)
K(RCTRL)
K(LCTRL)
K(RALT)
K(LALT)
K(RMETA)
K(LMETA)
K(LSUPER)
K(RSUPER)
K(MODE)
K(COMPOSE)

K(HELP)
K(PRINT)
K(SYSREQ)
K(BREAK)
K(MENU)
K(POWER)
K(EURO)
K(UNDO)
