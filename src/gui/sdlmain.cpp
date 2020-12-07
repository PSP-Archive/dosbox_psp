/*
 *  Copyright (C) 2002-2007  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: sdlmain.cpp,v 1.133 2007-08-18 16:32:04 qbix79 Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef WIN32
#include <signal.h>
#endif

#ifdef USE_SDL
#include "SDL.h"
#endif

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "joystick.h"

//#define DISABLE_JOYSTICK
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if (HAVE_DDRAW_H)
#include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "/.dosboxrc"
#endif

#ifdef PSP
#include <pspsdk.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspirkeyb.h>
#include "p_sprint.h"
#ifdef PSPME
#include "me_rpc.h"
#endif

static int sensitivity;
bool control_map = false, joy_state = false, irkeyb = false, show_key_hint = false;
bool autofire = false, startup_state_capslock = false, startup_state_numlock = false;
static SceUID main_thsema;
static bool suspended = false, reinit = false, key_hint = false;
char build_version[] = "DOSBox " VERSION "-psp build " __DATE__ " " __TIME__;
profile_regs_ll regs;
bool profile = false, fixup = false;
#include <pspdebug.h>

void cache_init(bool);
void cache_free_memory(void);

#define UNK KBD_NONE
keymap inputmap[] =
{{ KBD_esc, PSP_CTRL_SELECT },
{ KBD_enter, PSP_CTRL_START },
{ KBD_up, PSP_CTRL_UP },
{ KBD_right, PSP_CTRL_RIGHT },
{ KBD_down, PSP_CTRL_DOWN },
{ KBD_left, PSP_CTRL_LEFT },
{ KBD_w, PSP_CTRL_TRIANGLE },
{ KBD_d, PSP_CTRL_CIRCLE },
{ KBD_s, PSP_CTRL_CROSS },
{ KBD_a, PSP_CTRL_SQUARE },
{ KBD_button1, PSP_CTRL_LTRIGGER },
{ KBD_button2, PSP_CTRL_RTRIGGER },
{ UNK, 0 }};

static const KBD_KEYS ir_xlate_table[] = { UNK, KBD_esc, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7,
	KBD_8, KBD_9, KBD_0, KBD_minus, KBD_equals, KBD_backspace, KBD_tab, KBD_q, KBD_w, KBD_e, KBD_r, KBD_t,
	KBD_y, KBD_u, KBD_i, KBD_o, KBD_p, KBD_leftbracket, KBD_rightbracket, KBD_enter, KBD_leftctrl, KBD_a,
	KBD_s, KBD_d, KBD_f, KBD_g, KBD_h, KBD_j, KBD_k, KBD_l, KBD_semicolon, KBD_quote, KBD_grave, KBD_leftshift,
	KBD_backslash, KBD_z, KBD_x, KBD_c, KBD_v, KBD_b, KBD_n, KBD_m, KBD_comma, KBD_period, KBD_slash, KBD_rightshift,
	KBD_kpmultiply, KBD_leftalt, KBD_space, KBD_capslock, KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7,
	KBD_f8, KBD_f9, KBD_f10, KBD_numlock, KBD_scrolllock, KBD_kp7, KBD_kp8, KBD_kp9, KBD_kpminus, KBD_kp4,
	KBD_kp5, KBD_kp6, KBD_kpplus, KBD_kp1, KBD_kp2, KBD_kp3, KBD_kp0, KBD_kpperiod, UNK, UNK, UNK, UNK, UNK, UNK,
	UNK, UNK, UNK, UNK, UNK, UNK, KBD_kpenter, KBD_rightctrl, KBD_kpdivide, KBD_printscreen, KBD_rightalt, UNK,
	KBD_home, KBD_up, KBD_pageup, KBD_left, KBD_right, KBD_end, KBD_down, KBD_pagedown, KBD_insert, KBD_delete, UNK,
	UNK, UNK, UNK, UNK, UNK, UNK, KBD_pause};

void Mouse_AutoLock(bool enable) {}
void GUI_StartUp(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	std::string inifile=section->Get_string("irkeybini");
	sensitivity=section->Get_int("sensitivity");
	key_hint=section->Get_bool("keyhint");
	if(pspIrKeybInit((inifile == "")?NULL:inifile.c_str(),0) < 0)
		LOG_MSG("GUI:IR keyboard not found");
	else {
		LOG_MSG("GUI:IR keyboard found");
		irkeyb = true;
		pspIrKeybOutputMode(PSP_IRKBD_OUTPUT_MODE_SCANCODE);
		atexit((void (*)())pspIrKeybFinish);
	}
}

void GFX_SetTitle(Bits cycles,Bits frameskip,bool paused){ }
void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname) {
	// TODO: implement
}

void GFX_Events() {
	SceCtrlData pad;
	int x, y;
	SIrKeybScanCodeData key_state;
	static Bitu prev_pad = 0, last_sample = 0;
	static bool state_change = false, init = false;
	bool do_joy = false, do_keyb = false;
	char buffer[255];
	int length;

	sceKernelSignalSema(main_thsema, 1);
	
	if (!init) {
		sceCtrlSetSamplingCycle(0);
		sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
		JOYSTICK_Enable(0, true);
		JOYSTICK_Button(0, 0, false);
		JOYSTICK_Button(0, 1, false);
		JOYSTICK_Move_X(0, 0);
		JOYSTICK_Move_Y(0, 0);
		init = true;
	}

#ifdef PSPME
	sc_read_queue();
#endif

	if (PIC_Ticks < (last_sample + 17)) goto exit;  // limit the input rate to 60Hz
	last_sample = PIC_Ticks;
	
	if(profile) {
		PspDebugProfilerRegs small_regs;
		pspDebugProfilerGetRegs(&small_regs);
		regs.systemck += small_regs.systemck;
		regs.cpuck += small_regs.cpuck;
		regs.internal += small_regs.internal;
		regs.memory += small_regs.memory;
		regs.copz += small_regs.copz;
		regs.vfpu += small_regs.vfpu;
		regs.sleep += small_regs.sleep;
		regs.bus_access += small_regs.bus_access;
		regs.uncached_load += small_regs.uncached_load;
		regs.uncached_store += small_regs.uncached_store;
		regs.cached_load += small_regs.cached_load;
		regs.cached_store += small_regs.cached_store;
		regs.i_miss += small_regs.i_miss;
		regs.d_miss += small_regs.d_miss;
		regs.d_writeback += small_regs.d_writeback;
		regs.cop0_inst += small_regs.cop0_inst;
		regs.fpu_inst += small_regs.fpu_inst;
		regs.vfpu_inst += small_regs.vfpu_inst;
		regs.local_bus += small_regs.local_bus;
		pspDebugProfilerClear();
	}

	if(sceCtrlPeekBufferPositive(&pad, 1) <= 0) goto exit; 
	pad.Buttons &= 0xFFFF;
	
	if(state_change && pad.Buttons)
		goto exit;
	if(pad.Buttons == (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START | PSP_CTRL_DOWN)) {
		state_change = true;
		do_joy = true;
		pad.Buttons = 0;
		pad.Lx = 127;
		pad.Ly = 127;
	}
	if(pad.Buttons == (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT | PSP_CTRL_DOWN)) {
		state_change = true;
		do_keyb = true;
		pad.Buttons = 0;
	}

	if(!do_joy && !do_keyb) state_change = false;

	x = (int)pad.Lx - 127;
	y = (int)pad.Ly - 127;
	
	/* 32x32 dead zone */
	if(!((x+16)>>5) && !((y+16)>>5)) {
		x = 0;
		y = 0;
	}

	if(joy_state) {
		JOYSTICK_Move_X(0, (float)x/127.0f);
		JOYSTICK_Move_Y(0, (float)y/127.0f);
	} else if(x || y) {
		Bits absx = (x>0)?(sensitivity):(-1*sensitivity);
		Bits absy = (y>0)?(sensitivity):(-1*sensitivity);
		Mouse_CursorMoved((float)(absx*x*x)/100000.0f, (float)(absy*y*y)/100000.0f, 1, 1, 1);
	}

	if(irkeyb && (pspIrKeybReadinput(buffer, &length) == PSP_IRKBD_RESULT_OK) && (length > 0)) {
		int count = length / sizeof(SIrKeybScanCodeData);
		SIrKeybScanCodeData *key;
		for(int i = 0; i < count; i++) {
			key = &((SIrKeybScanCodeData *)buffer)[i];
			if(key->raw > 119) continue;
			KEYBOARD_AddKey(ir_xlate_table[key->raw], key->pressed);
		}
	}
	if(control_map) {
		show_key_hint = false;
		for(Bitu i=0; inputmap[i].mask; i++) 
			if(pad.Buttons & inputmap[i].mask) {
				if(prev_pad & inputmap[i].mask) continue;
				switch(inputmap[i].key) {
				case KBD_button1:
					if(joy_state) JOYSTICK_Button(0, 0, true);
					else Mouse_ButtonPressed(0);
					break;
				case KBD_button2:
					if(joy_state) JOYSTICK_Button(0, 1, true);
					else Mouse_ButtonPressed(1);
					break;
				default:
					KEYBOARD_AddKey(inputmap[i].key, 1);
				}
			} else if(prev_pad & inputmap[i].mask) {
				switch(inputmap[i].key) {
				case KBD_button1:
					if(joy_state) JOYSTICK_Button(0, 0, false);
					else Mouse_ButtonReleased(0);
					break;
				case KBD_button2:
					if(joy_state) JOYSTICK_Button(0, 1, false);
					else Mouse_ButtonReleased(1);
					break;
				default: 
					KEYBOARD_AddKey(inputmap[i].key, 0);
				}
			}
	} else {
		if(key_hint) show_key_hint = true;
		if(p_spReadKey(&key_state, pad.Buttons)) {
			if(key_state.ctrl) KEYBOARD_AddKey(KBD_leftctrl, key_state.pressed);
			if(key_state.alt) KEYBOARD_AddKey(KBD_leftalt, key_state.pressed);
			if(key_state.shift) KEYBOARD_AddKey(KBD_leftshift, key_state.pressed);
			KEYBOARD_AddKey(ir_xlate_table[key_state.raw], key_state.pressed);
		}
		if(pad.Buttons & PSP_CTRL_LTRIGGER) {
			if(!(prev_pad & PSP_CTRL_LTRIGGER))
				if(joy_state) JOYSTICK_Button(0, 0, true);
				else Mouse_ButtonPressed(0);
		} else if(prev_pad & PSP_CTRL_LTRIGGER) {
				if(joy_state) JOYSTICK_Button(0, 0, false);
				else Mouse_ButtonReleased(0);
		}
		if(pad.Buttons & PSP_CTRL_RTRIGGER) {
			if(!(prev_pad & PSP_CTRL_RTRIGGER))
				if(joy_state) JOYSTICK_Button(0, 1, true);
				else Mouse_ButtonPressed(1);
		} else if(prev_pad & PSP_CTRL_RTRIGGER) {
				if(joy_state) JOYSTICK_Button(0, 1, false);
				else Mouse_ButtonReleased(1);
		}
	}
	prev_pad = pad.Buttons;
	if(state_change) { 
		if(do_joy) joy_state = !joy_state;
		if(do_keyb) control_map = !control_map;
	}
		
