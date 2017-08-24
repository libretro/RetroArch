/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_WSCONS_WSKSYMDEF_H_
#define _DEV_WSCONS_WSKSYMDEF_H_

/*
 * Keysymbols encoded as 16-bit Unicode. Special symbols
 * are encoded in the private area (0xe000 - 0xf8ff).
 * Currently only ISO Latin-1 subset is supported.
 *
 * This file is parsed from userland. Encode keysyms as:
 *
 *	#define KS_[^ \t]* 0x[0-9a-f]*
 *
 * and don't modify the border comments.
 */

/*BEGINKEYSYMDECL*/

/*
 * Group Ascii (ISO Latin1) character in low byte
 */

#define	KS_BackSpace 		0x08
#define	KS_Tab 			0x09
#define	KS_Linefeed 		0x0a
#define	KS_Clear 		0x0b
#define	KS_Return 		0x0d
#define	KS_Escape 		0x1b
#define	KS_space 		0x20
#define	KS_exclam 		0x21
#define	KS_quotedbl 		0x22
#define	KS_numbersign 		0x23
#define	KS_dollar 		0x24
#define	KS_percent 		0x25
#define	KS_ampersand 		0x26
#define	KS_apostrophe 		0x27
#define	KS_parenleft 		0x28
#define	KS_parenright 		0x29
#define	KS_asterisk 		0x2a
#define	KS_plus 		0x2b
#define	KS_comma 		0x2c
#define	KS_minus 		0x2d
#define	KS_period 		0x2e
#define	KS_slash 		0x2f
#define	KS_0 			0x30
#define	KS_1 			0x31
#define	KS_2 			0x32
#define	KS_3 			0x33
#define	KS_4 			0x34
#define	KS_5 			0x35
#define	KS_6 			0x36
#define	KS_7 			0x37
#define	KS_8 			0x38
#define	KS_9 			0x39
#define	KS_colon 		0x3a
#define	KS_semicolon 		0x3b
#define	KS_less 		0x3c
#define	KS_equal 		0x3d
#define	KS_greater 		0x3e
#define	KS_question 		0x3f
#define	KS_at 			0x40
#define	KS_A 			0x41
#define	KS_B 			0x42
#define	KS_C 			0x43
#define	KS_D 			0x44
#define	KS_E 			0x45
#define	KS_F 			0x46
#define	KS_G 			0x47
#define	KS_H 			0x48
#define	KS_I 			0x49
#define	KS_J 			0x4a
#define	KS_K 			0x4b
#define	KS_L 			0x4c
#define	KS_M 			0x4d
#define	KS_N 			0x4e
#define	KS_O 			0x4f
#define	KS_P 			0x50
#define	KS_Q 			0x51
#define	KS_R 			0x52
#define	KS_S 			0x53
#define	KS_T 			0x54
#define	KS_U 			0x55
#define	KS_V 			0x56
#define	KS_W 			0x57
#define	KS_X 			0x58
#define	KS_Y 			0x59
#define	KS_Z 			0x5a
#define	KS_bracketleft 		0x5b
#define	KS_backslash 		0x5c
#define	KS_bracketright 	0x5d
#define	KS_asciicircum 		0x5e
#define	KS_underscore 		0x5f
#define	KS_grave 		0x60
#define	KS_a 			0x61
#define	KS_b 			0x62
#define	KS_c 			0x63
#define	KS_d 			0x64
#define	KS_e 			0x65
#define	KS_f 			0x66
#define	KS_g 			0x67
#define	KS_h 			0x68
#define	KS_i 			0x69
#define	KS_j 			0x6a
#define	KS_k 			0x6b
#define	KS_l 			0x6c
#define	KS_m 			0x6d
#define	KS_n 			0x6e
#define	KS_o 			0x6f
#define	KS_p 			0x70
#define	KS_q 			0x71
#define	KS_r 			0x72
#define	KS_s 			0x73
#define	KS_t 			0x74
#define	KS_u 			0x75
#define	KS_v 			0x76
#define	KS_w 			0x77
#define	KS_x 			0x78
#define	KS_y 			0x79
#define	KS_z 			0x7a
#define	KS_braceleft 		0x7b
#define	KS_bar 			0x7c
#define	KS_braceright 		0x7d
#define	KS_asciitilde 		0x7e
#define	KS_Delete 		0x7f

