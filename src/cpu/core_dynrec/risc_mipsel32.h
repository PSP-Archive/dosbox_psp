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


/* MIPS32 (little endian) backend by crazyc */


// some configuring defines that specify the capabilities of this architecture
// or aspects of the recompiling

// protect FC_ADDR over function calls if necessaray
// #define DRC_PROTECT_ADDR_REG

// try to use non-flags generating functions if possible
#define DRC_FLAGS_INVALIDATION
// try to replace _simple functions by code
#define DRC_FLAGS_INVALIDATION_DCODE

// type with the same size as a pointer
#define DRC_PTR_SIZE_IM Bit32u

// calling convention modifier
#define DRC_CALL_CONV	/* nothing */
#define DRC_FC		/* nothing */

// register mapping
typedef Bit8u HostReg;

#define HOST_v0 2
#define HOST_v1 3
#define HOST_a0 4
#define HOST_a1 5
#define HOST_t4 12
#define HOST_t5 13
#define HOST_t6 14
#define HOST_t7 15
#define HOST_s0 16
#define HOST_s1 17
#define HOST_s2 18
#define HOST_s3 19
#define HOST_s4 20
#define HOST_s5 21
#define HOST_s6 22
#define HOST_s7 23
#define HOST_t8 24
#define HOST_t9 25
#define HOST_fp 30
#define temp1 HOST_v1
#define temp2 HOST_t9 

// register that holds function return values
#define FC_RETOP HOST_v0

// register used for address calculations,
#define FC_ADDR HOST_fp			// has to be saved across calls, see DRC_PROTECT_ADDR_REG

// register that holds the first parameter
#define FC_OP1 HOST_a0

// register that holds the second parameter
#define FC_OP2 HOST_a1

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_t5

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_t6

// temporary register for LEA
#define TEMP_REG_DRC HOST_t7

#define FC_CACHE_EAX HOST_s0	// In this code, these are assumed to be contiguous
#define FC_CACHE_ECX HOST_s1
#define FC_CACHE_EDX HOST_s2
#define FC_CACHE_EBX HOST_s3
#define FC_CACHE_ESP HOST_s4
#define FC_CACHE_EBP HOST_s5
#define FC_CACHE_ESI HOST_s6
#define FC_CACHE_EDI HOST_s7

// this is dangerous, the compiler may mess it up, hopefully volatile will cause it to be careful
#define DRC_SAVE_CREG(creg) { Bit32u *addr = (Bit32u *)((Bit32u)&cpu_regs+((creg-FC_CACHE_EAX)*4)); \
			__asm__ volatile ("sw $%1, 0(%0)\n"::"r"(addr),"i"(creg)); }

// save some state to improve code gen
static bool temp1_valid = false;
static Bit32u temp1_value;
static Bit32u epilog_jump;
static Bit8u *branch_ptr = NULL;

// move a full register from reg_src to reg_dst
static bool gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	if(reg_src == reg_dst) return false; 
	cache_addw((reg_dst<<11)+0x21);      // addu reg_dst, $0, reg_src 
	cache_addw(reg_src); 
	return true;
}

// move a 32bit constant value into dest_reg
static Bit32u gen_mov_dword_to_reg_imm(HostReg dest_reg,Bit32u imm) {
	Bit32u ret;
	if(imm < 65536) 
		ret = ((0x3400+dest_reg)<<16)+(Bit16u)imm;	// ori dest_reg, $0, imm
	else if(((Bit32s)imm < 0) && ((Bit32s)imm >= -32768)) 
		ret = ((0x2400+dest_reg)<<16)+(Bit16u)imm;	// addiu dest_reg, $0, imm
	else if(!(imm & 0xffff)) 
		ret = ((0x3c00+dest_reg)<<16)+(Bit16u)(imm>>16);	// lui dest_reg, %hi(imm)
	else {
		cache_addw((Bit16u)(imm >> 16));	// lui dest_reg, %hi(imm)
		cache_addw(0x3c00+dest_reg);
		ret = ((0x3400+(dest_reg<<5)+dest_reg)<<16)+(Bit16u)imm;	// ori dest_reg, dest_reg, %lo(imm)
	}
	cache_addd(ret);
	return ret;
}