exit:
	sceKernelWaitSema(main_thsema, 1, NULL);
	if(suspended == true) {
		cache_init(reinit);
		suspended = false;
	}
}

#else

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif


#ifdef __WIN32__
#define NVIDIA_PixelDataRange 1

#ifndef WGL_NV_allocate_memory
#define WGL_NV_allocate_memory 1
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC) (int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);
#endif

PFNWGLALLOCATEMEMORYNVPROC db_glAllocateMemoryNV = NULL;
PFNWGLFREEMEMORYNVPROC db_glFreeMemoryNV = NULL;

#else 

#endif

#if defined(NVIDIA_PixelDataRange)

#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1
#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
typedef void (APIENTRYP PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (APIENTRYP PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
#endif

PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV = NULL;

#endif

#endif //C_OPENGL

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

enum SCREEN_TYPES	{ 
	SCREEN_SURFACE,
	SCREEN_SURFACE_DDRAW,
	SCREEN_OVERLAY,
	SCREEN_OPENGL
};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};


struct SDL_Block {
	bool active;							//If this isn't set don't draw
	bool updating;
	struct {
		Bit32u width;
		Bit32u height;
		Bit32u bpp;
		Bitu flags;
		double scalex,scaley;
		GFX_CallBack_t callback;
	} draw;
	bool wait_on_error;
	struct {
		struct {
			Bit16u width, height;
			bool fixed;
		} full;
		struct {
			Bit16u width, height;
		} window;
		Bit8u bpp;
		bool fullscreen;
		bool doublebuf;
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
	} desktop;
#if C_OPENGL
	struct {
		Bitu pitch;
		void * framebuf;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		bool bilinear;
		bool packed_pixel;
		bool paletted_texture;
#if defined(NVIDIA_PixelDataRange)
		bool pixel_data_range;
#endif
	} opengl;
#endif
	struct {
		SDL_Surface * surface;
#if (HAVE_DDRAW_H) && defined(WIN32)
		RECT rect;
#endif
	} blit;
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	SDL_cond *cond;
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		Bitu sensitivity;
	} mouse;
	SDL_Rect updateRects[1024];
	Bitu num_joysticks;
#if defined (WIN32)
	bool using_windib;
#endif
};