#define	KS_nobreakspace 	0xa0
#define	KS_exclamdown 		0xa1
#define	KS_cent 		0xa2
#define	KS_sterling 		0xa3
#define	KS_currency 		0xa4
#define	KS_yen 			0xa5
#define	KS_brokenbar 		0xa6
#define	KS_section 		0xa7
#define	KS_diaeresis 		0xa8
#define	KS_copyright 		0xa9
#define	KS_ordfeminine 		0xaa
#define	KS_guillemotleft 	0xab
#define	KS_notsign 		0xac
#define	KS_hyphen 		0xad
#define	KS_registered 		0xae
#define	KS_macron 		0xaf
#define	KS_degree 		0xb0
#define	KS_plusminus 		0xb1
#define	KS_twosuperior 		0xb2
#define	KS_threesuperior 	0xb3
#define	KS_acute 		0xb4
#define	KS_mu 			0xb5
#define	KS_paragraph 		0xb6
#define	KS_periodcentered 	0xb7
#define	KS_cedilla 		0xb8
#define	KS_onesuperior 		0xb9
#define	KS_masculine 		0xba
#define	KS_guillemotright 	0xbb
#define	KS_onequarter 		0xbc
#define	KS_onehalf 		0xbd
#define	KS_threequarters 	0xbe
#define	KS_questiondown 	0xbf
#define	KS_Agrave 		0xc0
#define	KS_Aacute 		0xc1
#define	KS_Acircumflex 		0xc2
#define	KS_Atilde 		0xc3
#define	KS_Adiaeresis 		0xc4
#define	KS_Aring 		0xc5
#define	KS_AE 			0xc6
#define	KS_Ccedilla 		0xc7
#define	KS_Egrave 		0xc8
#define	KS_Eacute 		0xc9
#define	KS_Ecircumflex 		0xca
#define	KS_Ediaeresis 		0xcb
#define	KS_Igrave 		0xcc
#define	KS_Iacute 		0xcd
#define	KS_Icircumflex 		0xce
#define	KS_Idiaeresis 		0xcf
#define	KS_ETH 			0xd0
#define	KS_Ntilde 		0xd1
#define	KS_Ograve 		0xd2
#define	KS_Oacute 		0xd3
#define	KS_Ocircumflex 		0xd4
#define	KS_Otilde 		0xd5
#define	KS_Odiaeresis 		0xd6
#define	KS_multiply 		0xd7
#define	KS_Ooblique 		0xd8
#define	KS_Ugrave 		0xd9
#define	KS_Uacute 		0xda
#define	KS_Ucircumflex 		0xdb
#define	KS_Udiaeresis 		0xdc
#define	KS_Yacute 		0xdd
#define	KS_THORN 		0xde
#define	KS_ssharp 		0xdf
#define	KS_agrave 		0xe0
#define	KS_aacute 		0xe1
#define	KS_acircumflex 		0xe2
#define	KS_atilde 		0xe3
#define	KS_adiaeresis 		0xe4
#define	KS_aring 		0xe5
#define	KS_ae 			0xe6
#define	KS_ccedilla 		0xe7
#define	KS_egrave 		0xe8
#define	KS_eacute 		0xe9
#define	KS_ecircumflex 		0xea
#define	KS_ediaeresis 		0xeb
#define	KS_igrave 		0xec
#define	KS_iacute 		0xed
#define	KS_icircumflex 		0xee
#define	KS_idiaeresis 		0xef
#define	KS_eth 			0xf0
#define	KS_ntilde 		0xf1
#define	KS_ograve 		0xf2
#define	KS_oacute 		0xf3
#define	KS_ocircumflex 		0xf4
#define	KS_otilde 		0xf5
#define	KS_odiaeresis 		0xf6
#define	KS_division 		0xf7
#define	KS_oslash 		0xf8
#define	KS_ugrave 		0xf9
#define	KS_uacute 		0xfa
#define	KS_ucircumflex 		0xfb
#define	KS_udiaeresis 		0xfc
#define	KS_yacute 		0xfd
#define	KS_thorn 		0xfe
#define	KS_ydiaeresis 		0xff

#define KS_Odoubleacute 	0x150
#define KS_odoubleacute 	0x151
#define KS_Udoubleacute 	0x170
#define KS_udoubleacute 	0x171

/*
 * Group Dead (dead accents)
 */