// this is the only place temp1 should be modified
static void INLINE mov_imm_to_temp1(Bit32u imm) {
	if (temp1_valid && (temp1_value == imm)) return;
	gen_mov_dword_to_reg_imm(temp1, imm);
	temp1_valid = true;
	temp1_value = imm;
}

static Bit16s gen_addr_temp1(Bit32u addr) {
	Bit32u hihalf = addr & 0xffff0000;
	Bit16s lohalf = addr & 0xffff;
	if (lohalf > 32764) {  // [l,s]wl will overflow
		hihalf = addr;
		lohalf = 0;
	} else if(lohalf < 0) hihalf += 0x10000;
	mov_imm_to_temp1(hihalf);
	return lohalf;
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static Bit32u gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	Bit16u lohalf = gen_addr_temp1((Bit32u)data);
	Bit32u ret;
	// alignment....
	if (dword) {
		if ((Bit32u)data & 3) {
			cache_addw(lohalf+3);		// lwl dest_reg, 3(temp1)
			cache_addw(0x8800+(temp1<<5)+dest_reg);
			ret = ((0x9800+(temp1<<5)+dest_reg)<<16)+lohalf;	// lwr dest_reg, 0(temp1)
		} else  ret = ((0x8C00+(temp1<<5)+dest_reg)<<16)+lohalf;	// lw dest_reg, 0(temp1)
	} else {
		if ((Bit32u)data & 1) {
			cache_addw(lohalf);		// lbu dest_reg, 0(temp1)
			cache_addw(0x9000+(temp1<<5)+dest_reg);
			cache_addw(lohalf+1);		// lbu temp2, 1(temp1)
			cache_addw(0x9000+(temp1<<5)+temp2);
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
			ret = ((0x7c00+(temp2<<5)+dest_reg)<<16)+0x7a04;	// ins dest_reg, temp2, 8, 8
#else
			cache_addw((temp2<<11)+0x200);		// sll temp2, temp2, 8
			cache_addw(temp2);
			ret = (((temp2<<5)+dest_reg)<<16)+((dest_reg<<11)+0x25);	// or dest_reg, temp2, dest_reg
#endif
		} else ret = ((0x9400+(temp1<<5)+dest_reg)<<16)+lohalf;	// lhu dest_reg, 0(temp1);
	}
	cache_addd(ret);
	return ret;
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg,Bit16u imm) {
	cache_addw(imm);			// ori dest_reg, $0, imm
	cache_addw(0x3400+dest_reg);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	Bit16s lohalf = gen_addr_temp1((Bit32u)dest);
	// alignment....
	if (dword) {
		if ((Bit32u)dest & 3) {
			cache_addw(lohalf+3);		// swl src_reg, 3(temp1)
			cache_addw(0xA800+(temp1<<5)+src_reg);
			cache_addw(lohalf);		// swr src_reg, 0(temp1)
			cache_addw(0xB800+(temp1<<5)+src_reg);
		} else {
			cache_addw(lohalf);		// sw src_reg, 0(temp1)
			cache_addw(0xAC00+(temp1<<5)+src_reg);
		}
	} else {
		if((Bit32u)dest & 1) {
			cache_addw(lohalf);		// sb src_reg, 0(temp1)
			cache_addw(0xA000+(temp1<<5)+src_reg);
			cache_addw((temp2<<11)+0x202);		// srl temp2, src_reg, 8
			cache_addw(src_reg);
			cache_addw(lohalf+1);		// sb temp2, 1(temp1)
			cache_addw(0xA000+(temp1<<5)+temp2);
		} else {
			cache_addw(lohalf);		// sh src_reg, 0(temp1);
			cache_addw(0xA400+(temp1<<5)+src_reg);
		}
	}
}