static SDL_Block sdl;

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;
void GFX_SetTitle(Bit32s cycles,Bits frameskip,bool paused){
	char title[200]={0};
	static Bit32s internal_cycles=0;
	static Bits internal_frameskip=0;
	if(cycles != -1) internal_cycles = cycles;
	if(frameskip != -1) internal_frameskip = frameskip;
	if(CPU_CycleAutoAdjust) {
		if (internal_cycles>=100)
			sprintf(title,"DOSBox %s, Cpu Cycles:      max, Frameskip %2d, Program: %8s",VERSION,internal_frameskip,RunningProgram);
		else
			sprintf(title,"DOSBox %s, Cpu Cycles:   [%3d%%], Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	} else {
		sprintf(title,"DOSBox %s, Cpu Cycles: %8d, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	}

	if(paused) strcat(title," PAUSED");
	SDL_WM_SetCaption(title,VERSION);
}

static void PauseDOSBox(bool pressed) {
	if (!pressed)
		return;
	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}
	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {
		case SDL_QUIT: throw(0); break;
		case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
		case SDL_KEYUP:
			if(event.key.keysym.sym==SDLK_PAUSE){
				paused=false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
		}
	}
}

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void) {
	return sdl.using_windib;
}
#endif

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) {
	Bitu testbpp,gotbpp;
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
check_surface:
		/* Check if we can satisfy the depth it loves */
		if (flags & GFX_LOVE_8) testbpp=8;
		else if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
check_gotbpp:
		if (sdl.desktop.fullscreen) gotbpp=SDL_VideoModeOK(640,480,testbpp,SDL_FULLSCREEN|SDL_HWSURFACE|SDL_HWPALETTE);
		else gotbpp=sdl.desktop.bpp;
		/* If we can't get our favorite mode check for another working one */
		switch (gotbpp) {
		case 8:
			if (flags & GFX_CAN_8) flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (flags & GFX_CAN_15) flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (flags & GFX_CAN_16) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (flags & GFX_CAN_32) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
		}
		flags |= GFX_CAN_RANDOM;
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (!(flags&(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32))) goto check_surface;
		if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
		flags|=GFX_SCALING;
		goto check_gotbpp;
#endif
	case SCREEN_OVERLAY:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#endif
	}
	return flags;
}


void GFX_ResetScreen(void) {
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
}

static int int_log2 (int val) {
    int log = 0;
    while ((val >>= 1) != 0)
	log++;
    return log;
}


static SDL_Surface * GFX_SetupSurfaceScaled(Bit32u sdl_flags, Bit32u bpp) {
	Bit16u fixedWidth;
	Bit16u fixedHeight;

	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
		sdl_flags |= SDL_FULLSCREEN|SDL_HWSURFACE;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
		sdl_flags |= SDL_HWSURFACE;
	}
	if (fixedWidth && fixedHeight) {
		double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
		double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
		if ( ratio_w < ratio_h) {
			sdl.clip.w=fixedWidth;
			sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*ratio_w);
		} else {
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h);
			sdl.clip.h=(Bit16u)fixedHeight;
		}
		if (sdl.desktop.fullscreen) 
			sdl.surface = SDL_SetVideoMode(fixedWidth,fixedHeight,bpp,sdl_flags);
		else
			sdl.surface = SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		if (sdl.surface && sdl.surface->flags & SDL_FULLSCREEN) {
			sdl.clip.x=(Sint16)((sdl.surface->w-sdl.clip.w)/2);
			sdl.clip.y=(Sint16)((sdl.surface->h-sdl.clip.h)/2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}
		return sdl.surface;
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
		sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		return sdl.surface;
	}
}

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback) {
	if (sdl.updating) 
		GFX_EndUpdate( 0 );

	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.callback=callback;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	Bitu bpp=0;
	Bitu retFlags = 0;
	
	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
dosurface:
		if (flags & GFX_CAN_8) bpp=8;
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.w=width;
		sdl.clip.h=height;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.full.fixed) {
				sdl.clip.x=(Sint16)((sdl.desktop.full.width-width)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.full.height-height)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.full.width,sdl.desktop.full.height,bpp,
					SDL_FULLSCREEN | ((flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) | 
					(sdl.desktop.doublebuf ? SDL_DOUBLEBUF|SDL_ASYNCBLIT : 0) | SDL_HWPALETTE);
				if (sdl.surface == NULL) E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",sdl.desktop.full.width,sdl.desktop.full.height,bpp,SDL_GetError());
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.surface=SDL_SetVideoMode(width,height,bpp,
					SDL_FULLSCREEN | ((flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) | 
					(sdl.desktop.doublebuf ? SDL_DOUBLEBUF|SDL_ASYNCBLIT  : 0)|SDL_HWPALETTE);
				if (sdl.surface == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",width,height,bpp,SDL_GetError());
			}
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.surface=SDL_SetVideoMode(width,height,bpp,(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE);
#ifdef WIN32
			if (sdl.surface == NULL) {
				SDL_QuitSubSystem(SDL_INIT_VIDEO);
				if (!sdl.using_windib) {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with windib enabled.");
					putenv("SDL_VIDEODRIVER=windib");
					sdl.using_windib=true;
				} else {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with directx enabled.");
					putenv("SDL_VIDEODRIVER=directx");
					sdl.using_windib=false;
				}
				SDL_InitSubSystem(SDL_INIT_VIDEO);
				sdl.surface = SDL_SetVideoMode(width,height,bpp,SDL_HWSURFACE);
			}
#endif
			if (sdl.surface == NULL) 
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",width,height,bpp,SDL_GetError());
		}
		if (sdl.surface) {
			switch (sdl.surface->format->BitsPerPixel) {
			case 8:
				retFlags = GFX_CAN_8;
                break;
			case 15:
				retFlags = GFX_CAN_15;
				break;
			case 16:
				retFlags = GFX_CAN_16;
                break;
			case 32:
				retFlags = GFX_CAN_32;
                break;
			}
			if (retFlags && (sdl.surface->flags & SDL_HWSURFACE))
				retFlags |= GFX_HARDWARE;
			if (retFlags && (sdl.surface->flags & SDL_DOUBLEBUF)) {
				sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,
					sdl.draw.width, sdl.draw.height,
					sdl.surface->format->BitsPerPixel,
					sdl.surface->format->Rmask,
					sdl.surface->format->Gmask,
					sdl.surface->format->Bmask,
				0);
				/* If this one fails be ready for some flickering... */
			}
		}
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		if (!GFX_SetupSurfaceScaled((sdl.desktop.doublebuf && sdl.desktop.fullscreen) ? SDL_DOUBLEBUF : 0,bpp)) goto dosurface;
		sdl.blit.rect.top=sdl.clip.y;
		sdl.blit.rect.left=sdl.clip.x;
		sdl.blit.rect.right=sdl.clip.x+sdl.clip.w;
		sdl.blit.rect.bottom=sdl.clip.y+sdl.clip.h;
		sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,sdl.draw.width,sdl.draw.height,
				sdl.surface->format->BitsPerPixel,
				sdl.surface->format->Rmask,
				sdl.surface->format->Gmask,
				sdl.surface->format->Bmask,
				0);
		if (!sdl.blit.surface || (!sdl.blit.surface->flags&SDL_HWSURFACE)) {
			if (sdl.blit.surface) {
				SDL_FreeSurface(sdl.blit.surface);
				sdl.blit.surface=0;
			}
			LOG_MSG("Failed to create ddraw surface, back to normal surface.");
			goto dosurface;
		}
		switch (sdl.surface->format->BitsPerPixel) {
		case 15:
			retFlags = GFX_CAN_15 | GFX_SCALING | GFX_HARDWARE;
			break;
		case 16:
			retFlags = GFX_CAN_16 | GFX_SCALING | GFX_HARDWARE;
               break;
		case 32:
			retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
               break;
		}
		sdl.desktop.type=SCREEN_SURFACE_DDRAW;
		break;
