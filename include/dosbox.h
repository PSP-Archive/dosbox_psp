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

#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "config.h"

#ifndef PSP
#define USE_GUS 1
#define USE_SDL 1
#else
//#define PSPME
typedef struct profile_regs_ll
{
	unsigned long long systemck;
	unsigned long long cpuck;
	unsigned long long internal;
	unsigned long long memory;
	unsigned long long copz;
	unsigned long long vfpu;
	unsigned long long sleep;
	unsigned long long bus_access;
	unsigned long long uncached_load;
	unsigned long long uncached_store;
	unsigned long long cached_load;
	unsigned long long cached_store;
	unsigned long long i_miss;
	unsigned long long d_miss;
	unsigned long long d_writeback;
	unsigned long long cop0_inst;
	unsigned long long fpu_inst;
	unsigned long long vfpu_inst;
	unsigned long long local_bus;
} profile_regs_ll;
#endif

// deal with unaligned memory
typedef struct { Bit32u val __attribute__((packed)); } unaligned_word;
typedef struct { Bit16u val __attribute__((packed)); } unaligned_half;

void E_Exit(const char * message,...) GCC_ATTRIBUTE( __format__(__printf__, 1, 2));

void MSG_Add(const char*,const char*); //add messages to the internal langaugefile
const char* MSG_Get(char const *);     //get messages from the internal langaugafile

class Section;

typedef Bitu (LoopHandler)(void);

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_Init(void);

class Config;
extern Config * control;

enum MachineType {
	MCH_HERC,
	MCH_CGA,
	MCH_TANDY,
	MCH_PCJR,
	MCH_VGA
};

enum SVGACards {
	SVGA_None,
	SVGA_S3Trio
}; 

extern SVGACards svgaCard;
extern MachineType machine;
extern bool SDLNetInited;

#define IS_TANDY_ARCH ((machine==MCH_TANDY) || (machine==MCH_PCJR))
#define TANDY_ARCH_CASE MCH_TANDY: case MCH_PCJR

#ifndef DOSBOX_LOGGING_H
#include "logging.h"
#endif // the logging system.

#endif /* DOSBOX_DOSBOX_H */
