/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * main.c - p-sprint
 *
 * Copyright (c) 2005 Arwin van Arum
 * special thanks to Shine for leading the way
 *
 * $Id$
 */
#include <pspctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pspirkeyb.h>
#include <pspirkeyb_rawkeys.h>
#include <pspgu.h>

#define LEFT	 1
#define UP	 2
#define RIGHT	 3
#define SQUARE	 4
#define TRIANGLE 5
#define CIRCLE	 6
#define OTHER	 7
#define UNSET	 0

#define CONTROL 1
#define SHIFT 2
#define ALT 4
#define MODE 8

typedef struct {
	unsigned char keycode;
	unsigned char label[5];
} keycodes;

static const keycodes p_sprint_keys[3][7][6] = 
// group 0
// left + *
{ { {	{KEY_B, "b"},
	{KEY_Y, "y"},
	{KEY_G, "g"},
	{KEY_LEFTBRACE, "["},
	{KEY_APOSTROPHE, "\'"},
	{KEY_COMMA, ","} },
// up + *
{	{KEY_F, "f"},
	{KEY_O, "o"},
	{KEY_U, "u"},
	{KEY_X, "x"},
	{KEY_V, "v"},
	{KEY_GRAVE, "`"} },
// right + *
{	{KEY_J, "j"},
	{KEY_L, "l"},
	{KEY_D, "d"},
	{KEY_M, "m"},
	{KEY_Z, "z"},
	{KEY_RIGHTBRACE, "]"} },
// square + *
{	{KEY_BACKSLASH, "\\"},
	{KEY_Q, "q"},
	{KEY_P, "p"},
	{KEY_S, "s"},
	{KEY_T, "t"},
	{KEY_C, "c"} },
// triangle + *
{	{KEY_SEMICOLON, ";"},
	{KEY_W, "w"},
	{KEY_K, "k"},
	{KEY_R, "r"},
	{KEY_E, "e"},
	{KEY_A, "a"} },
// circle + *
{	{KEY_DOT, "."},
	{KEY_MINUS, "-"},
	{KEY_SLASH, "/"},
	{KEY_H, "h"},
	{KEY_N, "n"},
	{KEY_I, "i"} },
// other
{	{KEY_BACKSPACE, "bs"},
	{KEY_SPACE, "sp"},
	{KEY_ENTER, "ent"},
	{KEY_ESC, "esc"},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""} } },
// group 1
// left + *
{ {	{KEY_F1, "f1"},
	{KEY_F2, "f2"},
	{KEY_F8, "f8"},
	{KEY_LEFTBRACE, "["},
	{KEY_SEMICOLON, ";"},
	{KEY_COMMA, ","} },
// up + *
{	{KEY_F3, "f3"},
	{KEY_F4, "f4"},
	{KEY_F5, "f5"},
	{KEY_KPSLASH, "n/"},
	{KEY_F12, "f12"},
	{KEY_GRAVE, "`"} },
// right + *
{	{KEY_F9, "f9"},
	{KEY_F6, "f6"},
	{KEY_F7, "f7"},
	{KEY_F10, "f10"},
	{KEY_F11, "f11"},
	{KEY_RIGHTBRACE, "]"} },
// square + *
{	{KEY_KPENTER, "nent"},
	{KEY_KPASTERISK, "n*"},
	{KEY_0, "0"},
	{KEY_1, "1"},
	{KEY_2, "2"},
	{KEY_9, "9"} },
// triangle + *
{	{KEY_SEMICOLON, ";"},
	{KEY_KPPLUS, "n+"},
	{KEY_KPMINUS, "n-"},
	{KEY_3, "3"},
	{KEY_4, "4"},
	{KEY_5, "5"} },
// circle + *
{	{KEY_DOT, "."},
	{KEY_EQUAL, "="},
	{KEY_RESERVED, ""},
	{KEY_8, "8"},
	{KEY_6, "6"},
	{KEY_7, "7"} },
// other
{	{KEY_BACKSPACE, "bs"},
	{KEY_SPACE, "sp"},
	{KEY_ENTER, "ent"},
	{KEY_ESC, "esc"},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""} } },
// group 2
// left + *
{ {	{KEY_LEFT, "left"},
	{KEY_HOME, "home"},
	{KEY_INSERT, "ins"},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""} },
// up + *
{	{KEY_END, "end"},
	{KEY_UP, "up"},
	{KEY_PAGEUP, "pgup"},
	{KEY_PAUSE, "paus"},
	{KEY_SYSRQ, "syrq"},
	{KEY_RESERVED, ""} },
// right + *
{	{KEY_DELETE, "del"},
	{KEY_PAGEDOWN, "pgdn"},
	{KEY_RIGHT, "righ"},
	{KEY_KPPLUS, "n+"},
	{KEY_KPASTERISK, "n*"},
	{KEY_RESERVED, ""} },
// square + *
{	{KEY_RESERVED, ""},
	{KEY_SCROLLLOCK, "sclk"},
	{KEY_KPMINUS, "n-"},
	{KEY_ESC, "esc"},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""} },