#endif
	case SCREEN_OVERLAY:
		if (sdl.overlay) {
			SDL_FreeYUVOverlay(sdl.overlay);
			sdl.overlay=0;
		}
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		if (!GFX_SetupSurfaceScaled(0,0)) goto dosurface;
		sdl.overlay=SDL_CreateYUVOverlay(width*2,height,SDL_UYVY_OVERLAY,sdl.surface);
		if (!sdl.overlay) {
			LOG_MSG("SDL:Failed to create overlay, switching back to surface");
			goto dosurface;
		}
		sdl.desktop.type=SCREEN_OVERLAY;
		retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
	{
		if (sdl.opengl.framebuf) {
#if defined(NVIDIA_PixelDataRange)
			if (sdl.opengl.pixel_data_range) db_glFreeMemoryNV(sdl.opengl.framebuf);
			else
#endif
			free(sdl.opengl.framebuf);
		}
		sdl.opengl.framebuf=0;
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		int texsize=2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
#if defined (WIN32) && SDL_VERSION_ATLEAST(1, 2, 11)
		SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
#endif
		GFX_SetupSurfaceScaled(SDL_OPENGL,0);
		if (!sdl.surface || sdl.surface->format->BitsPerPixel<15) {
			LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}
		/* Create the texture and display list */
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range) {
			sdl.opengl.framebuf=db_glAllocateMemoryNV(width*height*4,0.0,1.0,1.0);
			glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,width*height*4,sdl.opengl.framebuf);
			glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
		} else {
#else 
		{
#endif
			sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
		}
		sdl.opengl.pitch=width*4;
		glViewport(sdl.clip.x,sdl.clip.y,sdl.clip.w,sdl.clip.h);
		glMatrixMode (GL_PROJECTION);
		glDeleteTextures(1,&sdl.opengl.texture);
 		glGenTextures(1,&sdl.opengl.texture);
		glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);
		// No borders
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		if (sdl.opengl.bilinear) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

		glClearColor (0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapBuffers();
		glClear(GL_COLOR_BUFFER_BIT);
		glShadeModel (GL_FLAT); 
		glDisable (GL_DEPTH_TEST);
		glDisable (GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();

		GLfloat tex_width=((GLfloat)(width)/(GLfloat)texsize);
		GLfloat tex_height=((GLfloat)(height)/(GLfloat)texsize);

		if (glIsList(sdl.opengl.displaylist)) glDeleteLists(sdl.opengl.displaylist, 1);
		sdl.opengl.displaylist = glGenLists(1);
		glNewList(sdl.opengl.displaylist, GL_COMPILE);
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		glBegin(GL_QUADS);
		// lower left
		glTexCoord2f(0,tex_height); glVertex2f(-1.0f,-1.0f);
		// lower right
		glTexCoord2f(tex_width,tex_height); glVertex2f(1.0f, -1.0f);
		// upper right
		glTexCoord2f(tex_width,0); glVertex2f(1.0f, 1.0f);
		// upper left
		glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
		glEnd();
		glEndList();
		sdl.desktop.type=SCREEN_OPENGL;
		retFlags = GFX_CAN_32 | GFX_SCALING;
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range)
			retFlags |= GFX_HARDWARE;
#endif
	break;
		}//OPENGL
#endif	//C_OPENGL
	}//CASE
	if (retFlags) 
		GFX_Start();
	if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
	return retFlags;
}

void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

void GFX_SwitchFullScreen(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if (!sdl.mouse.locked) GFX_CaptureMouse();
	} else {
		if (sdl.mouse.locked) GFX_CaptureMouse();
	}
	GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed) {
	if (!pressed)
		return;
	GFX_SwitchFullScreen();
}


bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
	if (!sdl.active || sdl.updating) 
		return false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (sdl.blit.surface) {
			if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
				return false;
			pixels=(Bit8u *)sdl.blit.surface->pixels;
			pitch=sdl.blit.surface->pitch;
		} else {
			if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
				return false;
			pixels=(Bit8u *)sdl.surface->pixels;
			pixels+=sdl.clip.y*sdl.surface->pitch;
			pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
			pitch=sdl.surface->pitch;
		}
		sdl.updating=true;
		return true;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (SDL_LockSurface(sdl.blit.surface)) {
//			LOG_MSG("SDL Lock failed");
			return false;
		}
		pixels=(Bit8u *)sdl.blit.surface->pixels;
		pitch=sdl.blit.surface->pitch;
		sdl.updating=true;
		return true;
#endif
	case SCREEN_OVERLAY:
		SDL_LockYUVOverlay(sdl.overlay);
		pixels=(Bit8u *)*(sdl.overlay->pixels);
		pitch=*(sdl.overlay->pitches);
		sdl.updating=true;
		return true;
#if C_OPENGL
	case SCREEN_OPENGL:
		pixels=(Bit8u *)sdl.opengl.framebuf;
		pitch=sdl.opengl.pitch;
		sdl.updating=true;
		return true;
#endif
	}
	return false;
}


void GFX_EndUpdate( const Bit16u *changedLines ) {
	int ret;
	if (!sdl.updating) 
		return;
	sdl.updating=false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (SDL_MUSTLOCK(sdl.surface)) {
			if (sdl.blit.surface) {
				SDL_UnlockSurface(sdl.blit.surface);
				int Blit = SDL_BlitSurface( sdl.blit.surface, 0, sdl.surface, &sdl.clip );
				LOG(LOG_MISC,LOG_WARN)("BlitSurface returned %d",Blit);
			} else {
				SDL_UnlockSurface(sdl.surface);
			}
			SDL_Flip(sdl.surface);
		} else if (changedLines) {
			Bitu y = 0, index = 0, rectCount = 0;
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					SDL_Rect *rect = &sdl.updateRects[rectCount++];
					rect->x = sdl.clip.x;
					rect->y = sdl.clip.y + y;
					rect->w = (Bit16u)sdl.draw.width;
					rect->h = changedLines[index];
#if 0
					if (rect->h + rect->y > sdl.surface->h) {
						LOG_MSG("WTF");
					}
#endif
					y += changedLines[index];
				}
				index++;
			}
			if (rectCount)
				SDL_UpdateRects( sdl.surface, rectCount, sdl.updateRects );
		}
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		SDL_UnlockSurface(sdl.blit.surface);
		ret=IDirectDrawSurface3_Blt(
			sdl.surface->hwdata->dd_writebuf,&sdl.blit.rect,
			sdl.blit.surface->hwdata->dd_surface,0,
			DDBLT_WAIT, NULL);
		switch (ret) {
		case DD_OK:
			break;
		case DDERR_SURFACELOST:
			IDirectDrawSurface3_Restore(sdl.blit.surface->hwdata->dd_surface);
			IDirectDrawSurface3_Restore(sdl.surface->hwdata->dd_surface);
			break;
		default:
			LOG_MSG("DDRAW:Failed to blit, error %X",ret);
		}
		SDL_Flip(sdl.surface);
		break;
#endif
	case SCREEN_OVERLAY:
		SDL_UnlockYUVOverlay(sdl.overlay);
		SDL_DisplayYUVOverlay(sdl.overlay,&sdl.clip);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range) {
            glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
					sdl.draw.width, sdl.draw.height, GL_BGRA_EXT,
					GL_UNSIGNED_INT_8_8_8_8_REV, sdl.opengl.framebuf);
			glCallList(sdl.opengl.displaylist);
			SDL_GL_SwapBuffers();
		} else
#endif
		if (changedLines) {
			Bitu y = 0, index = 0;
            glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					Bit8u *pixels = (Bit8u *)sdl.opengl.framebuf + y * sdl.opengl.pitch;
					Bitu height = changedLines[index];
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, 
						sdl.draw.width, height, GL_BGRA_EXT,
						GL_UNSIGNED_INT_8_8_8_8_REV, pixels );
					y += height;
				}
				index++;
			}
			glCallList(sdl.opengl.displaylist);
			SDL_GL_SwapBuffers();
		}
		break;
#endif

	}
}


void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
	/* I should probably not change the GFX_PalEntry :) */
	if (sdl.surface->flags & SDL_HWPALETTE) {
		if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	} else {
		if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
	case SCREEN_SURFACE_DDRAW:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_OVERLAY:
		{
			Bit8u y =  ( 9797*(red) + 19237*(green) +  3734*(blue) ) >> 15;
			Bit8u u =  (18492*((blue)-(y)) >> 15) + 128;
			Bit8u v =  (23372*((red)-(y)) >> 15) + 128;
#ifdef WORDS_BIGENDIAN
			return (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
			return (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
		}
	case SCREEN_OPENGL:
//		return ((red << 0) | (green << 8) | (blue << 16)) | (255 << 24);
		//USE BGRA
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	}
	return 0;
}

void GFX_Stop() {
	if (sdl.updating) 
		GFX_EndUpdate( 0 );
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

static void GUI_ShutDown(Section * sec) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}

static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}

static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest 
// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus ) && 
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
		break;
#endif
	default:
		break;
	}
}

static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;

	/* Set Icon (must be done before any sdl_setvideomode call) */
