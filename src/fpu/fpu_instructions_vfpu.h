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

/* $Id: fpu_instructions.h,v 1.31 2007/01/08 19:45:39 qbix79 Exp $ */

// stuff from libpspmath by MrMr[iCE]

static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
	fpu.sw = 0;
	TOP=FPU_GET_TOP();
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us
}

static void FPU_FCLEX(void){
	fpu.sw &= 0x7f00;			//should clear exceptions
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PUSHF(FVAL in){
	TOP = (TOP - 1) &7;
	//actually check if empty
	fpu.tags[TOP] = TAG_Valid;
	fpu.regs[TOP].d = in;
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.tags[TOP] = TAG_Valid;
}

static void FPU_FPOP(void){
	fpu.tags[TOP]=TAG_Empty;
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static void FPU_FADD(Bitu op1, Bitu op2){
	fpu.regs[op1].d+=fpu.regs[op2].d;
	//flags and such :)
	return;
}

static void FPU_FDIV(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[st].d/fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FDIVR(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[other].d/fpu.regs[st].d;
	// flags and such :)
	return;
};

static void FPU_FMUL(Bitu st, Bitu other){
	fpu.regs[st].d*=fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUB(Bitu st, Bitu other){
	fpu.regs[st].d = fpu.regs[st].d - fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUBR(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[other].d - fpu.regs[st].d;
	//flags and such :)
	return;
}

static void FPU_FXCH(Bitu st, Bitu other){
	FPU_Tag tag = fpu.tags[other];
	FPU_Reg reg = fpu.regs[other];
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];
	fpu.tags[st] = tag;
	fpu.regs[st] = reg;
}

static void FPU_FST(Bitu st, Bitu other){
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];
}


static void FPU_FCOM(Bitu st, Bitu other){
	if(((fpu.tags[st] != TAG_Valid) && (fpu.tags[st] != TAG_Zero)) || 
		((fpu.tags[other] != TAG_Valid) && (fpu.tags[other] != TAG_Zero))){
		FPU_SET_C3(1);FPU_SET_C2(1);FPU_SET_C0(1);return;
	}
	if(fpu.regs[st].d == fpu.regs[other].d){
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
	}
	if(fpu.regs[st].d < fpu.regs[other].d){
		FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(1);return;
	}
	// st > other
	FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(0);return;
}

static INLINE void FPU_FUCOM(Bitu st, Bitu other){
	//does atm the same as fcom 
	FPU_FCOM(st,other);
}

static void FPU_FLD_F32(PhysPt addr,Bitu store_to) {
	union {
		float f;
		Bit32u l;
	}	blah;
	blah.l = mem_readd(addr);
	fpu.regs[store_to].d = static_cast<FVAL>(blah.f);
}

static void FPU_FLD_I16(PhysPt addr,Bitu store_to) {
	Bit16s blah = mem_readw(addr);
	fpu.regs[store_to].d = static_cast<FVAL>(blah);
}

static void FPU_FLD_I32(PhysPt addr,Bitu store_to) {
	Bit32s blah = mem_readd(addr);
	fpu.regs[store_to].d = static_cast<FVAL>(blah);
}

static void FPU_FBLD(PhysPt addr,Bitu store_to) {
	Bit64u val = 0;
	Bitu in = 0;
	Bit64u base = 1;
	for(Bitu i = 0;i < 9;i++){
		in = mem_readb(addr + i);
		val += ( (in&0xf) * base); //in&0xf shouldn't be higher then 9
		base *= 10;
		val += ((( in>>4)&0xf) * base);
		base *= 10;
	}

	//last number, only now convert to float in order to get
	//the best signification
	FVAL temp = static_cast<FVAL>(val);
	in = mem_readb(addr + 9);
	temp += ( (in&0xf) * base );
	if(in&0x80) temp *= -1.0;
	fpu.regs[store_to].d = temp;
}

static void FPU_FST_F32(PhysPt addr) {
	union {
		float f;
		Bit32u l;
	}	blah;
	//should depend on rounding method
	blah.f = static_cast<float>(fpu.regs[TOP].d);
	mem_writed(addr,blah.l);
}

static void FPU_FBST(PhysPt addr) {
	FPU_Reg val = fpu.regs[TOP];
	bool sign = false;
	if(val.d<0.0){ //sign
		sign=true;
		val.d=-val.d;
	}
	//numbers from back to front
	Real32 temp=val.d;
	Bitu p;
	for(Bitu i=0;i<9;i++){
		val.d=temp;
		temp = static_cast<Real32>(static_cast<Bit32s>(floorf(val.d/10.0)));
		p = static_cast<Bitu>(val.d - 10.0*temp);  
		val.d=temp;
		temp = static_cast<Real32>(static_cast<Bit32s>(floorf(val.d/10.0)));
		p |= (static_cast<Bitu>(val.d - 10.0*temp)<<4);

		mem_writeb(addr+i,p);
	}
	val.d=temp;
	temp = static_cast<Real32>(static_cast<Bit32s>(floorf(val.d/10.0)));
	p = static_cast<Bitu>(val.d - 10.0*temp);
	if(sign)
		p|=0x80;
	mem_writeb(addr+9,p);
}

static float FROUND(float in){
	switch(fpu.round){
	case ROUND_Nearest:	
		return (roundf(in));
	case ROUND_Down:
		return (floorf(in));
	case ROUND_Up:
		return (ceilf(in));
	case ROUND_Chop:
		return (truncf(in));
	default:
		return in;
	}
}

#define BIAS80 16383
#define BIAS32 127

static Real32 FPU_FLD_F80(PhysPt addr) {
	struct{
		Bit16s begin;
		Bit32s upper;
	} test;
//	test.lower=mem_readd(addr);  // no point
	test.upper=mem_readd(addr+4);
	test.begin=mem_readw(addr+8);

	Bit32s exp32 = (test.begin & 0x7fff) - BIAS80;
	Bit32s mant32 = (test.upper >> 8) & 0x7fffff;
	Bit32s sign = (test.begin &0x8000)?1:0;
	if(exp32 == 0x4000) exp32 = 0x80;
	else if(exp32 > BIAS32) {
		exp32 = 0x80;
		mant32 = 0;
		sign = 0;
	} else if(exp32 < -BIAS32) {
		exp32 = -BIAS32;
		mant32 = 0;
		sign = 0;
	}

	exp32 = (exp32 + BIAS32) & 0xff;
	FPU_Reg result;
	result.l = (sign <<31)|(exp32 << 23)| mant32;
	return result.d;
}

static void FPU_ST80(PhysPt addr, Bitu reg) {
	struct{
		Bit16s begin;
		Bit32s upper;
	} test;
	Bit32s sign80 = (fpu.regs[reg].l&0x80000000)?1:0;
	Bit32s exp80 =  (fpu.regs[reg].l&0x7f800000)>>23;
	if(exp80 == 0xff)  exp80 = 0x7fff;
	else exp80 += (BIAS80 - BIAS32);
	Bit32s mant80 = (fpu.regs[reg].l&0x7fffff)<<8;
	if(fpu.regs[reg].d != 0) mant80 |= 0x80000000;
	test.begin = (static_cast<Bit16s>(sign80)<<15)| static_cast<Bit16s>(exp80);
	test.upper = mant80;
	mem_writed(addr,0);
	mem_writed(addr+4,test.upper);
	mem_writew(addr+8,test.begin);
}

static void FPU_FLD_F64(PhysPt addr,Bitu store_to) {
	double_reg reg;
	reg.l.lower = mem_readd(addr);
	reg.l.upper = mem_readd(addr+4);
	fpu.regs[store_to].d = static_cast<Real32>(reg.d);
}

static void FPU_FST_F64(PhysPt addr) {
	double_reg reg;
	reg.d = static_cast<Real64>(fpu.regs[TOP].d);
	mem_writed(addr,reg.l.lower);
	mem_writed(addr+4,reg.l.upper);
}

static void FPU_FST_I64(PhysPt addr) {
	double_reg blah;
	blah.ll = static_cast<Bit64s>(FROUND(fpu.regs[TOP].d));
	mem_writed(addr,blah.l.lower);
	mem_writed(addr+4,blah.l.upper);
}

static void FPU_FLD_I64(PhysPt addr,Bitu store_to) {
	double_reg blah;
	blah.l.lower = mem_readd(addr);
	blah.l.upper = mem_readd(addr+4);
	fpu.regs[store_to].d = static_cast<Real32>(blah.ll);
}

static void FPU_FSIN(void){
	__asm__ volatile (
		"lv.s    S000, %0\n"
       		"vcst.s  S001, VFPU_2_PI\n"
        	"vmul.s  S000, S000, S001\n"
	        "vsin.s  S000, S000\n"
		"sv.s    S000, %0\n"
	: "+m"(fpu.regs[TOP].d) : : );
//	fpu.regs[TOP].d = sinf(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	Real32 temp;
	__asm__ volatile (
		"lv.s	  S002, %0\n"
		"vcst.s   S003, VFPU_2_PI\n"
		"vmul.s   S002, S002, S003\n"
		"vrot.p   C000, S002, [s, c]\n"
		"sv.s	  S000, %0\n"
		"mfv      %1, S001\n"
	: "+m"(fpu.regs[TOP].d), "=r"(temp) : );
	FPU_PUSHF(temp);
/*	Real32 temp = fpu.regs[TOP].d;
	fpu.regs[TOP].d = sinf(temp);
	FPU_PUSH(cosf(temp));*/
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FCOS(void){
	__asm__ volatile (
		"lv.s    S000, %0\n"
	        "vcst.s  S001, VFPU_2_PI\n"
        	"vmul.s  S000, S000, S001\n"
	        "vcos.s  S000, S000\n"
        	"sv.s    S000, %0\n"
	: "+m"(fpu.regs[TOP].d) : );
//	fpu.regs[TOP].d = cosf(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	fpu.regs[TOP].d = sqrtf(fpu.regs[TOP].d);
	//flags and such :)
	return;
}

static Bit32u patan2f(Bit32u x, Bit32u y) {
	Bit32u r;
	// result = asinf(x/sqrt(x*x+1))
	__asm__ volatile (
		"mtv	  %2, S001\n"
		"mtv      %1, S000\n"
		"vrcp.s   S001, S001\n"
		"vmul.s   S000, S000, S001\n"
		"vmul.s   S001, S000, S000\n"
		"vadd.s   S001, S001, S001[1]\n"
		"vrsq.s   S001, S001\n"
		"vmul.s   S000, S000, S001\n"
		"vasin.s  S000, S000\n"
		"vcst.s   S001, VFPU_PI_2\n"
		"vmul.s   S000, S000, S001\n"
		"mfv      %0, S000\n"
	: "=r"(r) : "r"(x), "r"(y));
	return r;
}

static void FPU_FPATAN(void){
	FPU_Reg r;
	FPU_Reg y = fpu.regs[STV(1)], x = fpu.regs[TOP];
	if (fabsf(x.d) >= fabsf(y.d)) {
		r.l = patan2f(y.l,x.l);
		if   (x.d < 0.0f) r.d += (y.d>=0.0f ? PI : -PI);
	} else {
		r.l = patan2f(x.l,y.l);
		r.d = -r.d;
		r.d += (y.d < 0.0f ? -PI_2 : PI_2);
	}
	fpu.regs[STV(1)] = r;
//	fpu.regs[STV(1)].d = atan2f(fpu.regs[STV(1)].d,fpu.regs[TOP].d);
	FPU_FPOP();
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	__asm__ volatile (
		"lv.s    S000, %0\n"
		"vcst.s  S001, VFPU_2_PI\n"
	        "vmul.s  S000, S000, S001\n"
        	"vrot.p  C002, S000, [s, c]\n"
	        "vdiv.s  S000, S002, S003\n"
        	"sv.s    S000, %0\n"
	: "+m"(fpu.regs[TOP].d) : );
//	fpu.regs[TOP].d = tanf(fpu.regs[TOP].d);
	FPU_PUSHF(1.0f);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FRNDINT(void){
	fpu.regs[TOP].d = FROUND(fpu.regs[TOP].d);
}

static void FPU_FPREM(void){
	Bit32s ressaved;
	__asm__ volatile (
		"lv.s      S001, %2\n"
		"lv.s      S000, %0\n"
		"vrcp.s    S002, S001\n"
		"vmul.s    S003, S000, S002\n"
		"vf2iz.s   S002, S003, 0\n"
		"vi2f.s    S003, S002, 0\n"
		"mfv	   %1, S002\n"
		"vmul.s    S003, S003, S001\n"
		"vsub.s    S000, S000, S003\n"
		"sv.s      S000, %0\n"
	: "+m"(fpu.regs[TOP].d), "=r"(ressaved) : "m"(fpu.regs[STV(1)].d));
/*	Real32 valtop = fpu.regs[TOP].d;
	Real32 valdiv = fpu.regs[STV(1)].d;
	Bit32s ressaved = static_cast<Bit32s>( (valtop/valdiv) );
// Some backups
//	Real64 res=valtop - ressaved*valdiv; 
//      res= fmod(valtop,valdiv);
	fpu.regs[TOP].d = valtop - ressaved*valdiv;*/
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FPREM1(void){
	Bit32s ressaved;
	__asm__ volatile (
		"lv.s      S001, %2\n"
		"lv.s      S000, %0\n"
		"vrcp.s    S002, S001\n"
		"vmul.s    S003, S000, S002\n"
		"vf2in.s   S002, S003, 0\n"
		"vi2f.s    S003, S002, 0\n"
		"mfv	   %1, S002\n"
		"vmul.s    S003, S003, S001\n"
		"vsub.s    S000, S000, S003\n"
		"sv.s      S000, %0\n"
	: "+m"(fpu.regs[TOP].d), "=r"(ressaved) : "m"(fpu.regs[STV(1)].d));
/*	Real32 valtop = fpu.regs[TOP].d;
	Real32 valdiv = fpu.regs[STV(1)].d;
	float quot = valtop/valdiv;
	float quotf = floorf(quot);
	Bit32s ressaved;
	if (quot-quotf>0.5) ressaved = static_cast<Bit32s>(quotf+1);
	else if (quot-quotf<0.5) ressaved = static_cast<Bit32s>(quotf);
	else ressaved = static_cast<Bit64s>((((static_cast<Bit32s>(quotf))&1)!=0)?(quotf+1):(quotf));
	fpu.regs[TOP].d = valtop - ressaved*valdiv;*/
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FXAM(void){
	if(fpu.regs[TOP].l & 0x80000000)	//sign
	{ 
		FPU_SET_C1(1);
	} 
	else 
	{
		FPU_SET_C1(0);
	}
	if(fpu.tags[TOP] == TAG_Empty)
	{
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
	if(fpu.regs[TOP].d == 0.0f)		//zero or normalized number.
	{ 
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);
	}
	else
	{
		FPU_SET_C3(0);FPU_SET_C2(1);FPU_SET_C0(0);
	}
}


static void FPU_F2XM1(void){
	__asm__ volatile (
		"lv.s     S000, %0\n"
		"vexp2.s  S000, S000\n"
		"vsub.s   S000, S000, S000[1]\n"
		"sv.s     S000, %0\n"
	: "+m"(fpu.regs[TOP].d) : );
//	fpu.regs[TOP].d = powf(2.0,fpu.regs[TOP].d) - 1;
	return;
}

static void FPU_FYL2X(void){
	__asm__ volatile (
	        "lv.s    S000, %1\n"
		"lv.s    S001, %0\n"
        	"vlog2.s S000, S000\n"
	        "vmul.s  S000, S000, S001\n"
        	"sv.s    S000, %0\n"
	: "+m"(fpu.regs[STV(1)].d) : "m"(fpu.regs[TOP].d));
//	fpu.regs[STV(1)].d*=logf(fpu.regs[TOP].d)/LN2;
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void){
	__asm__ volatile (
	        "lv.s    S000, %1\n"
		"vadd.s  S000, S000, S000[1]\n"
		"lv.s    S001, %0\n"
        	"vlog2.s S000, S000\n"
	        "vmul.s  S000, S000, S001\n"
        	"sv.s    S000, %0\n"
	: "+m"(fpu.regs[STV(1)].d) : "m"(fpu.regs[TOP].d));
//	fpu.regs[STV(1)].d*=logf(fpu.regs[TOP].d+1.0)/LN2;
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	__asm__ volatile (
		"lv.s    S000, %1\n"
		"vf2iz.s S000, S000, 0\n"
		"vi2f.s  S000, S000, 0\n"
		"vexp2.s S000, S000\n"
		"lv.s    S001, %0\n"
		"vmul.s  S001, S001, S000\n"
		"sv.s    S001, %0\n"
	: "+m"(fpu.regs[TOP].d) : "m"(fpu.regs[STV(1)].d));
//	fpu.regs[TOP].d *= powf(2.0,static_cast<Real32>(static_cast<Bit32s>(fpu.regs[STV(1)].d)));
	return; //2^x where x is chopped.
}

static void FPU_FXTRACT(void) {
	// function stores real bias in st and 
	// pushes the significant number onto the stack
	// if double ever uses a different base please correct this function

	FPU_Reg test = fpu.regs[TOP];
	Bit32s exp80 =  test.l&0x7f800000;
	Bit32s exp80final = (exp80>>23) - BIAS32;
	Real32 mant;
	__asm__ volatile (
		"mtv	 %1, S000\n"
		"vi2f.s  S000, S000, 0\n"
		"vrexp2.s S000, S000\n"
		"mtv	 %2, S001\n"
		"vmul.s  S001, S001, S000\n"
		"mfv     %0, S001\n"
	: "=r"(mant) : "r"(exp80final), "r"(test.d));
//	Real32 mant = test.d / (powf(2.0,static_cast<Real32>(exp80final)));
	fpu.regs[TOP].d = static_cast<Real32>(exp80final);
	FPU_PUSHF(mant); 
}

static void FPU_FSTENV(PhysPt addr){
	FPU_SET_TOP(TOP);
	if(!cpu.code.big) {
		mem_writew(addr+0,static_cast<Bit16u>(fpu.cw));
		mem_writew(addr+2,static_cast<Bit16u>(fpu.sw));
		mem_writew(addr+4,static_cast<Bit16u>(FPU_GetTag()));
	} else { 
		mem_writed(addr+0,static_cast<Bit32u>(fpu.cw));
		mem_writed(addr+4,static_cast<Bit32u>(fpu.sw));
		mem_writed(addr+8,static_cast<Bit32u>(FPU_GetTag()));
	}
}

static void FPU_FLDENV(PhysPt addr){
	Bit16u tag;
	Bit32u tagbig;
	Bitu cw;
	if(!cpu.code.big) {
		cw     = mem_readw(addr+0);
		fpu.sw = mem_readw(addr+2);
		tag    = mem_readw(addr+4);
	} else { 
		cw     = mem_readd(addr+0);
		fpu.sw = (Bit16u)mem_readd(addr+4);
		tagbig = mem_readd(addr+8);
		tag    = static_cast<Bit16u>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i));
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
	FPU_FLDENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		fpu.regs[STV(i)].d = FPU_FLD_F80(addr+start);
		start += 10;
	}
}

static void FPU_FCHS(void){
	fpu.regs[TOP].d = -1.0*(fpu.regs[TOP].d);
}

static void FPU_FABS(void){
	fpu.regs[TOP].d = fabsf(fpu.regs[TOP].d);
}

static void FPU_FTST(void){
	fpu.regs[8].d = 0.0;
	FPU_FCOM(TOP,8);
}

static void FPU_FLD1(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 1.0;
}

static void FPU_FLDL2T(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2T;
}

static void FPU_FLDL2E(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2E;
}

static void FPU_FLDPI(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = PI;
}

static void FPU_FLDLG2(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LG2;
}

static void FPU_FLDLN2(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LN2;
}

static void FPU_FLDZ(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 0.0;
	fpu.tags[TOP] = TAG_Zero;
}

static INLINE void FPU_FLD_F32_EA(PhysPt addr) {
	FPU_FLD_F32(addr,8);
}
static INLINE void FPU_FLD_F64_EA(PhysPt addr) {
	FPU_FLD_F64(addr,8);
}
static INLINE void FPU_FLD_I32_EA(PhysPt addr) {
	FPU_FLD_I32(addr,8);
}
static INLINE void FPU_FLD_I16_EA(PhysPt addr) {
	FPU_FLD_I16(addr,8);
}

static void FPU_FLD_F80(PhysPt addr) {
	fpu.regs[TOP].d = FPU_FLD_F80(addr);
}

static void FPU_FST_F80(PhysPt addr) {
	FPU_ST80(addr,TOP);
}

static INLINE void FPU_FADD_EA(Bitu op1){
	FPU_FADD(op1,8);
}
static INLINE void FPU_FMUL_EA(Bitu op1){
	FPU_FMUL(op1,8);
}
static INLINE void FPU_FSUB_EA(Bitu op1){
	FPU_FSUB(op1,8);
}
static INLINE void FPU_FSUBR_EA(Bitu op1){
	FPU_FSUBR(op1,8);
}
static INLINE void FPU_FDIV_EA(Bitu op1){
	FPU_FDIV(op1,8);
}
static INLINE void FPU_FDIVR_EA(Bitu op1){
	FPU_FDIVR(op1,8);
}
static INLINE void FPU_FCOM_EA(Bitu op1){
	FPU_FCOM(op1,8);
}

static void FPU_FST_I16(PhysPt addr) {
	mem_writew(addr,static_cast<Bit16s>(FROUND(fpu.regs[TOP].d)));
}

static void FPU_FST_I32(PhysPt addr) {
	mem_writed(addr,static_cast<Bit32s>(FROUND(fpu.regs[TOP].d)));
}
