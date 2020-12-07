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

/* $Id: paging.h,v 1.25 2007-06-12 20:22:07 c2woody Exp $ */

#ifndef DOSBOX_PAGING_H
#define DOSBOX_PAGING_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

class PageDirectory;

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)
#ifndef PSP
#define TLB_SIZE		(1024*1024)
#define TLB_BANKS		0
#define BANK_SHIFT		32
#define BANK_MASK		0
#define PAGING_LINKS 		(128*1024/4)
#else 
#define TLB_SIZE		65536	// This must a power of 2 and greater then LINK_START
#define BANK_SHIFT		28
#define BANK_MASK		0xffff // always the same as TLB_SIZE-1?
#define TLB_BANKS		((1024*1024/TLB_SIZE)-1)
#define PAGING_LINKS 		(10*1024/4)
#endif

#define PFLAG_READABLE		0x1
#define PFLAG_WRITEABLE		0x2
#define PFLAG_HASROM		0x4
#define PFLAG_HASCODE		0x8				//Page contains dynamic code
#define PFLAG_NOCODE		0x10			//No dynamic code can be generated here
#define PFLAG_INIT			0x20			//No dynamic code can be generated here

#define LINK_START	((1024+64)/4)			//Start right after the HMA

class PageHandler {
public:
	virtual ~PageHandler(void) { }
	virtual Bitu readb(PhysPt addr);
	virtual Bitu readw(PhysPt addr);
	virtual Bitu readd(PhysPt addr);
	virtual void writeb(PhysPt addr,Bitu val);
	virtual void writew(PhysPt addr,Bitu val);
	virtual void writed(PhysPt addr,Bitu val);
	virtual HostPt GetHostReadPt(Bitu phys_page);
	virtual HostPt GetHostWritePt(Bitu phys_page);
	virtual bool readb_checked(PhysPt addr, Bitu * val);
	virtual bool readw_checked(PhysPt addr, Bitu * val);
	virtual bool readd_checked(PhysPt addr, Bitu * val);
	virtual bool writeb_checked(PhysPt addr,Bitu val);
	virtual bool writew_checked(PhysPt addr,Bitu val);
	virtual bool writed_checked(PhysPt addr,Bitu val);
	Bitu flags;
};

typedef struct {
	HostPt read;
	HostPt write;
	PageHandler * handler;
	Bit32u	phys_page;
} tlb_entry;

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);
void PAGING_InitTLB(void);
void PAGING_ClearTLB(void);

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page);
void PAGING_UnlinkPages(Bitu lin_page,Bitu pages);
/* This maps the page directly, only use when paging is disabled */
void PAGING_MapPage(Bitu lin_page,Bitu phys_page);
bool PAGING_MakePhysPage(Bitu & page);
void PAGING_InitTLBBank(tlb_entry **bank);

void MEM_SetLFB( Bitu page, Bitu pages, PageHandler *handler);
void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);


#ifdef _MSC_VER
#pragma pack (1)
#endif
struct X86_PageEntryBlock{
#ifdef WORDS_BIGENDIAN
	Bit32u		base:20;
	Bit32u		avl:3;
	Bit32u		g:1;
	Bit32u		pat:1;
	Bit32u		d:1;
	Bit32u		a:1;
	Bit32u		pcd:1;
	Bit32u		pwt:1;
	Bit32u		us:1;
	Bit32u		wr:1;
	Bit32u		p:1;
#else
	Bit32u		p:1;
	Bit32u		wr:1;
	Bit32u		us:1;
	Bit32u		pwt:1;
	Bit32u		pcd:1;
	Bit32u		a:1;
	Bit32u		d:1;
	Bit32u		pat:1;
	Bit32u		g:1;
	Bit32u		avl:3;
	Bit32u		base:20;
#endif
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

union X86PageEntry {
	Bit32u load;
	X86_PageEntryBlock block;
};

struct PagingBlock {
	Bitu			cr3;
	Bitu			cr2;
	struct {
		Bitu page;
		PhysPt addr;
	} base;
	tlb_entry tlb[TLB_SIZE];
	tlb_entry *tlb_banks[TLB_BANKS];
	struct {
		Bitu used;
		Bit32u entries[PAGING_LINKS];
	} links;
	Bit32u		firstmb[LINK_START];
	bool		enabled;
};

extern PagingBlock paging; 

/* Some support functions */

PageHandler * MEM_GetPageHandler(Bitu phys_page);

/* Unaligned address handlers */
Bit16u mem_unalignedreadw(PhysPt address);
Bit32u mem_unalignedreadd(PhysPt address);
void mem_unalignedwritew(PhysPt address,Bit16u val);
void mem_unalignedwrited(PhysPt address,Bit32u val);

bool mem_unalignedreadw_checked_x86(PhysPt address,Bit16u * val);
bool mem_unalignedreadd_checked_x86(PhysPt address,Bit32u * val);
bool mem_unalignedwritew_checked_x86(PhysPt address,Bit16u val);
bool mem_unalignedwrited_checked_x86(PhysPt address,Bit32u val);

/* Special inlined memory reading/writing */

INLINE tlb_entry *get_tlb_entry(PhysPt address) {
	Bitu index=(address>>12);
	if (GCC_UNLIKELY(TLB_BANKS && (index > TLB_SIZE))) {
		Bitu bank=(address>>BANK_SHIFT) - 1;
		if (GCC_UNLIKELY(!paging.tlb_banks[bank]))
			PAGING_InitTLBBank(&paging.tlb_banks[bank]);
		return &paging.tlb_banks[bank][index & BANK_MASK];
	}
	return &paging.tlb[index];
}

/* Use this helper function to access linear addresses in readX/writeX functions */
INLINE PhysPt PAGING_GetPhysicalPage(PhysPt linePage) {
	tlb_entry *entry = get_tlb_entry(linePage);
	return (entry->phys_page<<12);
}

INLINE PhysPt PAGING_GetPhysicalAddress(PhysPt linAddr) {
	tlb_entry *entry = get_tlb_entry(linAddr);
	return (entry->phys_page<<12)|(linAddr&0xfff);
}

/* the complier will hopefully optimize this nicely */

INLINE Bit8u mem_readb_inline(PhysPt address) {
	Bitu index=(address>>12);
	tlb_entry *entry = get_tlb_entry(address);
	if (entry->read) return host_readb(entry->read+address);
	else return (Bit8u)entry->handler->readb(address);
}

INLINE Bit16u mem_readw_inline(PhysPt address) {
	if (!(address & 1)) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->read) return host_readw(entry->read+address);
		else return (Bit16u) entry->handler->readw(address);
	} else return mem_unalignedreadw(address);
}