#if WORDS_BIGENDIAN
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif
	SDL_WM_SetIcon(logos,NULL);

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.wait_on_error=section->Get_bool("waitonerror");
	const char * priority=section->Get_string("priority");
	if (priority && priority[0]) {
		Bitu next;
		if (!strncasecmp(priority,"lowest",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_LOWEST;next=6;
		} else if (!strncasecmp(priority,"lower",5)) {
			sdl.priority.focus=PRIORITY_LEVEL_LOWER;next=5;
		} else if (!strncasecmp(priority,"normal",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_NORMAL;next=6;
		} else if (!strncasecmp(priority,"higher",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_HIGHER;next=6;
		} else if (!strncasecmp(priority,"highest",7)) {
			sdl.priority.focus=PRIORITY_LEVEL_HIGHEST;next=7;
		} else {
			next=0;sdl.priority.focus=PRIORITY_LEVEL_HIGHER;
		}
		priority=&priority[next];
		if (next && priority[0]==',' && priority[1]) {
			priority++;
			if (!strncasecmp(priority,"lowest",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;
			} else if (!strncasecmp(priority,"lower",5)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;
			} else if (!strncasecmp(priority,"normal",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
			} else if (!strncasecmp(priority,"higher",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;
			} else if (!strncasecmp(priority,"highest",7)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST;
			} else if (!strncasecmp(priority,"pause",5)) {
				/* we only check for pause here, because it makes no sense
				 * for DOSBox to be paused while it has focus
				 */
				sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
			} else {
				sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
			}
		} else sdl.priority.nofocus=sdl.priority.focus;
	} else {
		sdl.priority.focus=PRIORITY_LEVEL_HIGHER;
		sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
	}
	SetPriority(sdl.priority.focus); //Assume focus on startup
	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;
	sdl.desktop.full.fixed=false;
	const char* fullresolution=section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0;
	sdl.desktop.full.height = 0;
	if(fullresolution && *fullresolution) {
		char res[100];
		strncpy( res, fullresolution, sizeof( res ));
		fullresolution = lowcase (res);//so x and X are allowed
		if(strcmp(fullresolution,"original")) {
			sdl.desktop.full.fixed = true;
			char* height = const_cast<char*>(strchr(fullresolution,'x'));
			if(height && * height) {
				*height = 0;
				sdl.desktop.full.height = atoi(height+1);
				sdl.desktop.full.width  = atoi(res);
			}
		}
	}

	sdl.desktop.window.width  = 0;
	sdl.desktop.window.height = 0;
	const char* windowresolution=section->Get_string("windowresolution");
	if(windowresolution && *windowresolution) {
		char res[100];
		strncpy( res,windowresolution, sizeof( res ));
		windowresolution = lowcase (res);//so x and X are allowed
		if(strcmp(windowresolution,"original")) {
			char* height = const_cast<char*>(strchr(windowresolution,'x'));
			if(height && *height) {
				*height = 0;
				sdl.desktop.window.height = atoi(height+1);
				sdl.desktop.window.width  = atoi(res);
			}
		}
	}
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");
	if (!sdl.desktop.full.width) {
#ifdef WIN32
		sdl.desktop.full.width=GetSystemMetrics(SM_CXSCREEN);
#else	
		sdl.desktop.full.width=1024;
#endif
	}
	if (!sdl.desktop.full.height) {
#ifdef WIN32
		sdl.desktop.full.height=GetSystemMetrics(SM_CYSCREEN);
#else	
		sdl.desktop.full.height=768;
#endif
	}
	sdl.mouse.autoenable=section->Get_bool("autolock");
	if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock=false;
	sdl.mouse.sensitivity=section->Get_int("sensitivity");
	const char * output=section->Get_string("output");
	if (!strcasecmp(output,"surface")) {
		sdl.desktop.want_type=SCREEN_SURFACE;
#if (HAVE_DDRAW_H) && defined(WIN32)
	} else if (!strcasecmp(output,"ddraw")) {
		sdl.desktop.want_type=SCREEN_SURFACE_DDRAW;
#endif
	} else if (!strcasecmp(output,"overlay")) {
		sdl.desktop.want_type=SCREEN_OVERLAY;
#if C_OPENGL
	} else if (!strcasecmp(output,"opengl")) {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=true;
	} else if (!strcasecmp(output,"openglnb")) {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=false;
#endif
	} else {
		LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output);
		sdl.desktop.want_type=SCREEN_SURFACE;
	}

	sdl.overlay=0;
#if C_OPENGL
   if(sdl.desktop.want_type==SCREEN_OPENGL){ /* OPENGL is requested */
	sdl.surface=SDL_SetVideoMode(640,400,0,SDL_OPENGL);
	if (sdl.surface == NULL) {
		LOG_MSG("Could not initialize OpenGL, switching back to surface");
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else {
	sdl.opengl.framebuf=0;
	sdl.opengl.texture=0;
	sdl.opengl.displaylist=0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
#if defined(__WIN32__) && defined(NVIDIA_PixelDataRange)
	glPixelDataRangeNV = (PFNGLPIXELDATARANGENVPROC) wglGetProcAddress("glPixelDataRangeNV");
	db_glAllocateMemoryNV = (PFNWGLALLOCATEMEMORYNVPROC) wglGetProcAddress("wglAllocateMemoryNV");
	db_glFreeMemoryNV = (PFNWGLFREEMEMORYNVPROC) wglGetProcAddress("wglFreeMemoryNV");
#endif
	const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
	if(gl_ext && *gl_ext){
		sdl.opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") > 0);
		sdl.opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") > 0);
#if defined(NVIDIA_PixelDataRange)
		sdl.opengl.pixel_data_range=(strstr(gl_ext,"GL_NV_pixel_data_range") >0 ) &&
			glPixelDataRangeNV && db_glAllocateMemoryNV && db_glFreeMemoryNV;
		sdl.opengl.pixel_data_range = 0;					
#endif
    	} else {
		sdl.opengl.packed_pixel=sdl.opengl.paletted_texture=false;
	}
	}
	} /* OPENGL is requested end */
   
#endif	//OPENGL
	/* Initialize screen for first time */
	sdl.surface=SDL_SetVideoMode(640,400,0,0);
	if (sdl.surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
	sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
	}
	GFX_Stop();
/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(PauseDOSBox,MK_pause,MMOD2,"pause","Pause");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDLMod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
	else {
		SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
		sdl.mouse.requestlock=false;
	}
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (sdl.mouse.locked || !sdl.mouse.autoenable) 
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.sensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
			break;
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

static Bit8u laltstate = SDL_KEYUP;
static Bit8u raltstate = SDL_KEYUP;

void GFX_Events() {
	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay=0;
	int time=GetTicks();
	if (time-poll_delay>20) {
		poll_delay=time;
		if (sdl.num_joysticks>0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (event.active.gain) {
					if (sdl.desktop.fullscreen && !sdl.mouse.locked)
						GFX_CaptureMouse();
					SetPriority(sdl.priority.focus);
				} else {
					if (sdl.mouse.locked) {
#ifdef WIN32
						if (sdl.desktop.fullscreen) {
							VGA_KillDrawing();
							sdl.desktop.fullscreen=false;
							GFX_ResetScreen();
						}
#endif
						GFX_CaptureMouse();
					}
					SetPriority(sdl.priority.nofocus);
					MAPPER_LosingFocus();
				}
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt-Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
					KEYBOARD_ClrBuffer();
					SDL_Delay(500);
					while (SDL_PollEvent(&ev)) {
						// flush event queue.
					}

					while (paused) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event. 
						case SDL_ACTIVEEVENT:     // wait until we get window focus back
							if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
								// We've got focus back, so unpause and break out of the loop
								if (ev.active.gain) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
							}
							break;
						}
					}
				}
			}
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_VIDEORESIZE:
//			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		case SDL_VIDEOEXPOSE:
			if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
			break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym==SDLK_LALT) laltstate = event.key.type;
			if (event.key.keysym.sym==SDLK_RALT) raltstate = event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) &&
				((laltstate==SDL_KEYDOWN) || (raltstate==SDL_KEYDOWN))) break;
#endif
		default:
			void MAPPER_CheckEvent(SDL_Event * event);
			MAPPER_CheckEvent(&event);
		}
	}
}

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif

#endif

#ifdef PSP
#include <pspiofilemgr.h>
#include <psppower.h>
#include <pspsuspend.h>
#include <pspsysmem_kernel.h>
#include <pspmodulemgr.h>
#include "regs.h"

extern "C" {

static const char x86_regs[][4] = {
	"eax","ecx","edx","ebx","esp","ebp","esi","edi","es","cs","ss","ds","fs","gs","eip" };
	
static void dump_x86_regs() {
	int i;
	pspDebugScreenPrintf("\n");
        for(i = 0; i < 8; i+=4)
                pspDebugScreenPrintf("%s:%08X %s:%08X %s:%08X %s:%08X\n", x86_regs[i], cpu_regs.regs[i].dword[DW_INDEX],
                                x86_regs[i+1], cpu_regs.regs[i+1].dword[DW_INDEX],x86_regs[i+2], cpu_regs.regs[i+2].dword[DW_INDEX],
				x86_regs[i+3], cpu_regs.regs[i+3].dword[DW_INDEX]);
	
	pspDebugScreenPrintf("%s:%08X %s:%08X %s:%08X %s:%08X\n", x86_regs[8], Segs.val[0],
                                x86_regs[9], Segs.val[1],x86_regs[10], Segs.val[2], x86_regs[11], Segs.val[3]);
	pspDebugScreenPrintf("%s:%08X %s:%08X %s:%08X\n", x86_regs[12], Segs.val[4], 
                                x86_regs[13], Segs.val[5], x86_regs[14], cpu_regs.ip.dword[DW_INDEX]);
}

static bool in_exception = false;

static void trap_handler(PspDebugRegBlock *regs) {
	if(in_exception) __asm__ volatile(".word 0x70000000");
	in_exception = true;
	pspDebugScreenInit();
	pspDebugScreenPrintf("%s\n", build_version);
	pspDebugDumpException(regs);
	dump_x86_regs();
	sceKernelSleepThread();
}

#ifdef PSPME
void me_trap_handler(PspDebugRegBlock *regs) {
	pspDebugScreenInit();
	pspDebugScreenPrintf("ME exception\n");
	pspDebugDumpException(regs);
	dump_x86_regs();
	sceKernelSleepThread();
}
#endif
PSP_MODULE_INFO("DOSBox", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER|PSP_THREAD_ATTR_VFPU);

static PspDebugRegBlock exception_regs;
char *realpath(const char *file_name, char *resolved_name);

__attribute__((constructor)) void install_trap_handler() {
	SceKernelLMOption option;
	char path[MAXPATHLEN];
	int args[2], fd, modid;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.mpidtext = PSP_MEMORY_PARTITION_KERNEL;
	option.mpiddata = PSP_MEMORY_PARTITION_KERNEL;
	option.position = 0;
	option.access = 1;

	if(realpath("exception.prx", path) && ((modid = sceKernelLoadModule(path, 0, &option)) >= 0)) {
		args[0] = (int)trap_handler;
		args[1] = (int)&exception_regs;
		sceKernelStartModule(modid, 8, args, &fd, NULL);
	}

	if(realpath("fixup.prx", path) && ((modid = sceKernelLoadModule(path, 0, &option)) >= 0)) {
		fixup = true;
		sceKernelStartModule(modid, 0, args, &fd, NULL);
		sceKernelUnloadModule(modid);
	}
}

static SceUID main_thid;
static int exit_cbid;

static void end_game() {
#ifdef PSPME
	stop_me();
#endif		
	sceKernelVolatileMemUnlock(0);
	sceKernelExitGame();
}

extern bool cache_initialized;

static int psp_exit_callback(int arg1, int arg2, void *common) {
	sceKernelTerminateDeleteThread(main_thid);
	exit(0);
	return 0;
}

static int psp_power_callback(int unknown, int powerInfo, void *nothing) {
	if((powerInfo & PSP_POWER_CB_POWER_SWITCH) && (suspended == false)) {
		sceKernelWaitSema(main_thsema, 1, NULL);
		suspended = true;
		reinit = cache_initialized;
		cache_initialized = false;
		if(reinit) cache_free_memory();
		sceKernelVolatileMemUnlock(0);
	} else if((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) && (suspended == true)) 
		sceKernelSignalSema(main_thsema, 1);		// doing VolatileMemLock in the callback doesn't work
	return 0;
}


static int psp_callback_thread(SceSize args, void *argp) {
	int cbid;
	cbid = sceKernelCreateCallback("Power Callback", psp_power_callback, NULL);
	scePowerRegisterCallback(0, cbid);
	exit_cbid = sceKernelCreateCallback("Exit Callback", psp_exit_callback, NULL);
	sceKernelRegisterExitCallback(exit_cbid);
	sceKernelSleepThreadCB();
}

static int psp_setup_callbacks(void) {
	int thid = 0;
	atexit(end_game);
	main_thid = sceKernelGetThreadId();
	main_thsema = sceKernelCreateSema("Suspend Sema", 0, 0, 1, NULL);
	thid = sceKernelCreateThread("update_thread", psp_callback_thread, 0x11, 512, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0) sceKernelStartThread(thid, 0, 0);
	return thid;
}
}
#endif