#define	KS_dead_grave 		0x0300
#define	KS_dead_acute 		0x0301
#define	KS_dead_circumflex 	0x0302
#define	KS_dead_tilde 		0x0303
#define	KS_dead_diaeresis 	0x0308
#define	KS_dead_abovering 	0x030a
#define	KS_dead_cedilla 	0x0327

/*
 * Group Cyrillic (koi8-r)
 */

#define KS_Cyrillic_A		0xe1
#define KS_Cyrillic_BE		0xe2
#define KS_Cyrillic_VE		0xf7
#define KS_Cyrillic_GE		0xe7
#define KS_Cyrillic_DE		0xe4
#define KS_Cyrillic_IE		0xe5
#define KS_Cyrillic_YO		0xb3
#define KS_Cyrillic_ZHE		0xf6
#define KS_Cyrillic_ZE		0xfa
#define KS_Cyrillic_I		0xe9
#define KS_Cyrillic_ISHORT	0xea
#define KS_Cyrillic_IUKR	0xb6
#define KS_Cyrillic_YI		0xb7
#define KS_Cyrillic_KA		0xeb
#define KS_Cyrillic_EL		0xec
#define KS_Cyrillic_EM		0xed
#define KS_Cyrillic_EN		0xee
#define KS_Cyrillic_O		0xef
#define KS_Cyrillic_PE		0xf0
#define KS_Cyrillic_ER		0xf2
#define KS_Cyrillic_ES		0xf3
#define KS_Cyrillic_TE		0xf4
#define KS_Cyrillic_U		0xf5
#define KS_Cyrillic_EF		0xe6
#define KS_Cyrillic_HA		0xe8
#define KS_Cyrillic_TSE		0xe3
#define KS_Cyrillic_CHE		0xfe
#define KS_Cyrillic_SHA		0xfb
#define KS_Cyrillic_SCHA	0xfd
#define KS_Cyrillic_HSIGHN	0xff
#define KS_Cyrillic_YERU	0xf9
#define KS_Cyrillic_SSIGHN	0xf8
#define KS_Cyrillic_E		0xfc
#define KS_Cyrillic_YU		0xe0
#define KS_Cyrillic_YA		0xf1
#define KS_Cyrillic_a		0xc1
#define KS_Cyrillic_be		0xc2
#define KS_Cyrillic_ve		0xd7
#define KS_Cyrillic_ge		0xc7
#define KS_Cyrillic_de		0xc4
#define KS_Cyrillic_ie		0xc5
#define KS_Cyrillic_yo		0xa3
#define KS_Cyrillic_zhe		0xd6
#define KS_Cyrillic_ze		0xda
#define KS_Cyrillic_i		0xc9
#define KS_Cyrillic_ishort	0xca
#define KS_Cyrillic_iukr	0xa6
#define KS_Cyrillic_yi		0xa7
#define KS_Cyrillic_ka		0xcb
#define KS_Cyrillic_el		0xcc
#define KS_Cyrillic_em		0xcd
#define KS_Cyrillic_en		0xce
#define KS_Cyrillic_o		0xcf
#define KS_Cyrillic_pe		0xd0
#define KS_Cyrillic_er		0xd2
#define KS_Cyrillic_es		0xd3
#define KS_Cyrillic_te		0xd4
#define KS_Cyrillic_u		0xd5
#define KS_Cyrillic_ef		0xc6
#define KS_Cyrillic_ha		0xc8
#define KS_Cyrillic_tse		0xc3
#define KS_Cyrillic_che		0xde
#define KS_Cyrillic_sha		0xdb
#define KS_Cyrillic_scha	0xdd
#define KS_Cyrillic_hsighn	0xdf
#define KS_Cyrillic_yeru	0xd9
#define KS_Cyrillic_ssighn	0xd8
#define KS_Cyrillic_e		0xdc
#define KS_Cyrillic_yu		0xc0
#define KS_Cyrillic_ya		0xd1
#define KS_Cyrillic_gheukr	0xad
#define KS_Cyrillic_GHEUKR	0xbd
#define KS_Cyrillic_yeukr	0xa4
#define KS_Cyrillic_YEUKR	0xb4

/*
 * Group Latin-2 (iso8859-2)
 */

