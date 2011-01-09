//=====================================================================
//
// ikitwin.c - windib & x11 basic drawing support
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __IKITWIN_H__
#define __IKITWIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(__APPLE__) && (!defined(__unix))
#define __unix
#endif

#if defined(__unix)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlibint.h>
#include <sys/time.h>
#include <unistd.h>

#elif defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include <mmsystem.h>
#include <time.h>
#else
#error This file can only be compiled under windows or unix
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


//=====================================================================
// 跨平台窗口管理接口
//=====================================================================
int ikitwin_init(void);

void ikitwin_quit(void);

int ikitwin_set_mode(int w, int h, int bpp, int flag);

void ikitwin_release_mode(void);

int ikitwin_update(const int *rect, int n);

int ikitwin_lock(void **pixel, long *pitch);

void ikitwin_unlock(void);

int ikitwin_depth(int *rmask, int *gmask, int *bmask);

void ikitwin_info(int *screenw, int *screenh, int *screenbpp);

int ikitwin_dispatch(void);

int ikitwin_set_caption(const char *title);


//---------------------------------------------------------------------
// 工具接口
//---------------------------------------------------------------------
void ikitwin_sleep(long millisecond);

void ikitwin_timeofday(long *sec, long *usec);

IINT64 ikitwin_clock(void);

float ikitwin_fps(void);

int ikitwin_tick(int fps);


//=====================================================================
// 事件接口和全局控制变量
//=====================================================================
extern int (*ikitwin_private_active)(int mode, int type);
extern int (*ikitwin_private_mouse_motion)(int bs, int rl, int x, int y);
extern int (*ikitwin_private_mouse_button)(int st, int bt, int x, int y);
extern int (*ikitwin_private_keyboard)(int st, int keycode);
extern int (*ikitwin_private_quit)(void);

#define IKIT_ACTIVE_APP			1
#define IKIT_ACTIVE_FOCUSINPUT	2
#define IKIT_ACTIVE_FOCUSMOUSE	4
#define IKIT_KEYDOWN	1
#define IKIT_KEYUP		0

#define IMOUSE_LEFT		1
#define IMOUSE_RIGHT	2
#define IMOUSE_MIDDLE	3
#define IMOUSE_WHEELUP	4
#define IMOUSE_WHEELDW	5

extern char ikitwin_windowid[];

extern int ikitwin_screen_w;
extern int ikitwin_screen_h;
extern int ikitwin_screen_bpp;

extern int ikitwin_mouse_x;
extern int ikitwin_mouse_y;
extern int ikitwin_mouse_btn;
extern int ikitwin_mouse_in;

extern int ikitwin_keymap[];
extern int ikitwin_closing;

#ifndef __unix
extern HWND mswin_hwnd;
#endif

//---------------------------------------------------------------------
// Keyboard Definition
//---------------------------------------------------------------------
#ifndef __IKEY_DEFINED__
#define __IKEY_DEFINED__

typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	IKEY_UNKNOWN		= 0,
	IKEY_FIRST		= 0,
	IKEY_BACKSPACE		= 8,
	IKEY_TAB		= 9,
	IKEY_CLEAR		= 12,
	IKEY_RETURN		= 13,
	IKEY_PAUSE		= 19,
	IKEY_ESCAPE		= 27,
	IKEY_SPACE		= 32,
	IKEY_EXCLAIM		= 33,
	IKEY_QUOTEDBL		= 34,
	IKEY_HASH		= 35,
	IKEY_DOLLAR		= 36,
	IKEY_AMPERSAND		= 38,
	IKEY_QUOTE		= 39,
	IKEY_LEFTPAREN		= 40,
	IKEY_RIGHTPAREN		= 41,
	IKEY_ASTERISK		= 42,
	IKEY_PLUS		= 43,
	IKEY_COMMA		= 44,
	IKEY_MINUS		= 45,
	IKEY_PERIOD		= 46,
	IKEY_SLASH		= 47,
	IKEY_0			= 48,
	IKEY_1			= 49,
	IKEY_2			= 50,
	IKEY_3			= 51,
	IKEY_4			= 52,
	IKEY_5			= 53,
	IKEY_6			= 54,
	IKEY_7			= 55,
	IKEY_8			= 56,
	IKEY_9			= 57,
	IKEY_COLON		= 58,
	IKEY_SEMICOLON		= 59,
	IKEY_LESS		= 60,
	IKEY_EQUALS		= 61,
	IKEY_GREATER		= 62,
	IKEY_QUESTION		= 63,
	IKEY_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	IKEY_LEFTBRACKET	= 91,
	IKEY_BACKSLASH		= 92,
	IKEY_RIGHTBRACKET	= 93,
	IKEY_CARET		= 94,
	IKEY_UNDERSCORE		= 95,
	IKEY_BACKQUOTE		= 96,
	IKEY_A			= 97,
	IKEY_B			= 98,
	IKEY_C			= 99,
	IKEY_D			= 100,
	IKEY_E			= 101,
	IKEY_F			= 102,
	IKEY_G			= 103,
	IKEY_H			= 104,
	IKEY_I			= 105,
	IKEY_J			= 106,
	IKEY_K			= 107,
	IKEY_L			= 108,
	IKEY_M			= 109,
	IKEY_N			= 110,
	IKEY_O			= 111,
	IKEY_P			= 112,
	IKEY_Q			= 113,
	IKEY_R			= 114,
	IKEY_S			= 115,
	IKEY_T			= 116,
	IKEY_U			= 117,
	IKEY_V			= 118,
	IKEY_W			= 119,
	IKEY_X			= 120,
	IKEY_Y			= 121,
	IKEY_Z			= 122,
	IKEY_DELETE		= 127,
	/* End of ASCII mapped keysyms */

	/* International keyboard syms */
	IKEY_WORLD_0		= 160,		/* 0xA0 */
	IKEY_WORLD_1		= 161,
	IKEY_WORLD_2		= 162,
	IKEY_WORLD_3		= 163,
	IKEY_WORLD_4		= 164,
	IKEY_WORLD_5		= 165,
	IKEY_WORLD_6		= 166,
	IKEY_WORLD_7		= 167,
	IKEY_WORLD_8		= 168,
	IKEY_WORLD_9		= 169,
	IKEY_WORLD_10		= 170,
	IKEY_WORLD_11		= 171,
	IKEY_WORLD_12		= 172,
	IKEY_WORLD_13		= 173,
	IKEY_WORLD_14		= 174,
	IKEY_WORLD_15		= 175,
	IKEY_WORLD_16		= 176,
	IKEY_WORLD_17		= 177,
	IKEY_WORLD_18		= 178,
	IKEY_WORLD_19		= 179,
	IKEY_WORLD_20		= 180,
	IKEY_WORLD_21		= 181,
	IKEY_WORLD_22		= 182,
	IKEY_WORLD_23		= 183,
	IKEY_WORLD_24		= 184,
	IKEY_WORLD_25		= 185,
	IKEY_WORLD_26		= 186,
	IKEY_WORLD_27		= 187,
	IKEY_WORLD_28		= 188,
	IKEY_WORLD_29		= 189,
	IKEY_WORLD_30		= 190,
	IKEY_WORLD_31		= 191,
	IKEY_WORLD_32		= 192,
	IKEY_WORLD_33		= 193,
	IKEY_WORLD_34		= 194,
	IKEY_WORLD_35		= 195,
	IKEY_WORLD_36		= 196,
	IKEY_WORLD_37		= 197,
	IKEY_WORLD_38		= 198,
	IKEY_WORLD_39		= 199,
	IKEY_WORLD_40		= 200,
	IKEY_WORLD_41		= 201,
	IKEY_WORLD_42		= 202,
	IKEY_WORLD_43		= 203,
	IKEY_WORLD_44		= 204,
	IKEY_WORLD_45		= 205,
	IKEY_WORLD_46		= 206,
	IKEY_WORLD_47		= 207,
	IKEY_WORLD_48		= 208,
	IKEY_WORLD_49		= 209,
	IKEY_WORLD_50		= 210,
	IKEY_WORLD_51		= 211,
	IKEY_WORLD_52		= 212,
	IKEY_WORLD_53		= 213,
	IKEY_WORLD_54		= 214,
	IKEY_WORLD_55		= 215,
	IKEY_WORLD_56		= 216,
	IKEY_WORLD_57		= 217,
	IKEY_WORLD_58		= 218,
	IKEY_WORLD_59		= 219,
	IKEY_WORLD_60		= 220,
	IKEY_WORLD_61		= 221,
	IKEY_WORLD_62		= 222,
	IKEY_WORLD_63		= 223,
	IKEY_WORLD_64		= 224,
	IKEY_WORLD_65		= 225,
	IKEY_WORLD_66		= 226,
	IKEY_WORLD_67		= 227,
	IKEY_WORLD_68		= 228,
	IKEY_WORLD_69		= 229,
	IKEY_WORLD_70		= 230,
	IKEY_WORLD_71		= 231,
	IKEY_WORLD_72		= 232,
	IKEY_WORLD_73		= 233,
	IKEY_WORLD_74		= 234,
	IKEY_WORLD_75		= 235,
	IKEY_WORLD_76		= 236,
	IKEY_WORLD_77		= 237,
	IKEY_WORLD_78		= 238,
	IKEY_WORLD_79		= 239,
	IKEY_WORLD_80		= 240,
	IKEY_WORLD_81		= 241,
	IKEY_WORLD_82		= 242,
	IKEY_WORLD_83		= 243,
	IKEY_WORLD_84		= 244,
	IKEY_WORLD_85		= 245,
	IKEY_WORLD_86		= 246,
	IKEY_WORLD_87		= 247,
	IKEY_WORLD_88		= 248,
	IKEY_WORLD_89		= 249,
	IKEY_WORLD_90		= 250,
	IKEY_WORLD_91		= 251,
	IKEY_WORLD_92		= 252,
	IKEY_WORLD_93		= 253,
	IKEY_WORLD_94		= 254,
	IKEY_WORLD_95		= 255,		/* 0xFF */

	/* Numeric keypad */
	IKEY_KP0		= 256,
	IKEY_KP1		= 257,
	IKEY_KP2		= 258,
	IKEY_KP3		= 259,
	IKEY_KP4		= 260,
	IKEY_KP5		= 261,
	IKEY_KP6		= 262,
	IKEY_KP7		= 263,
	IKEY_KP8		= 264,
	IKEY_KP9		= 265,
	IKEY_KP_PERIOD		= 266,
	IKEY_KP_DIVIDE		= 267,
	IKEY_KP_MULTIPLY	= 268,
	IKEY_KP_MINUS		= 269,
	IKEY_KP_PLUS		= 270,
	IKEY_KP_ENTER		= 271,
	IKEY_KP_EQUALS		= 272,

	/* Arrows + Home/End pad */
	IKEY_UP			= 273,
	IKEY_DOWN		= 274,
	IKEY_RIGHT		= 275,
	IKEY_LEFT		= 276,
	IKEY_INSERT		= 277,
	IKEY_HOME		= 278,
	IKEY_END		= 279,
	IKEY_PAGEUP		= 280,
	IKEY_PAGEDOWN		= 281,

	/* Function keys */
	IKEY_F1			= 282,
	IKEY_F2			= 283,
	IKEY_F3			= 284,
	IKEY_F4			= 285,
	IKEY_F5			= 286,
	IKEY_F6			= 287,
	IKEY_F7			= 288,
	IKEY_F8			= 289,
	IKEY_F9			= 290,
	IKEY_F10		= 291,
	IKEY_F11		= 292,
	IKEY_F12		= 293,
	IKEY_F13		= 294,
	IKEY_F14		= 295,
	IKEY_F15		= 296,

	/* Key state modifier keys */
	IKEY_NUMLOCK		= 300,
	IKEY_CAPSLOCK		= 301,
	IKEY_SCROLLOCK		= 302,
	IKEY_RSHIFT		= 303,
	IKEY_LSHIFT		= 304,
	IKEY_RCTRL		= 305,
	IKEY_LCTRL		= 306,
	IKEY_RALT		= 307,
	IKEY_LALT		= 308,
	IKEY_RMETA		= 309,
	IKEY_LMETA		= 310,
	IKEY_LSUPER		= 311,		/* Left "Windows" key */
	IKEY_RSUPER		= 312,		/* Right "Windows" key */
	IKEY_MODE		= 313,		/* "Alt Gr" key */
	IKEY_COMPOSE		= 314,		/* Multi-key compose key */

	/* Miscellaneous function keys */
	IKEY_HELP		= 315,
	IKEY_PRINT		= 316,
	IKEY_SYSREQ		= 317,
	IKEY_BREAK		= 318,
	IKEY_MENU		= 319,
	IKEY_POWER		= 320,		/* Power Macintosh power key */
	IKEY_EURO		= 321,		/* Some european keyboards */
	IKEY_UNDO		= 322,		/* Atari keyboard has Undo */

	/* Add any other keys here */

	IKEY_LAST
}	IKEYDESC;

#endif


#ifdef __cplusplus
}
#endif

#endif