/* static variable to show wether there is not a valid stdout.
 * Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;

void GFX_ShowMsg(char const* format,...) {
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
        strcat(buf,"\n");
	va_end(msg);
	if(!no_stdout) printf(buf);       
};

extern "C" int main(int argc, char* argv[]) {
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;

#ifdef PSP
		psp_setup_callbacks();
		pspSdkDisableFPUExceptions();
#ifdef PSPME
		init_me();
#endif
#endif

		if (control->cmdline->FindExist("-version") || 
		    control->cmdline->FindExist("--version") ) {
			printf(VERSION "\n");
			return 0;
		}
	   

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			if(freopen(STDOUT_FILE, "w", stdout) == NULL)
				no_stdout = true; // No stdout so don't write messages
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
			setbuf(stderr, NULL);					/* No buffering */
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","r",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif  //defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-version") || 
		    control->cmdline->FindExist("--version") ) {
			printf("\nDOSBox version %s, copyright 2002-2007 DOSBox Team.\n\n",VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return 0;
		}

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

#ifdef OS2
        PPIB pib;
        PTIB tib;
        DosGetInfoBlocks(&tib, &pib);
        if (pib->pib_ultype == 2) pib->pib_ultype = 3;
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
#endif
#ifdef USE_SDL
	if ( SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_CDROM
		|SDL_INIT_NOPARACHUTE
		) < 0 ) E_Exit("Can't init SDL %s",SDL_GetError());

#ifndef DISABLE_JOYSTICK
	//Initialise Joystick seperately. This way we can warn when it fails instead
	//of exiting the application
	if( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) LOG_MSG("Failed to init joystick support");
#endif

#if defined (WIN32)
#if SDL_VERSION_ATLEAST(1, 2, 10)
		sdl.using_windib=true;
#else
		sdl.using_windib=false;
#endif
		char sdl_drv_name[128];
		if (getenv("SDL_VIDEODRIVER")==NULL) {
			if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
				sdl.using_windib=false;
				if (strcmp(sdl_drv_name,"directx")!=0) {
					SDL_QuitSubSystem(SDL_INIT_VIDEO);
					putenv("SDL_VIDEODRIVER=directx");
					if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) {
						putenv("SDL_VIDEODRIVER=windib");
						if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
						sdl.using_windib=true;
					}
				}
			}
		} else {
			char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
			if (strcmp(sdl_videodrv,"directx")==0) sdl.using_windib = false;
			else if (strcmp(sdl_videodrv,"windib")==0) sdl.using_windib = true;
		}
		if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
			if (strcmp(sdl_drv_name,"windib")==0) LOG_MSG("SDL_Init: Starting up with SDL windib video driver.\n          Try to update your video card and directx drivers!");
		}
#endif
		sdl.num_joysticks=SDL_NumJoysticks();
#endif
		Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
#ifdef USE_SDL
		sdl_sec->AddInitFunction(&MAPPER_StartUp);
#endif
		sdl_sec->Add_bool("fullscreen",false);
		sdl_sec->Add_bool("fulldouble",false);
		sdl_sec->Add_string("fullresolution","original");
		sdl_sec->Add_string("windowresolution","original");
		sdl_sec->Add_string("output","surface");
		sdl_sec->Add_bool("autolock",true);
		sdl_sec->Add_int("sensitivity",100);
		sdl_sec->Add_bool("waitonerror",true);
		sdl_sec->Add_string("priority","higher,normal");
		sdl_sec->Add_string("mapperfile","mapper.txt");
		sdl_sec->Add_bool("usescancodes",true);
		sdl_sec->Add_string("irkeybini","");
		sdl_sec->Add_bool("keyhint",false);

		MSG_Add("SDL_CONFIGFILE_HELP",
			"fullscreen -- Start dosbox directly in fullscreen.\n"
			"fulldouble -- Use double buffering in fullscreen.\n"
			"fullresolution -- What resolution to use for fullscreen: original or fixed size (e.g. 1024x768).\n"
			"windowresolution -- Scale the window to this size IF the output device supports hardware scaling.\n"
			"output -- What to use for output: surface,overlay"
#if C_OPENGL
			",opengl,openglnb"
#endif
#if (HAVE_DDRAW_H) && defined(WIN32)
			",ddraw"
#endif
			".\n"
			"autolock -- Mouse will automatically lock, if you click on the screen.\n"
			"sensitiviy -- Mouse sensitivity.\n"
			"waitonerror -- Wait before closing the console if dosbox has an error.\n"
			"priority -- Priority levels for dosbox: lowest,lower,normal,higher,highest,pause (when not focussed).\n"
			"            Second entry behind the comma is for when dosbox is not focused/minimized.\n"
			"mapperfile -- File used to load/save the key/event mappings from.\n"
			"usescancodes -- Avoid usage of symkeys, might not work on all operating systems.\n"
			);
		/* Init all the dosbox subsystems */
		DOSBOX_Init();
		std::string config_file;
		bool parsed_anyconfigfile = false;
		// First parse the configfile in the $HOME directory
		if ((getenv("HOME") != NULL)) {
			config_file = (std::string)getenv("HOME") + 
				      (std::string)DEFAULT_CONFIG_FILE;
			if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		}
		// Add extra settings from dosbox.conf in the local directory if there is no configfile specified at the commandline
		if (!control->cmdline->FindString("-conf",config_file,true)) config_file="dosbox.conf";
		if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		// Add extra settings from additional configfiles at the commandline
		while(control->cmdline->FindString("-conf",config_file,true))
			if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		// Give a message if no configfile whatsoever was found.
		if(!parsed_anyconfigfile) LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
	
#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		/* Init all the sections */
		control->Init();
#ifdef USE_SDL
		/* Some extra SDL Functions */
		if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not allready in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_Run(true);
#endif
		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
#ifdef USE_SDL
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif 
		}
#elif defined(PSP)
		pspIrKeybFinish();
		pspDebugScreenInit();
		pspDebugScreenPrintf("Exit to error: %s", error);
		sceKernelSleepThread();
#endif
	}
	catch (int){ 
		;//nothing pressed killswitch
	}
#ifndef PSP
	catch(...){
		throw;//dunno what happened. rethrow for sdl to catch 
	}
#else
	sceKernelNotifyCallback(exit_cbid, 0);
	sceKernelSleepThread();
#endif
#ifdef USE_SDL
	SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
#endif
	return 0;
};