static void INLINE gen_set_creg_word(HostReg src_reg, Bitu dest_reg, bool dword) {
	//gen_mov_word_from_reg(src_reg,DRCD_REG_WORD(dest_reg,dword),dword);
	//return;
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
	cache_addw(((dword?31:15)<<11)+4);		// ins dest_reg+FC_CACHE_EAX, src_reg, 0, dword?31:15
	cache_addw(0x7c00+(src_reg<<5)+(dest_reg+FC_CACHE_EAX));
#else
#error "gen_set_creg_word" // (dword)?gen_mov_regs(dest_reg+FC_CACHE_EAX,src_reg):"bleh"
#endif
}

static Bit32u INLINE gen_get_creg_word(HostReg dest_reg, Bitu src_reg, bool dword) {
	Bit32u ret;
	//return gen_mov_word_to_reg(dest_reg,DRCD_REG_WORD(src_reg,dword),dword);
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
	ret = ((0x7c00+(src_reg+FC_CACHE_EAX<<5)+(dest_reg))<<16)+((dword?31:15)<<11);	// ext dest_reg, src_reg+FC_CACHE_EAX, 0, dword?31:15
	cache_addd(ret);
#else
#error "gen_get_creg_word" // (dword)?gen_mov_regs(dest_reg,src_reg+FC_CACHE_EAX):"bleh"
#endif
	return ret;
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static Bit32u gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	Bit16u lohalf = gen_addr_temp1((Bit32u)data);
	Bit32u ret = ((0x9000+(temp1<<5)+dest_reg)<<16)+lohalf;	// lbu dest_reg, 0(temp1)
	cache_addd(ret);
	return ret;
}

static Bit32u gen_mov_byte_creg_to_reg_low(HostReg dest_reg,Bitu creg,Bitu idx) {
	Bit32u ret;
	//return gen_mov_byte_to_reg_low(dest_reg,DRCD_REG_BYTE(creg,idx));
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
	idx *= 8;
	ret = ((0x7c00+(creg+FC_CACHE_EAX<<5)+(dest_reg))<<16)+((idx+7)<<11)+(idx<<6);	// ext dest_reg, creg+FC_CACHE_EAX, idx*8+7, idx*8
	cache_addd(ret);
#else
#error "gen_mov_byte_creg_to_reg_low" // gen_mov_regs(dest_reg,creg+FC_CACHE_EAX);sll dest_reg, dest_reg, idx*8
#endif
	return ret;
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	gen_mov_byte_to_reg_low(dest_reg, data);
}

static void INLINE gen_mov_byte_creg_to_reg_low_canuseword(HostReg dest_reg,Bitu creg,Bitu idx) {
	gen_mov_byte_creg_to_reg_low(dest_reg,creg,idx);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_imm(HostReg dest_reg,Bit8u imm) {
	gen_mov_word_to_reg_imm(dest_reg, imm);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,Bit8u imm) {
	gen_mov_byte_to_reg_low_imm(dest_reg, imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	Bit16s lohalf = gen_addr_temp1((Bit32u)dest);
	cache_addw(lohalf);			// sb src_reg, 0(temp1)
	cache_addw(0xA000+(temp1<<5)+src_reg);
}

static void INLINE gen_mov_byte_creg_from_reg_low(HostReg src_reg,Bitu creg,Bitu idx) {
	//gen_mov_byte_from_reg_low(src_reg,DRCD_REG_BYTE(creg,idx));
	//return;
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
	idx *= 8;
	cache_addw(((idx+7)<<11)+(idx<<6)+4);			// ins creg+FC_CACHE_EAX, src_reg, idx*8+7, idx*8 
	cache_addw(0x7c00+(src_reg<<5)+(creg+FC_CACHE_EAX));
#else
#error "gen_mov_byte_creg_to_reg_low"
#endif	
}

// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	if (sign) {
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
		cache_addw((reg<<11)+0x420);	// seb reg, reg
		cache_addw(0x7c00+reg);
#else
#error "your arch lacks seb"
#endif
	} else {
		cache_addw(0xff);		// andi reg, reg, 0xff
		cache_addw(0x3000+(reg<<5)+reg);
	}
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	if (sign) {
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
		cache_addw((reg<<11)+0x620);	// seh reg, reg
		cache_addw(0x7c00+reg);
#else
#error "your arch lacks seh"
#endif
	} else {
		cache_addw(0xffff);		// andi reg, reg, 0xffff
		cache_addw(0x3000+(reg<<5)+reg);
	}
}