// triangle + *
{	{KEY_RESERVED, ""},
	{KEY_NUMLOCK, "nmlk"},
	{KEY_RESERVED, ""},
	{KEY_LEFTCTRL, "ltct"},
	{KEY_LEFTSHIFT, "ltsh"},
	{KEY_LEFTALT, "ltal"} },
// circle + *
{	{KEY_RIGHTCTRL, "rtct"},
	{KEY_RIGHTSHIFT, "rtsh"},
	{KEY_RIGHTALT, "rtal"},
	{KEY_CAPSLOCK, "cplk"},
	{KEY_RESERVED, ""},
	{KEY_TAB, "tab"} },
// other
{	{KEY_DOWN, "down"},
	{KEY_SPACE, "sp"},
	{KEY_ENTER, "ent"},
	{KEY_ESC, "esc"},
	{KEY_RESERVED, ""},
	{KEY_RESERVED, ""} } } };

static unsigned char hint_btn = UNSET;
static unsigned char active_group = 0;

#define HINT_WIDTH 72
#define HINT_HEIGHT 16
#define HINT_SIZE (HINT_WIDTH*HINT_HEIGHT)

/* borrowed from pspsdk */
extern unsigned char msx[];

static void debug_put_char_32(int x, int y, unsigned char *buffer, unsigned char ch) {
   int i,j;
   unsigned char *font;
   unsigned int *buf_ptr = (unsigned int *)buffer;
 
   buf_ptr += (y*HINT_WIDTH)+x;
   font = &msx[(int)ch * 8];
   
   for (i=0; i < 8; i++, font++) {
      for (j=0; j < 8; j++) {
          if ((*font & (128 >> j)))
          	buf_ptr[j] = -1;
      }
      buf_ptr += HINT_WIDTH;
   }
}

static void debug_put_char_16(int x, int y, unsigned char *buffer, unsigned char ch) {
   int i,j;
   unsigned char *font;
   unsigned short *buf_ptr = (unsigned short *)buffer;

   buf_ptr += (y*HINT_WIDTH)+x;
   font = &msx[(int)ch * 8];

   for (i=0; i < 8; i++, font++) {
      for (j=0; j < 8; j++) {
          if ((*font & (128 >> j)))
          	buf_ptr[j] = -1; 
      }
      buf_ptr += HINT_WIDTH;
   }
}

static void DrawHint(const keycodes *buttons, unsigned char *buffer, int bpp) {
	int i, j, x, y;
	char *string;
	for(j = 0; j < 3; j++) {
		switch(j) {
		case 0: x = 0; y = 8; break;
		case 1: x = 20; y = 0; break;
		case 2: x = 40; y = 8; break;
		}
		string = buttons[j].label;
		i = strnlen(string, 4);
		x += (32-(i*8))/2;
		for(i -= 1; i >= 0; i--) {
			if(bpp == 16)
				debug_put_char_16(x+(i*8), y, buffer, string[i]);
			else
				debug_put_char_32(x+(i*8), y, buffer, string[i]);
		}
	}
}

unsigned int *p_spGetHintList(int bpp) {
	static unsigned char last_hint = UNSET, last_bpp = 0, last_group;
	static int hints[2][HINT_SIZE] __attribute__((aligned (16)));
	static unsigned int list[38] __attribute__((aligned (16)));
	if(hint_btn == UNSET) return NULL;
	if((hint_btn != last_hint) || (last_group != active_group) || (bpp != last_bpp)) {
		int psm = (bpp == 16 ? GU_PSM_5650 : GU_PSM_8888);
		last_bpp = bpp;
		last_hint = hint_btn;
		last_group = active_group;
		//memset(hints, '\0', HINT_SIZE*2*4);
		DrawHint(&p_sprint_keys[active_group][hint_btn-1][0], (unsigned char *)hints[0], bpp);
		DrawHint(&p_sprint_keys[active_group][hint_btn-1][3], (unsigned char *)hints[1], bpp);
		sceGuStart(GU_SEND, list);
		sceGuCopyImage(psm, 0, 0, HINT_WIDTH, HINT_HEIGHT, HINT_WIDTH, hints[0], 
				0, 272-HINT_HEIGHT, 512, (void *)0x04000000);
		sceGuCopyImage(psm, 0, 0, HINT_WIDTH, HINT_HEIGHT, HINT_WIDTH, hints[1], 
				480-HINT_WIDTH, 272-HINT_HEIGHT, 512, (void *)0x04000000);
		sceGuFinish();
	}
	return list;
}