#define KS_L2_Aogonek		0xa1
#define KS_L2_breve		0xa2
#define KS_L2_Lstroke		0xa3
#define KS_L2_Lcaron		0xa5
#define KS_L2_Sacute		0xa6
#define KS_L2_Scaron		0xa9
#define KS_L2_Scedilla		0xaa
#define KS_L2_Tcaron		0xab
#define KS_L2_Zacute		0xac
#define KS_L2_Zcaron		0xae
#define KS_L2_Zdotabove		0xaf
#define KS_L2_aogonek		0xb1
#define KS_L2_ogonek		0xb2
#define KS_L2_lstroke		0xb3
#define KS_L2_lcaron		0xb5
#define KS_L2_sacute		0xb6
#define KS_L2_caron		0xb7
#define KS_L2_scaron		0xb9
#define KS_L2_scedilla		0xba
#define KS_L2_tcaron		0xbb
#define KS_L2_zacute		0xbc
#define KS_L2_dblacute		0xbd
#define KS_L2_zcaron		0xbe
#define KS_L2_zdotabove		0xbf
#define KS_L2_Racute		0xc0
#define KS_L2_Abreve		0xc3
#define KS_L2_Lacute		0xc5
#define KS_L2_Cacute		0xc6
#define KS_L2_Ccaron		0xc8
#define KS_L2_Eogonek		0xca
#define KS_L2_Ecaron		0xcc
#define KS_L2_Dcaron		0xcf
#define KS_L2_Dstroke		0xd0
#define KS_L2_Nacute		0xd1
#define KS_L2_Ncaron		0xd2
#define KS_L2_Odoubleacute	0xd5
#define KS_L2_Rcaron		0xd8
#define KS_L2_Uring		0xd9
#define KS_L2_Udoubleacute	0xdb
#define KS_L2_Tcedilla		0xde
#define KS_L2_racute		0xe0
#define KS_L2_abreve		0xe3
#define KS_L2_lacute		0xe5
#define KS_L2_cacute		0xe6
#define KS_L2_ccaron		0xe8
#define KS_L2_eogonek		0xea
#define KS_L2_ecaron		0xec
#define KS_L2_dcaron		0xef
#define KS_L2_dstroke		0xf0
#define KS_L2_nacute		0xf1
#define KS_L2_odoubleacute	0xf5
#define KS_L2_rcaron		0xf8
#define KS_L2_uring		0xf9
#define KS_L2_udoubleacute	0xfb
#define KS_L2_tcedilla		0xfe
#define KS_L2_dotabove		0xff

/*
 * Group Latin-5 (iso8859-9)
 */

#define KS_L5_Gbreve		0xd0
#define KS_L5_Idotabove		0xdd
#define KS_L5_Scedilla		0xde
#define KS_L5_gbreve		0xf0
#define KS_L5_idotless		0xfd
#define KS_L5_scedilla		0xfe

 /*
 * Group Latin-7 (iso8859-13)
 */

#define KS_L7_rightdblquot     0xa1
#define KS_L7_dbllow9quot      0xa5
#define KS_L7_Ostroke          0xa8
#define KS_L7_Rcedilla         0xaa
#define KS_L7_AE               0xaf
#define KS_L7_leftdblquot      0xb4
#define KS_L7_ostroke          0xb8
#define KS_L7_rcedilla         0xba
#define KS_L7_ae               0xbf
#define KS_L7_Aogonek          0xc0
#define KS_L7_Iogonek          0xc1
#define KS_L7_Amacron          0xc2
#define KS_L7_Cacute           0xc3
#define KS_L7_Eogonek          0xc6
#define KS_L7_Emacron          0xc7
#define KS_L7_Ccaron           0xc8
#define KS_L7_Zacute           0xca
#define KS_L7_Edot             0xcb
#define KS_L7_Gcedilla         0xcc
#define KS_L7_Kcedilla         0xcd
#define KS_L7_Imacron          0xce
#define KS_L7_Lcedilla         0xcf
#define KS_L7_Scaron           0xd0
#define KS_L7_Nacute           0xd1
#define KS_L7_Ncedilla         0xd2
#define KS_L7_Omacron          0xd4
#define KS_L7_Uogonek          0xd8
#define KS_L7_Lstroke          0xd9
#define KS_L7_Sacute           0xda
#define KS_L7_Umacron          0xdb
#define KS_L7_Zdot             0xdd
#define KS_L7_Zcaron           0xde
#define KS_L7_aogonek          0xe0
#define KS_L7_iogonek          0xe1
#define KS_L7_amacron          0xe2
#define KS_L7_cacute           0xe3
#define KS_L7_eogonek          0xe6
#define KS_L7_emacron          0xe7
#define KS_L7_ccaron           0xe8
#define KS_L7_zacute           0xea
#define KS_L7_edot             0xeb
#define KS_L7_gcedilla         0xec
#define KS_L7_kcedilla         0xed
#define KS_L7_imacron          0xee
#define KS_L7_lcedilla         0xef
#define KS_L7_scaron           0xf0
#define KS_L7_nacute           0xf1
#define KS_L7_ncedilla         0xf2
#define KS_L7_omacron          0xf4
#define KS_L7_uogonek          0xf8
#define KS_L7_lstroke          0xf9
#define KS_L7_sacute           0xfa
#define KS_L7_umacron          0xfb
#define KS_L7_zdot             0xfd
#define KS_L7_zcaron           0xfe
#define KS_L7_rightsnglquot    0xff