// used for loads and creg moves that depending on arch may need to be zero extended
static void gen_zextend_byte(HostReg reg) {
	// not needed on mips as loads and creg moves are zero extended by default
}

static void gen_zextend_word(HostReg reg) {
	// not needed on mips as loads and creg moves are zero extended by default
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	gen_mov_word_to_reg(temp2, op, 1);
	cache_addw((reg<<11)+0x21);		// addu reg, reg, temp2 
	cache_addw((reg<<5)+temp2);
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,Bit32u imm) {
	if(!imm) return;
	if(((Bit32s)imm >= -32768) && ((Bit32s)imm < 32768)) {
		cache_addw((Bit16u)imm);	// addiu reg, reg, imm
		cache_addw(0x2400+(reg<<5)+reg);
	} else {
		mov_imm_to_temp1(imm);
		cache_addw((reg<<11)+0x21);	// addu reg, reg, temp1
		cache_addw((reg<<5)+temp1);
	}
}

static void gen_add_creg(HostReg reg,Bitu creg) {
	//gen_add(reg, DRCD_REG(creg));
	//return;
	cache_addw((reg<<11)+0x21);		// addu reg, reg, creg 
	cache_addw((reg<<5)+creg+FC_CACHE_EAX);
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,Bit32u imm) {
	if(imm < 65536) { 
		cache_addw((Bit16u)imm);      // andi reg, reg, imm 
		cache_addw(0x3000+(reg<<5)+reg); 
	} else { 
		mov_imm_to_temp1((Bit32u)imm); 
		cache_addw((reg<<11)+0x24);      // and reg, temp1, reg 
		cache_addw((temp1<<5)+reg); 
	} 
}


// move a 32bit constant value into memory
static void INLINE gen_mov_direct_dword(void* dest,Bit32u imm) {
	gen_mov_dword_to_reg_imm(temp2, imm);
	gen_mov_word_from_reg(temp2, dest, 1);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_direct_dword(dest,(Bit32u)imm);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void INLINE gen_add_direct_word(void* dest,Bit32u imm,bool dword) {
	if(!imm) return;
	gen_mov_word_to_reg(temp2, dest, dword);
	gen_add_imm(temp2, imm);
	gen_mov_word_from_reg(temp2, dest, dword);
}

static void gen_add_creg_imm(Bitu creg, Bit32u imm) {
	//gen_add_direct_word(DRCD_REG(creg),imm,1);
	//return;
	gen_add_imm(creg+FC_CACHE_EAX, imm);

}
// add an 8bit constant value to a dword memory value
static void INLINE gen_add_direct_byte(void* dest,Bit8s imm) {
	gen_add_direct_word(dest, (Bit32s)imm, 1);
}

// subtract an 8bit constant value from a dword memory value
static void INLINE gen_sub_direct_byte(void* dest,Bit8s imm) {
	gen_add_direct_word(dest, -((Bit32s)imm), 1);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void INLINE gen_sub_direct_word(void* dest,Bit32u imm,bool dword) {
	gen_add_direct_word(dest, -(Bit32s)imm, dword);
}

// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_addw((scale_reg<<11)+(scale<<6));		// sll scale_reg, scale_reg, scale
		cache_addw(scale_reg);
	}
	cache_addw((dest_reg<<11)+0x21);			// addu dest_reg, dest_reg, scale_reg
	cache_addw((dest_reg<<5)+scale_reg);
	gen_add_imm(dest_reg, imm);
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_addw((dest_reg<<11)+(scale<<6));		// sll dest_reg, dest_reg, scale
		cache_addw(dest_reg);
	}
	gen_add_imm(dest_reg, imm);
}