INLINE Bit32u mem_readd_inline(PhysPt address) {
	if (!(address & 3)) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->read) return host_readd(entry->read+address);
		else return entry->handler->readd(address);
	} else return mem_unalignedreadd(address);
}

INLINE void mem_writeb_inline(PhysPt address,Bit8u val) {
	Bitu index=(address>>12);
	tlb_entry *entry = get_tlb_entry(address);

	if (entry->write) host_writeb(entry->write+address,val);
	else entry->handler->writeb(address,val);
}

INLINE void mem_writew_inline(PhysPt address,Bit16u val) {
	if (!(address & 1)) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->write) host_writew(entry->write+address,val);
		else entry->handler->writew(address,val);
	} else mem_unalignedwritew(address,val);
}

INLINE void mem_writed_inline(PhysPt address,Bit32u val) {
	if (!(address & 3)) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->write) host_writed(entry->write+address,val);
		else entry->handler->writed(address,val);
	} else mem_unalignedwrited(address,val);
}


INLINE Bit16u mem_readw_dyncorex86(PhysPt address) {
	if ((address & 0xfff)<0xfff) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->read) return host_readw(entry->read+address);
		else return (Bit16u)entry->handler->readw(address);
	} else return mem_unalignedreadw(address);
}

INLINE Bit32u mem_readd_dyncorex86(PhysPt address) {
	if ((address & 0xfff)<0xffd) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->read) return host_readd(entry->read+address);
		else return entry->handler->readd(address);
	} else return mem_unalignedreadd(address);
}

INLINE void mem_writew_dyncorex86(PhysPt address,Bit16u val) {
	if ((address & 0xfff)<0xfff) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->write) host_writew(entry->write+address,val);
		else entry->handler->writew(address,val);
	} else mem_unalignedwritew(address,val);
}

INLINE void mem_writed_dyncorex86(PhysPt address,Bit32u val) {
	if ((address & 0xfff)<0xffd) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);

		if (entry->write) host_writed(entry->write+address,val);
		else entry->handler->writed(address,val);
	} else mem_unalignedwrited(address,val);
}


INLINE bool mem_readb_checked_x86(PhysPt address, Bit8u * val) {
	Bitu index=(address>>12);
	tlb_entry *entry = get_tlb_entry(address);
	if (entry->read) {
		*val=host_readb(entry->read+address);
		return false;
	} else {
		Bitu uval;
		bool retval;
		retval=entry->handler->readb_checked(address, &uval);
		*val=(Bit8u)uval;
		return retval;
	}
}

INLINE bool mem_readw_checked_x86(PhysPt address, Bit16u * val) {
	if ((address & 0xfff)<0xfff) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);
		if (entry->read) {
			*val=host_readw(entry->read+address);
			return false;
		} else {
			Bitu uval;
			bool retval;
			retval=entry->handler->readw_checked(address, &uval);
			*val=(Bit16u)uval;
			return retval;
		}
	} else return mem_unalignedreadw_checked_x86(address, val);
}


INLINE bool mem_readd_checked_x86(PhysPt address, Bit32u * val) {
	if ((address & 0xfff)<0xffd) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);
		if (entry->read) {
			*val=host_readd(entry->read+address);
			return false;
		} else {
			Bitu uval;
			bool retval;
			retval=entry->handler->readd_checked(address, &uval);
			*val=(Bit32u)uval;
			return retval;
		}
	} else return mem_unalignedreadd_checked_x86(address, val);
}

INLINE bool mem_writeb_checked_x86(PhysPt address,Bit8u val) {
	Bitu index=(address>>12);
	tlb_entry *entry = get_tlb_entry(address);
	if (entry->write) {
		host_writeb(entry->write+address,val);
		return false;
	} else return entry->handler->writeb_checked(address,val);
}

INLINE bool mem_writew_checked_x86(PhysPt address,Bit16u val) {
	if ((address & 0xfff)<0xfff) {
		tlb_entry *entry = get_tlb_entry(address);
		Bitu index=(address>>12);
		if (entry->write) {
			host_writew(entry->write+address,val);
			return false;
		} else return entry->handler->writew_checked(address,val);
	} else return mem_unalignedwritew_checked_x86(address,val);
}

INLINE bool mem_writed_checked_x86(PhysPt address,Bit32u val) {
	if ((address & 0xfff)<0xffd) {
		Bitu index=(address>>12);
		tlb_entry *entry = get_tlb_entry(address);
		if (entry->write) {
			host_writed(entry->write+address,val);
			return false;
		} else return entry->handler->writed_checked(address,val);
	} else return mem_unalignedwrited_checked_x86(address,val);
}


#endif