/*
 * Group 1 (modifiers)
 */

#define	KS_Shift_L 		0xf101
#define	KS_Shift_R 		0xf102
#define	KS_Control_L 		0xf103
#define	KS_Control_R 		0xf104
#define	KS_Caps_Lock 		0xf105
#define	KS_Shift_Lock 		0xf106
#define	KS_Alt_L 		0xf107
#define	KS_Alt_R 		0xf108
#define	KS_Multi_key 		0xf109
#define	KS_Mode_switch 		0xf10a
#define	KS_Num_Lock 		0xf10b
#define KS_Hold_Screen		0xf10c
#define KS_Cmd			0xf10d
#define KS_Cmd1			0xf10e
#define KS_Cmd2			0xf10f
#define KS_Meta_L		0xf110
#define KS_Meta_R		0xf111
#define KS_Zenkaku_Hankaku	0xf112	/* Zenkaku/Hankaku toggle */
#define KS_Hiragana_Katakana	0xf113	/* Hiragana/Katakana toggle */
#define KS_Henkan_Mode		0xf114	/* Start/Stop Conversion */
#define KS_Henkan		0xf115	/* Alias for Henkan_Mode */
#define KS_Muhenkan		0xf116	/* Cancel Conversion */
#define KS_Mode_Lock		0xf117

/*
 * Group 2 (keypad) character in low byte
 */

#define	KS_KP_F1 		0xf291
#define	KS_KP_F2 		0xf292
#define	KS_KP_F3 		0xf293
#define	KS_KP_F4 		0xf294
#define	KS_KP_Home 		0xf295
#define	KS_KP_Left 		0xf296
#define	KS_KP_Up 		0xf297
#define	KS_KP_Right 		0xf298
#define	KS_KP_Down 		0xf299
#define	KS_KP_Prior 		0xf29a
#define	KS_KP_Next 		0xf29b
#define	KS_KP_End 		0xf29c
#define	KS_KP_Begin 		0xf29d
#define	KS_KP_Insert 		0xf29e
#define	KS_KP_Delete 		0xf29f

#define	KS_KP_Space 		0xf220
#define	KS_KP_Tab 		0xf209
#define	KS_KP_Enter 		0xf20d
#define	KS_KP_Equal 		0xf23d
#define	KS_KP_Numbersign	0xf223
#define	KS_KP_Multiply 		0xf22a
#define	KS_KP_Add 		0xf22b
#define	KS_KP_Separator 	0xf22c
#define	KS_KP_Subtract 		0xf22d
#define	KS_KP_Decimal 		0xf22e
#define	KS_KP_Divide 		0xf22f
#define	KS_KP_0 		0xf230
#define	KS_KP_1 		0xf231
#define	KS_KP_2 		0xf232
#define	KS_KP_3 		0xf233
#define	KS_KP_4 		0xf234
#define	KS_KP_5 		0xf235
#define	KS_KP_6 		0xf236
#define	KS_KP_7 		0xf237
#define	KS_KP_8 		0xf238
#define	KS_KP_9 		0xf239

/*
 * Group 3 (function)
 */