static INLINE void gen_mov_ll_ret_to_reg_creg(HostReg low, Bitu high) {
	gen_mov_regs(low,HOST_v0);
	//gen_mov_regs(HOST_t5, HOST_v1);
	//gen_set_creg_word(HOST_t5,high, true);
	//return;
	gen_mov_regs(high+FC_CACHE_EAX,HOST_v1);
}

#define DELAY cache_addd(0)			// nop

// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void * func) {
#if C_DEBUG
	if ((cache.pos ^ func) & 0xf0000000) LOG_MSG("jump overflow\n");
#endif
	temp1_valid = false;
	if(branch_ptr) {
		*(Bit32u *)branch_ptr = 0x0c000000+(((Bit32u)func>>2)&0x3ffffff);
		branch_ptr = NULL;
	} else {
		cache_addd(0x0c000000+(((Bit32u)func>>2)&0x3ffffff));		// jal func
		DELAY;
	}
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static Bit32u INLINE gen_call_function_setup(void * func,Bitu paramcount,bool fastcall=false) {
	Bit32u proc_addr = (Bit32u)(branch_ptr?branch_ptr:cache.pos);
	gen_call_function_raw(func);
	return proc_addr;
}

static Bit32u INLINE gen_get_cpos() {
	return (Bit32u)(branch_ptr?branch_ptr:cache.pos);
}

#ifdef __mips_eabi
// max of 8 parameters in $a0-$a3 and $t0-$t3

// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param,bool last_param=false) {
	Bit32u delay_op = gen_mov_dword_to_reg_imm(param+4, imm);
	if(last_param) {
		branch_ptr = cache.pos-4;
		cache_addd(delay_op);
	}
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(Bitu addr,Bitu param,bool last_param=false) {
	gen_load_param_imm(addr, param, last_param);
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param,bool last_param=false) {
	if(last_param) {
		branch_ptr=cache.pos;
		DELAY;		// placeholder
	}
	if(!gen_mov_regs(param+4, reg) && last_param) DELAY;
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param,bool last_param=false,bool dword=true) {
	Bit32u delay_op = gen_mov_word_to_reg(param+4, (void *)mem, dword);
	if(last_param) {
		branch_ptr = cache.pos-4;
		cache_addd(delay_op);
	}
}

static void INLINE gen_load_param_mem_byte(Bitu mem,Bitu param,bool last_param=false) {
	Bit32u delay_op = gen_mov_byte_to_reg_low(param+4, (void *)mem);
	if(last_param) {
		branch_ptr = cache.pos-4;
		cache_addd(delay_op);
	}
}

static void INLINE gen_load_param_creg_byte(Bitu creg,Bitu idx,Bitu param,bool last_param=false) {
	Bit32u delay_op = gen_mov_byte_creg_to_reg_low(param+4,creg,idx);
	if(last_param) {
		branch_ptr = cache.pos-4;
		cache_addd(delay_op);
	}
}

static void INLINE gen_load_param_creg(Bitu creg,Bitu param,bool last_param=false,bool dword=true) {
	Bit32u delay_op = gen_get_creg_word(param+4,creg,dword);
	if(last_param) {
		branch_ptr = cache.pos-4;
		cache_addd(delay_op);
	}
}

#else
#error "add your mips abi"
#endif