static int p_spGetControlKeys(unsigned int butpress1, unsigned int butpress2, unsigned char old_state) {
	/* determines whether or not a two-button combination
	contains a shift option and returns the shift option
	a.k.a. modifiers if true */

	/* TODO: allow for combinations of control keys */

	unsigned int mainbut;
	unsigned int shiftbut;
	
	mainbut = butpress1 & butpress2;
	
	if ((!(mainbut==butpress2))&(!(butpress2==0))) {
		shiftbut = butpress2 ^ butpress1;
		switch (shiftbut)
		{
		case PSP_CTRL_DOWN:
		case PSP_CTRL_CROSS:
			old_state|=MODE;
			break;
		case PSP_CTRL_RIGHT:
		case PSP_CTRL_CIRCLE:
			old_state|=ALT;
			break;
		case PSP_CTRL_SQUARE:
		case PSP_CTRL_LEFT:
			old_state|=CONTROL;
			break;
		case PSP_CTRL_TRIANGLE:
		case PSP_CTRL_UP:
			old_state|=SHIFT;
			break;
		}
	}
	return old_state;
}

int p_spReadKey(SIrKeybScanCodeData * newKey, unsigned int cur_btnstate) {
	/* main p_sprint function: 
	non-blocking routine that fills a pspirkeyb scancode structure
	Function returns true when a key is found, else false. */

	unsigned char new_btn;
	static unsigned int prev_btnstate = 0;
	static unsigned char shift_state = 0;
	static unsigned char prev_btn = UNSET;
	static SIrKeybScanCodeData prev_Key;
	
	if(!cur_btnstate) {
		/* release key */
		if(prev_Key.pressed && prev_btn != UNSET) {
			prev_Key.pressed = 0;
			shift_state = 0;
			prev_btn = UNSET;
			goto release;
		}
		goto not_found;
	} else if(cur_btnstate==prev_btnstate) goto not_found; 
	else if(prev_btnstate) { /* handle button state-change */
		shift_state = p_spGetControlKeys(prev_btnstate, cur_btnstate, shift_state);
		if(shift_state & 8) {
			/* a group select special, so no regular shift value */
			switch(prev_btnstate) { 
			case PSP_CTRL_LEFT:
				if(active_group == 1) active_group = 0;
				else active_group = 1;
				break;
			case PSP_CTRL_UP:
				if(active_group == 2) active_group = 0;
				else active_group = 2;
				break;
			case PSP_CTRL_RIGHT:
				/* don't do anything for now */
				break;
			}
			shift_state = 0;
			prev_btn = UNSET;
			hint_btn = UNSET;
		}
		goto not_found;
	} else if(prev_btn != UNSET) {
		/* found 2nd button press, get keyCode */
		hint_btn = UNSET;
		switch(cur_btnstate) {
		case PSP_CTRL_START:
		case PSP_CTRL_SELECT:
		case PSP_CTRL_DOWN:
		case PSP_CTRL_CROSS:
		default:
			/* clear set keys */
			shift_state = 0;
			prev_btn = UNSET;
			goto not_found;
		case PSP_CTRL_LEFT:
			new_btn = LEFT;
			break;
		case PSP_CTRL_UP:
			new_btn = UP;
			break;
		case PSP_CTRL_RIGHT:
			new_btn = RIGHT;
			break;
		case PSP_CTRL_SQUARE:
			new_btn = SQUARE;
			break;
		case PSP_CTRL_TRIANGLE:
			new_btn = TRIANGLE;
			break;
		case PSP_CTRL_CIRCLE:
			new_btn = CIRCLE;
			break;
		}
	} else {
		/* found first button press */
		/* check if buttonstate is a single-key that returns a value
		(e.g. 'x' for space or 'down' for backspace) */
		switch(cur_btnstate) {
		case PSP_CTRL_START:
			prev_btn = OTHER;
			new_btn = 3;
			break;
		case PSP_CTRL_SELECT:
			prev_btn = OTHER;
			new_btn = 4;
			break;
		case PSP_CTRL_DOWN:
			prev_btn = OTHER;
			new_btn = 1;
			break;
		case PSP_CTRL_CROSS:
			prev_btn = OTHER;
			new_btn = 2;
			break;
		case PSP_CTRL_LEFT:
			prev_btn = LEFT;
			break;
		case PSP_CTRL_UP:
			prev_btn = UP;
			break;
		case PSP_CTRL_RIGHT:
			prev_btn = RIGHT;
			break;
		case PSP_CTRL_SQUARE:
			prev_btn = SQUARE;
			break;
		case PSP_CTRL_TRIANGLE:
			prev_btn = TRIANGLE;
			break;
		case PSP_CTRL_CIRCLE:
			prev_btn = CIRCLE;
			break;
		}
		if(prev_btn != OTHER) {
			hint_btn = prev_btn;
			goto not_found;
		}
	}

	prev_Key.pressed = 1;
	prev_Key.raw = p_sprint_keys[active_group][prev_btn-1][new_btn-1].keycode;
	prev_Key.ctrl = (shift_state & CONTROL) && 1;
	prev_Key.shift = (shift_state & SHIFT) && 1;
	prev_Key.alt = (shift_state & ALT) && 1;
release:
	prev_btnstate = cur_btnstate;
	memcpy(newKey, &prev_Key, sizeof(SIrKeybScanCodeData));
	return 1;
not_found:
	prev_btnstate = cur_btnstate;
	return 0;
}