#define KS_f1			0xf300
#define KS_f2			0xf301
#define KS_f3			0xf302
#define KS_f4			0xf303
#define KS_f5			0xf304
#define KS_f6			0xf305
#define KS_f7			0xf306
#define KS_f8			0xf307
#define KS_f9			0xf308
#define KS_f10			0xf309
#define KS_f11			0xf30a
#define KS_f12			0xf30b
#define KS_f13			0xf30c
#define KS_f14			0xf30d
#define KS_f15			0xf30e
#define KS_f16			0xf30f
#define KS_f17			0xf310
#define KS_f18			0xf311
#define KS_f19			0xf312
#define KS_f20			0xf313

#define KS_F1			0xf340
#define KS_F2			0xf341
#define KS_F3			0xf342
#define KS_F4			0xf343
#define KS_F5			0xf344
#define KS_F6			0xf345
#define KS_F7			0xf346
#define KS_F8			0xf347
#define KS_F9			0xf348
#define KS_F10			0xf349
#define KS_F11			0xf34a
#define KS_F12			0xf34b
#define KS_F13			0xf34c
#define KS_F14			0xf34d
#define KS_F15			0xf34e
#define KS_F16			0xf34f
#define KS_F17			0xf350
#define KS_F18			0xf351
#define KS_F19			0xf352
#define KS_F20			0xf353

#define KS_Home			0xf381
#define KS_Prior		0xf382
#define KS_Next			0xf383
#define KS_Up			0xf384
#define KS_Down			0xf385
#define KS_Left			0xf386
#define KS_Right		0xf387
#define KS_End			0xf388
#define KS_Insert		0xf389
#define KS_Help			0xf38a
#define KS_Execute		0xf38b
#define KS_Find			0xf38c
#define KS_Select		0xf38d
#define KS_Again		0xf38e
#define KS_Props		0xf38f
#define KS_Undo			0xf390
#define KS_Front		0xf391
#define KS_Copy			0xf392
#define KS_Open			0xf393
#define KS_Paste		0xf394
#define KS_Cut			0xf395

#define KS_Menu			0xf3c0
#define KS_Pause		0xf3c1
#define KS_Print_Screen		0xf3c2

#define KS_AudioMute		0xf3d1
#define KS_AudioLower		0xf3d2
#define KS_AudioRaise		0xf3d3

/*
 * Group 4 (command)
 */

#define KS_Cmd_Screen0		0xf400
#define KS_Cmd_Screen1		0xf401
#define KS_Cmd_Screen2		0xf402
#define KS_Cmd_Screen3		0xf403
#define KS_Cmd_Screen4		0xf404
#define KS_Cmd_Screen5		0xf405
#define KS_Cmd_Screen6		0xf406
#define KS_Cmd_Screen7		0xf407
#define KS_Cmd_Screen8		0xf408
#define KS_Cmd_Screen9		0xf409
#define KS_Cmd_Screen10		0xf40a
#define KS_Cmd_Screen11		0xf40b
#define KS_Cmd_Debugger		0xf420
#define KS_Cmd_ResetEmul	0xf421
#define KS_Cmd_ResetClose	0xf422
#define KS_Cmd_BacklightOn	0xf423
#define KS_Cmd_BacklightOff	0xf424
#define KS_Cmd_BacklightToggle	0xf425
#define KS_Cmd_BrightnessUp	0xf426
#define KS_Cmd_BrightnessDown	0xf427
#define KS_Cmd_BrightnessRotate	0xf428
#define KS_Cmd_ContrastUp	0xf429
#define KS_Cmd_ContrastDown	0xf42a
#define KS_Cmd_ContrastRotate	0xf42b
#define KS_Cmd_ScrollBack	0xf42c
#define KS_Cmd_ScrollFwd	0xf42d
#define KS_Cmd_KbdReset		0xf42e

/*
 * Group 5 (internal)
 */

#define KS_voidSymbol		0xf500

/*ENDKEYSYMDECL*/

/*
 * keysym groups
 */

#define KS_GROUP_Mod		0xf100
#define KS_GROUP_Keypad		0xf200
#define KS_GROUP_Function	0xf300
#define KS_GROUP_Command	0xf400
#define KS_GROUP_Internal	0xf500
#define KS_GROUP_Dead		0xf801		/* not encoded in keysym */
#define KS_GROUP_Ascii		0xf802		/* not encoded in keysym */
#define KS_GROUP_Keycode	0xf803		/* not encoded in keysym */

#define KS_NUMKEYCODES	0x1000
#define KS_KEYCODE(v)	((v) | 0xe000)

