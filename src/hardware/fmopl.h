/* ScummVM - Scumm Interpreter
 * Copyright (C) 1999-2000 Tatsuyuki Satoh
 * Copyright (C) 2001-2006 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 * LGPL licensed version of MAMEs fmopl (V0.37a modified) by
 * Tatsuyuki Satoh. Included from LGPL'ed AdPlug.
 */


#ifndef SOUND_FMOPL_H
#define SOUND_FMOPL_H

#ifndef PI
#define PI 3.14159265358979323846
#endif

typedef Bit8u uint8;
typedef Bit16s int16;

typedef void (*OPL_TIMERHANDLER)(int channel,float interval_Sec);
typedef void (*OPL_IRQHANDLER)(int param,int irq);
typedef void (*OPL_UPDATEHANDLER)(int param,int min_interval_us);

#define OPL_TYPE_WAVESEL   0x01  /* waveform select    */

/* Saving is necessary for member of the 'R' mark for suspend/resume */
/* ---------- OPL one of slot  ---------- */
typedef struct fm_opl_slot {
	int TL;		/* total level     :TL << 8				*/
	int TLL;	/* adjusted now TL						*/
	uint8 KSR;	/* key scale rate  :(shift down bit)	*/
	int *AR;	/* attack rate     :&AR_TABLE[AR<<2]	*/
	int *DR;	/* decay rate      :&DR_TABLE[DR<<2]	*/
	int SL;		/* sustain level   :SL_TABLE[SL]		*/
	int *RR;	/* release rate    :&DR_TABLE[RR<<2]	*/
	uint8 ksl;	/* keyscale level  :(shift down bits)	*/
	uint8 ksr;	/* key scale rate  :kcode>>KSR			*/
	uint mul;	/* multiple        :ML_TABLE[ML]		*/
	uint Cnt;	/* frequency count						*/
	uint Incr;	/* frequency step						*/

	/* envelope generator state */
	uint8 eg_typ;/* envelope type flag					*/
	uint8 evm;	/* envelope phase						*/
	int evc;	/* envelope counter						*/
	int eve;	/* envelope counter end point			*/
	int evs;	/* envelope counter step				*/
	int evsa;	/* envelope step for AR :AR[ksr]		*/
	int evsd;	/* envelope step for DR :DR[ksr]		*/
	int evsr;	/* envelope step for RR :RR[ksr]		*/

	/* LFO */
	uint8 ams;		/* ams flag                            */
	uint8 vib;		/* vibrate flag                        */
	/* wave selector */
	int **wavetable;
} OPL_SLOT;

/* ---------- OPL one of channel  ---------- */
typedef struct fm_opl_channel {
	OPL_SLOT SLOT[2];
	uint8 CON;			/* connection type					*/
	uint8 FB;			/* feed back       :(shift down bit)*/
	int *connect1;		/* slot1 output pointer				*/
	int *connect2;		/* slot2 output pointer				*/
	int op1_out[2];		/* slot1 output for selfeedback		*/

	/* phase generator state */
	uint block_fnum;	/* block+fnum						*/
	uint8 kcode;		/* key code        : KeyScaleCode	*/
	uint fc;			/* Freq. Increment base				*/
	uint ksl_base;		/* KeyScaleLevel Base step			*/
	uint8 keyon;		/* key on/off flag					*/
} OPL_CH;

/* OPL state */
typedef struct fm_opl_f {
	uint8 type;			/* chip type                         */
	int clock;			/* master clock  (Hz)                */
	int rate;			/* sampling rate (Hz)                */
	float freqbase;	/* frequency base                    */
	float TimerBase;	/* Timer base time (==sampling time) */
	uint8 address;		/* address register                  */
	uint8 status;		/* status flag                       */
	uint8 statusmask;	/* status mask                       */
	uint mode;			/* Reg.08 : CSM , notesel,etc.       */

	/* Timer */
	int T[2];			/* timer counter                     */
	uint8 st[2];		/* timer enable                      */

	/* FM channel slots */
	OPL_CH *P_CH;		/* pointer of CH                     */
	int	max_ch;			/* maximum channel                   */

	/* Rythm sention */
	uint8 rythm;		/* Rythm mode , key flag */

	/* time tables */
	int AR_TABLE[76];	/* atttack rate tables				*/
	int DR_TABLE[76];	/* decay rate tables				*/
	uint FN_TABLE[1024];/* fnumber -> increment counter		*/

	/* LFO */
	int *ams_table;
	int *vib_table;
	int amsCnt;
	int amsIncr;
	int vibCnt;
	int vibIncr;

	/* wave selector enable flag */
	uint8 wavesel;

	/* external event callback handler */
	OPL_TIMERHANDLER  TimerHandler;		/* TIMER handler   */
	int TimerParam;						/* TIMER parameter */
	OPL_IRQHANDLER    IRQHandler;		/* IRQ handler    */
	int IRQParam;						/* IRQ parameter  */
	OPL_UPDATEHANDLER UpdateHandler;	/* stream update handler   */
	int UpdateParam;					/* stream update parameter */
} FM_OPL;

/* ---------- Generic interface section ---------- */
#define OPL_TYPE_YM3526 (0)
#define OPL_TYPE_YM3812 (OPL_TYPE_WAVESEL)

#if 0
void OPLBuildTables(int ENV_BITS_PARAM, int EG_ENT_PARAM);

FM_OPL *OPLCreate(int type, int clock, int rate);
void OPLDestroy(FM_OPL *OPL);
void OPLSetTimerHandler(FM_OPL *OPL, OPL_TIMERHANDLER TimerHandler, int channelOffset);
void OPLSetIRQHandler(FM_OPL *OPL, OPL_IRQHANDLER IRQHandler, int param);
void OPLSetUpdateHandler(FM_OPL *OPL, OPL_UPDATEHANDLER UpdateHandler, int param);

void OPLResetChip(FM_OPL *OPL);
int OPLWrite(FM_OPL *OPL, int a, int v);
unsigned char OPLRead(FM_OPL *OPL, int a);
int OPLTimerOver(FM_OPL *OPL, int c);
void OPLWriteReg(FM_OPL *OPL, int r, int v);
void YM3812UpdateOne(FM_OPL *OPL, int16 *buffer, int length, int output_rate);
FM_OPL *makeAdlibOPL(int rate);
#endif
#endif