// jump to an address pointed at by ptr, offset is in imm
static void INLINE gen_jmp_ptr(void * ptr,Bits imm=0) {
	gen_mov_word_to_reg(temp2, ptr, 1);
	if((imm < -32768) || (imm >= 32768)) {
		gen_add_imm(temp2, imm);
		imm = 0;
	}
	temp1_valid = false;
	cache_addw((Bit16u)imm);	// lw temp2, imm(temp2)
	cache_addw(0x8C00+(temp2<<5)+temp2);
	cache_addd((temp2<<21)+8); 	// jr temp2 
	DELAY;
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static Bit32u INLINE gen_create_branch_on_zero(HostReg reg,bool dword) {
	temp1_valid = false;
	if(!dword) { 
		cache_addw(0xffff);	// andi temp1, reg, 0xffff
		cache_addw(0x3000+(reg<<5)+temp1);
	}
	cache_addw(0);			// beq $0, reg, 0
	cache_addw(0x1000+(dword?reg:temp1));
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static Bit32u INLINE gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	temp1_valid = false;
	if(!dword) { 
		cache_addw(0xffff);	// andi temp1, reg, 0xffff
		cache_addw(0x3000+(reg<<5)+temp1);
	}
	cache_addw(0);			// bne $0, reg, 0
	cache_addw(0x1400+(dword?reg:temp1));
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// calculate relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(Bit32u)cache.pos-data;
	if (len<0) len=-len;
	if (len>126) LOG_MSG("Big jump %d",len);
#endif
	temp1_valid = false;			// this is a branch target
	*(Bit16u*)data=((Bit16u)((Bit32u)cache.pos-data-4)>>2);
}

#if 0	// assume for the moment no branch will go farther then +/- 128KB

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit32u gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	temp1_valid = false;
	if (!isdword) {
		cache_addw(0xff);	// andi temp1, reg, 0xff
		cache_addw(0x3000+(reg<<5)+temp1);
	}
	cache_addw(3);			// beq $0, reg, +12
	cache_addw(0x1000+(isdword?reg:temp1));	
	DELAY;
	cache_addd(0x00000000);		// fill j
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit32u INLINE gen_create_branch_long_leqzero(HostReg reg) {
	temp1_valid = false;
	cache_addw(3);				// bgtz reg, +12
	cache_addw(0x1c00+(reg<<5));
	DELAY;
	cache_addd(0x00000000);			// fill j 
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(Bit32u data) {
	temp1_valid = false;
	// this is an absolute branch
	*(Bit32u*)data=0x08000000+(((Bit32u)cache.pos>>2)&0x3ffffff);
}
#else		
// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit32u gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	temp1_valid = false;
	if (!isdword) {
		cache_addw(0xff);	// andi temp1, reg, 0xff
		cache_addw(0x3000+(reg<<5)+temp1);
	}
	cache_addw(0);			// bne $0, reg, 0
	cache_addw(0x1400+(isdword?reg:temp1));	
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit32u INLINE gen_create_branch_long_leqzero(HostReg reg) {
	temp1_valid = false;
	cache_addw(0);			// blez reg, 0
	cache_addw(0x1800+(reg<<5));
	DELAY;
	return ((Bit32u)cache.pos-8);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(Bit32u data) {
	gen_fill_branch(data);
}
#endif

static void gen_save_creg(Bitu creg) {
	//return;
	Bit16u lohalf = gen_addr_temp1((Bit32u)&cpu_regs+(creg*4));
	cache_addw(lohalf);		// sw creg+FC_CACHE_EAX, lohalf(temp1)
	cache_addw(0xAC00+(temp1<<5)+creg+FC_CACHE_EAX);
}

static void gen_reload_creg(Bitu creg) {
	//return;
	Bit16u lohalf = gen_addr_temp1((Bit32u)&cpu_regs+(creg*4));
	cache_addw(lohalf);		// lw creg+FC_CACHE_EAX, lohalf(temp1)
	cache_addw(0x8C00+(temp1<<5)+creg+FC_CACHE_EAX);
}

static void gen_load_regs(void) {
	gen_mov_dword_to_reg_imm(HOST_v1,(Bit32u)&cpu_regs);
	cache_addd(0x8c700000);			// lw $s0, 0($v1)
	cache_addd(0x8c710004);			// lw $s1, 4($v1)
	cache_addd(0x8c720008);			// lw $s2, 8($v1)
	cache_addd(0x8c73000c);			// lw $s3, 12($v1)
	cache_addd(0x8c740010);			// lw $s4, 16($v1)
	cache_addd(0x8c750014);			// lw $s5, 20($v1)
	cache_addd(0x8c760018);			// lw $s6, 24($v1)
	cache_addd(0x8c77001c);			// lw $s7, 28($v1)
}

static void gen_store_regs(void) {
	gen_mov_dword_to_reg_imm(HOST_v1,(Bit32u)&cpu_regs);
	cache_addd(0xac700000);			// sw $s0, 0($v1)
	cache_addd(0xac710004);			// sw $s1, 4($v1)
	cache_addd(0xac720008);			// sw $s2, 8($v1)
	cache_addd(0xac73000c);			// sw $s3, 12($v1)
	cache_addd(0x34040000+BR_NoESP);	// ori $a0, $0, BR_NoESP
	cache_addd(0x54440001);			// bnel $v0, $a0, +4
	cache_addd(0xac740010);			// sw $s4, 16($v1)
	cache_addd(0xac750014);			// sw $s5, 20($v1)
	cache_addd(0xac760018);			// sw $s6, 24($v1)
	cache_addd(0xac77001c);			// sw $s7, 28($v1)
}

static void gen_run_code(void) {
	temp1_valid = false;
	cache_addd(0x27bdffd8);			// addiu $sp, $sp, -40
	cache_addd(0xafb00004);			// sw $s0, 4($sp)
	cache_addd(0xafb10008);			// sw $s1, 8($sp)
	cache_addd(0xafb2000c);			// sw $s2, 12($sp)
	cache_addd(0xafb30010);			// sw $s3, 16($sp)
	cache_addd(0xafb40014);			// sw $s4, 20($sp)
	cache_addd(0xafb50018);			// sw $s5, 24($sp)
	cache_addd(0xafb6001c);			// sw $s6, 28($sp)
	cache_addd(0xafb70020);			// sw $s7, 32($sp)
	cache_addd(0xafbe0024);			// sw $fp, 36($sp)
	gen_load_regs();
	cache_addd(0x00800008);			// jr $a0
	cache_addd(0xafbf0000);			// sw $ra, 0($sp)
	epilog_jump = 0x08000000+(((Bit32u)cache.pos>>2)&0x3ffffff);
	gen_store_regs();
	cache_addd(0x8fbf0000);			// lw $ra, 0($sp)
	cache_addd(0x8fb00004);			// lw $s0, 4($sp)
	cache_addd(0x8fb10008);			// lw $s1, 8($sp)
	cache_addd(0x8fb2000c);			// lw $s2, 12($sp)
	cache_addd(0x8fb30010);			// lw $s3, 16($sp)
	cache_addd(0x8fb40014);			// lw $s4, 20($sp)
	cache_addd(0x8fb50018);			// lw $s5, 24($sp)
	cache_addd(0x8fb6001c);			// lw $s6, 28($sp)
	cache_addd(0x8fb70020);			// lw $s7, 32($sp)
	cache_addd(0x8fbe0024);			// lw $fp, 36($sp)
	cache_addd(0x03e00008);			// jr $ra
	cache_addd(0x27bd0028);			// addiu $sp, $sp, 40
}

static void gen_return_function(Bit16u retcode, bool ret_exception) {
	temp1_valid = false;
	cache_addd(epilog_jump);
	if (!ret_exception) {
		gen_mov_word_to_reg_imm(FC_RETOP,retcode);
	} else DELAY;
}

#ifdef PSP
// called when a call to a function can be replaced by a
// call to a simpler function
static INLINE Bit32u gen_fill_function_ptr(void* fct_ptr,Bitu flags_type) {
	//Bit64u ret;
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			return 0x00851021;					// addu $v0, $a0, $a1
		case t_ORb:
		case t_ORw:
		case t_ORd:
			return 0x00851025;					// or $v0, $a0, $a1
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			return 0x00851024;					// and $v0, $a0, $a1
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			return 0x00851023;					// subu $v0, $a0, $a1
		case t_XORb:
		case t_XORw:
		case t_XORd:
			return 0x00851026;					// xor $v0, $a0, $a1
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			return 0;							// nop
		case t_INCb:
		case t_INCw:
		case t_INCd:
			return 0x24820001;					// addiu $v0, $a0, 1
		case t_DECb:
		case t_DECw:
		case t_DECd:
			return 0x2482ffff;					// addiu $v0, $a0, -1
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			return 0x00a41004;					// sllv $v0, $a0, $a1
		case t_SHRb:
		case t_SHRw:
		case t_SHRd:
			return 0x00a41006;					// srlv $v0, $a0, $a1
		case t_SARd:
			return 0x00a41007;					// srav $v0, $a0, $a1
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
		/*case t_SARb:
			ret = 0x7c042420;					// seb $a0, $a0
			return (0x00a41007ll<<32)+ret;				// srav $v0, $a0, $a1
		case t_SARw:
			ret = 0x7c042620;					// seh $a0, $a0
			return (0x00a41007ll<<32)+ret;				// srav $v0, $a0, $a1
		case t_ROLd:
			ret = 0x00052823;					// subu	$a1, $0, $a1
			return (0x00a41046ll<<32)+ret;				// rotrv $v0, $a0, $a1*/
		case t_RORd:
			return 0x00a41046;					// rotrv $v0, $a0, $a1
#endif
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			return 0x00041023;					// subu $v0, $0, $a0
		default:
			return 	-1;
	}
}
#else
#ifdef DRC_FLAGS_INVALIDATION
// called when a call to a function can be replaced by a
// call to a simpler function
static void gen_fill_function_ptr(Bit8u * pos,void* fct_ptr,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION_DCODE
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			*(Bit32u*)pos=0x00851021;					// addu $v0, $a0, $a1
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(Bit32u*)pos=0x00851025;					// or $v0, $a0, $a1
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(Bit32u*)pos=0x00851024;					// and $v0, $a0, $a1
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(Bit32u*)pos=0x00851023;					// subu $v0, $a0, $a1
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(Bit32u*)pos=0x00851026;					// xor $v0, $a0, $a1
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			*(Bit32u*)pos=0;							// nop
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(Bit32u*)pos=0x24820001;					// addiu $v0, $a0, 1
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(Bit32u*)pos=0x2482ffff;					// addiu $v0, $a0, -1
			break;
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			*(Bit32u*)pos=0x00a41004;					// sllv $v0, $a0, $a1
			break;
		case t_SHRb:
		case t_SHRw:
		case t_SHRd:
			*(Bit32u*)pos=0x00a41006;					// srlv $v0, $a0, $a1
			break;
		case t_SARd:
			*(Bit32u*)pos=0x00a41007;					// srav $v0, $a0, $a1
			break;
#if (_MIPS_ISA==MIPS32R2) || defined(PSP)
		case t_RORd:
			*(Bit32u*)pos=0x00a41046;					// rotrv $v0, $a0, $a1
			break;
#endif
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(Bit32u*)pos=0x00041023;					// subu $v0, $0, $a0
			break;
		default:
			*(Bit32u*)pos=0x0c000000+((((Bit32u)fct_ptr)>>2)&0x3ffffff);		// jal simple_func
			break;
	}
#else
	*(Bit32u*)pos=0x0c000000+((((Bit32u)fct_ptr)>>2)&0x3ffffff);		// jal simple_func
#endif
}
#endif
#endif