#define KS_GROUP(k)	((k) >= 0x0300 && (k) < 0x0370 ? KS_GROUP_Dead : \
			    (((k) & 0xf000) == 0xe000 ? KS_GROUP_Keycode : \
			      (((k) & 0xf800) == 0xf000 ? ((k) & 0xff00) : \
				KS_GROUP_Ascii)))

#define KS_VALUE(k)	(((k) & 0xf000) == 0xe000 ? ((k) & 0x0fff) : \
			    (((k) & 0xf800) == 0xf000 ? ((k) & 0x00ff) : (k)))

/*
 * Keyboard types: 8bit encoding, 24bit variant
 */

#define KB_ENCODING(e)		((e) & 0x0000ff00)
#define KB_VARIANT(e)		((e) & 0xffff00ff)

#define	KB_NONE			0x0000
#define KB_USER			0x0100
#define KB_US			0x0200
#define KB_DE			0x0300
#define KB_DK			0x0400
#define KB_IT			0x0500
#define KB_FR			0x0600
#define KB_UK			0x0700
#define KB_JP			0x0800
#define KB_SV			0x0900
#define KB_NO			0x0a00
#define KB_ES			0x0b00
#define KB_HU			0x0c00
#define KB_BE			0x0d00
#define KB_RU			0x0e00
#define KB_SG			0x0f00
#define KB_SF			0x1000
#define KB_PT			0x1100
#define KB_UA			0x1200
#define KB_LT			0x1300
#define KB_LA			0x1400
#define KB_BR			0x1500
#define KB_NL			0x1600
#define KB_TR			0x1700
#define KB_PL			0x1800
#define KB_SI			0x1900
#define KB_CF			0x1a00
#define KB_LV			0x1b00

#define KB_NODEAD		0x000001 /* disable dead accents */
#define KB_DECLK		0x000002 /* DEC LKnnn layout */
#define KB_LK401		0x000004 /* DEC LK401 instead LK201 */
#define KB_SWAPCTRLCAPS		0x000008 /* swap Left-Control and Caps-Lock */
#define KB_DVORAK		0x000010 /* Dvorak layout */
#define KB_METAESC		0x000020 /* generate ESC prefix on ALT-key */
#define KB_IOPENER		0x000040 /* f1-f12 -> ESC,f1-f11 */
#define KB_MACHDEP		0x000080 /* machine dependent */
#define	KB_APPLE		0x010000 /* Apple specific layout */

#define KB_ENCTAB \
	{ KB_USER,	"user" }, \
	{ KB_US,	"us" }, \
	{ KB_DE,	"de" }, \
	{ KB_DK,	"dk" }, \
	{ KB_IT,	"it" }, \
	{ KB_FR,	"fr" }, \
	{ KB_UK,	"uk" }, \
	{ KB_JP,	"jp" }, \
	{ KB_SV,	"sv" }, \
	{ KB_NO,	"no" }, \
	{ KB_ES,	"es" }, \
	{ KB_HU,	"hu" }, \
	{ KB_BE,	"be" }, \
	{ KB_RU,	"ru" }, \
	{ KB_UA,	"ua" }, \
	{ KB_SG,	"sg" }, \
	{ KB_SF,	"sf" }, \
	{ KB_PT,	"pt" }, \
	{ KB_LT,	"lt" }, \
	{ KB_LA,	"la" }, \
	{ KB_BR,	"br" },	\
	{ KB_NL,	"nl" }, \
	{ KB_TR,	"tr" }, \
	{ KB_PL,	"pl" }, \
	{ KB_SI,	"si" }, \
	{ KB_CF,	"cf" }, \
	{ KB_LV,	"lv" }

#define KB_VARTAB \
	{ KB_NODEAD,	"nodead" }, \
	{ KB_DECLK,	"declk" }, \
	{ KB_LK401,	"lk401" }, \
	{ KB_SWAPCTRLCAPS, "swapctrlcaps" }, \
	{ KB_DVORAK,	"dvorak" }, \
	{ KB_METAESC,	"metaesc" }, \
	{ KB_IOPENER,	"iopener" }, \
	{ KB_MACHDEP,	"machdep" }, \
	{ KB_APPLE,	"apple" }

#endif /* !_DEV_WSCONS_WSKSYMDEF_H_ */
