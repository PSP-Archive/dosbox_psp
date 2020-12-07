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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "mapper.h"
#include "mem.h"

/* 
	Thanks to vdmsound for nice simple way to implement this
*/

#define logerror

#ifdef _MSC_VER
  /* Disable recurring warnings */
# pragma warning ( disable : 4018 )
# pragma warning ( disable : 4244 )
#endif

struct __MALLOCPTR {
	void* m_ptr;

	__MALLOCPTR(void) : m_ptr(NULL) { }
	__MALLOCPTR(void* src) : m_ptr(src) { }
	void* operator=(void* rhs) { return (m_ptr = rhs); }
	operator int*() const { return (int*)m_ptr; }
	operator int**() const { return (int**)m_ptr; }
	operator char*() const { return (char*)m_ptr; }
};

namespace OPL2 {
	#include "fmopl.c"
}
#define OPL2_INTERNAL_FREQ    3600000   // The OPL2 operates at 3.6MHz

class OPL: public Module_base {
private:
	IO_ReadHandleObject ReadHandler[2];
	IO_WriteHandleObject WriteHandler[2];
	MixerObject MixerChan;
public:
	OPL2::FM_OPL * YM3812;
	OPL(Section* configuration);
	~OPL() {
		OPL2::OPLDestroy(YM3812);
	}
};

static OPL* test;

static struct {
	bool active;
	MixerChannel * chan;
	Bit32u last_used;
} opl;

Bit8u adlib_commandreg;

void TimerOver(Bitu val){
	OPL2::OPLTimerOver(test->YM3812,val & 0xff);
}
void TimerHandler(int channel,float interval_Sec) {
	if (interval_Sec==0.0) return;
	PIC_AddEvent(TimerOver,1000.0f*interval_Sec,channel);		
}

static void OPL_CallBack(Bitu len) {
	/* Check for size to update and check for 1 ms updates to the opl registers */
	OPL2::YM3812UpdateOne(test->YM3812,(OPL2::int16 *)MixTemp,len);
	opl.chan->AddSamples_m16(len,(Bit16s*)MixTemp);
	if ((PIC_Ticks-opl.last_used)>30000) {
		opl.chan->Enable(false);
		opl.active=false;
	}
}

Bitu OPL_Read(Bitu port,Bitu iolen) {
	Bitu addr=port & 1;
	return OPL2::OPLRead(test->YM3812, addr) | 0x06;
}

void OPL_Write(Bitu port,Bitu val,Bitu iolen) {
	opl.last_used=PIC_Ticks;
	if (!opl.active) {
		opl.active=true;
		opl.chan->Enable(true);
	}
	port&=1;
	if (!port) adlib_commandreg=val;
	OPL2::OPLWrite(test->YM3812,port,val);
}

OPL::OPL(Section* configuration):Module_base(configuration) {
	Section_prop * section=static_cast<Section_prop *>(configuration);
	Bitu base = section->Get_hex("sbbase");
	Bitu rate = section->Get_int("oplrate");
	if (!(YM3812 = OPL2::OPLCreate(OPL_TYPE_YM3812, OPL2_INTERNAL_FREQ, rate))) {
		E_Exit("Can't create OPL2 Emulator");	
	};
	OPL2::OPLSetTimerHandler(YM3812,TimerHandler,0);
	WriteHandler[0].Install(0x388,OPL_Write,IO_MB,2);
	ReadHandler[0].Install(0x388,OPL_Read,IO_MB,2);
	WriteHandler[1].Install(base,OPL_Write,IO_MB,2);
	ReadHandler[1].Install(base,OPL_Read,IO_MB,2);

	opl.active=false;
	opl.last_used=0;
	opl.chan=MixerChan.Install(OPL_CallBack,rate,"FM");
}

//Initialize static members
void OPL_Init(Section* sec,OPL_Mode oplmode) {
	test = new OPL(sec);
}

void OPL_ShutDown(Section* sec){
	delete test;
}
