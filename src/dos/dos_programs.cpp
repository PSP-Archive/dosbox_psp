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

/* $Id: dos_programs.cpp,v 1.77 2007-08-22 11:54:54 qbix79 Exp $ */

#include "dosbox.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <math.h>
#include "programs.h"
#include "support.h"
#include "drives.h"
#include "cross.h"
#include "regs.h"
#include "callback.h"
#include "cdrom.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "bios.h"

#ifdef PSP
#include <sys/fcntl.h>
#endif

#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
#include <sys/types.h>
#include <pwd.h>
#endif


#if defined(OS2)
#define INCL DOSFILEMGR
#define INCL_DOSERRORS
#include "os2.h"
#endif

#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

void MSCDEX_SetCDInterface(int intNr, int forceCD);

//Helper function for mount and imgmount to correctly find the path
static void ResolveHomedir(std::string & temp_line) {
	if(!temp_line.size() || temp_line[0] != '~') return; //No ~

	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
}

class MOUNT : public Program {
public:
	void Run(void)
	{
		DOS_Drive * newdrive;char drive;
		std::string label;
		std::string umount;

		/* Check for unmounting */
		if (cmd->FindString("-u",umount,false)) {
			umount[0] = toupper(umount[0]);
			int i_drive = umount[0]-'A';
				if(i_drive < DOS_DRIVES && Drives[i_drive]) {
					switch (DriveManager::UnmountDrive(i_drive)) {
					case 0:
						Drives[i_drive] = 0;
						if(i_drive == DOS_GetDefaultDrive()) 
							DOS_SetDrive(toupper('Z') - 'A');
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCES"),umount[0]);
						break;
					case 1:
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
						break;
					case 2:
						WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
						break;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"),umount[0]);
				}
			return;
		}
#ifdef USE_SDL	  
		// Show list of cdroms
		if (cmd->FindExist("-cd",false)) {
			int num = SDL_CDNumDrives();
   			WriteOut(MSG_Get("PROGRAM_MOUNT_CDROMS_FOUND"),num);
			for (int i=0; i<num; i++) {
				WriteOut("%2d. %s\n",i,SDL_CDName(i));
			};
			return;
		}
#endif
		/* Parse the command line */
		/* if the command line is empty show current mounts */
		if (!cmd->GetCount()) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
			for (int d=0;d<DOS_DRIVES;d++) {
				if (Drives[d]) {
					WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),d+'A',Drives[d]->GetInfo());
				}
			}
			return;
		}

		std::string type="dir";
		cmd->FindString("-t",type,true);
		if (type=="floppy" || type=="dir" || type=="cdrom") {
			Bit16u sizes[4];
			Bit8u mediaid;
			std::string str_size;
			if (type=="floppy") {
				str_size="512,1,2880,2880";/* All space free */
				mediaid=0xF0;		/* Floppy 1.44 media */
			} else if (type=="dir") {
				// 512*127*16513==~1GB total size
				// 512*127*1700==~100MB total free size
				str_size="512,127,16513,1700";
				mediaid=0xF8;		/* Hard Disk */
			} else if (type=="cdrom") {
				str_size="2048,1,65535,0";
				mediaid=0xF8;		/* Hard Disk */
			} else {
				WriteOut(MSG_Get("PROGAM_MOUNT_ILL_TYPE"),type.c_str());
				return;
			}
			/* Parse the free space in mb's (kb's for floppies) */
			std::string mb_size;
			if(cmd->FindString("-freesize",mb_size,true)) {
				char teststr[1024];
				Bit16u sizemb = static_cast<Bit16u>(atoi(mb_size.c_str()));
				if (type=="floppy") {
					sprintf(teststr,"512,1,2880,%d",sizemb*1024/(512*1));
				} else {
					sprintf(teststr,"512,127,16513,%d",sizemb*1024*1024/(512*127));
				}
				str_size=teststr;
			}
		   
			cmd->FindString("-size",str_size,true);
			char number[20];const char * scan=str_size.c_str();
			Bitu index=0;Bitu count=0;
			/* Parse the str_size string */
			while (*scan) {
				if (*scan==',') {
					number[index]=0;sizes[count++]=atoi(number);
					index=0;
				} else number[index++]=*scan;
				scan++;
			}
			number[index]=0;sizes[count++]=atoi(number);
		
			// get the drive letter
			cmd->FindCommand(1,temp_line);
			if ((temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) goto showusage;
			drive=toupper(temp_line[0]);
			if (!isalpha(drive)) goto showusage;

			if (!cmd->FindCommand(2,temp_line)) goto showusage;
			if (!temp_line.size()) goto showusage;
			struct stat test;
			//Win32 : strip tailing backslashes
			//os2: some special drive check
			//rest: substiture ~ for home
			bool failed = false;
#if defined (WIN32) || defined(OS2)
			/* Removing trailing backslash if not root dir so stat will succeed */
			if(temp_line.size() > 3 && temp_line[temp_line.size()-1]=='\\') temp_line.erase(temp_line.size()-1,1);
			if (stat(temp_line.c_str(),&test)) {
#endif
#if defined(WIN32)
// Nothing to do here.
#elif defined (OS2)
				if (temp_line.size() <= 2) // Seems to be a drive.
				{
					failed = true;
					HFILE cdrom_fd = 0;
					ULONG ulAction = 0;

					APIRET rc = DosOpen((unsigned char*)temp_line.c_str(), &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
						OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
					DosClose(cdrom_fd);
					if (rc != NO_ERROR && rc != ERROR_NOT_READY)
					{
						failed = true;
					} else {
						failed = false;
					}
				}
			}
			if (failed) {
#else
			if (stat(temp_line.c_str(),&test)) {
				failed = true;
				ResolveHomedir(temp_line);
				//Try again after resolving ~
				if(!stat(temp_line.c_str(),&test)) failed = false;
			}
			if(failed) {
#endif
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
				return;
			}
			/* Not a switch so a normal directory/file */
			if (!(test.st_mode & S_IFDIR)) {
#ifdef OS2
				HFILE cdrom_fd = 0;
				ULONG ulAction = 0;

				APIRET rc = DosOpen((unsigned char*)temp_line.c_str(), &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
					OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
				DosClose(cdrom_fd);
				if (rc != NO_ERROR && rc != ERROR_NOT_READY) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
			}
#else
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
#endif
			}

			if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
			Bit8u bit8size=(Bit8u) sizes[1];
			if (type=="cdrom") {
				int num = -1;
				cmd->FindInt("-usecd",num,true);
				int error;
				if (cmd->FindExist("-aspi",false)) {
					MSCDEX_SetCDInterface(CDROM_USE_ASPI, num);
				} else if (cmd->FindExist("-ioctl",false)) {
					MSCDEX_SetCDInterface(CDROM_USE_IOCTL, num);
				} else if (cmd->FindExist("-noioctl",false)) {
					MSCDEX_SetCDInterface(CDROM_USE_SDL, num);
				} else {
					MSCDEX_SetCDInterface(CDROM_USE_IOCTL, num);
				}
				newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
				// Check Mscdex, if it worked out...
				switch (error) {
					case 0  :	WriteOut(MSG_Get("MSCDEX_SUCCESS"));				break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));				break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
				};
				if (error && error!=5) {
					delete newdrive;
					return;
				}
			} else {
				/* Give a warning when mount c:\ or the / */
#if defined (WIN32) || defined(OS2)
				if( (temp_line == "c:\\") || (temp_line == "C:\\"))	WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
#else
				if(temp_line == "/") WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_OTHER"));
#endif
				newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
			return;
		}
		if (Drives[drive-'A']) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),drive,Drives[drive-'A']->GetInfo());
			if (newdrive) delete newdrive;
			return;
		}
		if (!newdrive) E_Exit("DOS:Can't create drive");
		Drives[drive-'A']=newdrive;
		/* Set the correct media byte in the table */
		mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,newdrive->GetMediaByte());
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,newdrive->GetInfo());
		/* check if volume label is given and don't allow it to updated in the future */
		if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str(),false);
		/* For hard drives set the label to DRIVELETTER_Drive.
		 * For floppy drives set the label to DRIVELETTER_Floppy.
		 * This way every drive except cdroms should get a label.*/
		else if(type == "dir") { 
			label = drive; label += "_DRIVE";
			newdrive->dirCache.SetLabel(label.c_str(),true);
		} else if(type == "floppy") {
			label = drive; label += "_FLOPPY";
			newdrive->dirCache.SetLabel(label.c_str(),true);
		}
		return;
showusage:
		WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"));
		return;
	}
};

static void MOUNT_ProgramStart(Program * * make) {
	*make=new MOUNT;
}

class MEM : public Program {
public:
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");

		Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
		Bit8u umb_flag=dos_infoblock.GetUMBChainState();
		Bit8u old_memstrat=DOS_GetMemAllocStrategy()&0xff;
		if (umb_start!=0xffff) {
			if ((umb_flag&1)==1) DOS_LinkUMBsToMemChain(0);
			DOS_SetMemAllocStrategy(0);
		}

		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		if ((machine==MCH_PCJR) && (real_readb(0x2000,0)==0x5a) && (real_readw(0x2000,1)==0) && (real_readw(0x2000,3)==0x7ffe)) {
			WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),0x7ffe*16/1024);
		} else WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);

		if (umb_start!=0xffff) {
			DOS_LinkUMBsToMemChain(1);
			DOS_SetMemAllocStrategy(0x40);	// search in UMBs only

			Bit16u largest_block=0,total_blocks=0,block_count=0;
			for (;; block_count++) {
				blocks=0xffff;
				DOS_AllocateMemory(&seg,&blocks);
				if (blocks==0) break;
				total_blocks+=blocks;
				if (blocks>largest_block) largest_block=blocks;
				DOS_AllocateMemory(&seg,&blocks);
			}

			Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
			if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
			DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy

			if (block_count>0) WriteOut(MSG_Get("PROGRAM_MEM_UPPER"),total_blocks*16/1024,block_count,largest_block*16/1024);
		}

		/* Test for and show free XMS */
		reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
		if (reg_al==0x80) {
			reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
			Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
			reg_ah=8;
			CALLBACK_RunRealFar(xms_seg,xms_off);
			if (!reg_bl) {
				WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"),reg_dx);
			}
		}	
		/* Test for and show free EMS */
		Bit16u handle;
		char emm[9] = { 'E','M','M','X','X','X','X','0',0 };
		if (DOS_OpenFile(emm,0,&handle)) {
			DOS_CloseFile(handle);
			reg_ah=0x42;
			CALLBACK_RunRealInt(0x67);
			WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"),reg_bx*16);
		}
	}
};


static void MEM_ProgramStart(Program * * make) {
	*make=new MEM;
}

extern Bit32u floppytype;


class BOOT : public Program {
private:
   
	FILE *getFSFile_mounted(char const* filename, Bit32u *ksize, Bit32u *bsize, Bit8u *error) {
		//if return NULL then put in error the errormessage code if an error was requested
		bool tryload = (*error)?true:false;
		*error = 0;
		Bit8u drive;
		FILE *tmpfile;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName(const_cast<char*>(filename),fullname,&drive)) return NULL;

		try {		
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if(!ldp) return NULL;

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if(tmpfile == NULL) {
				if (!tryload) *error=1;
				return NULL;
			}

			// get file size
			fseek(tmpfile,0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			fclose(tmpfile);

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
			if(tmpfile == NULL) {
//				if (!tryload) *error=2;
//				return NULL;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
				if(tmpfile == NULL) {
					if (!tryload) *error=1;
					return NULL;
				}
			}

			return tmpfile;
		}
		catch(...) {
			return NULL;
		}
	}
   
	FILE *getFSFile(char const * filename, Bit32u *ksize, Bit32u *bsize,bool tryload=false) {
		Bit8u error = tryload?1:0;
		FILE* tmpfile = getFSFile_mounted(filename,ksize,bsize,&error);
		if(tmpfile) return tmpfile;
		//File not found on mounted filesystem. Try regular filesystem
		std::string filename_s(filename);
		ResolveHomedir(filename_s);
		tmpfile = fopen(filename_s.c_str(),"rb+");
		if(!tmpfile) {
			if( (tmpfile = fopen(filename_s.c_str(),"rb")) ) {
				//File exists; So can't be opened in correct mode => error 2
//				fclose(tmpfile);
//				if(tryload) error = 2;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				fseek(tmpfile,0L, SEEK_END);
				*ksize = (ftell(tmpfile) / 1024);
				*bsize = ftell(tmpfile);
				return tmpfile;
			}
			// Give the delayed errormessages from the mounted variant (or from above)
			if(error == 1) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
			if(error == 2) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
			return NULL;
		}
		fseek(tmpfile,0L, SEEK_END);
		*ksize = (ftell(tmpfile) / 1024);
		*bsize = ftell(tmpfile);
		return tmpfile;
	}

	void printError(void) {
		WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"));
	}

	void disable_umb_ems_xms(void) {
		Section* dos_sec = control->GetSection("dos");
		dos_sec->ExecuteDestroy(false);
		char test[20];
		strcpy(test,"umb=false");
		dos_sec->HandleInputline(test);
		strcpy(test,"xms=false");
		dos_sec->HandleInputline(test);
		strcpy(test,"ems=false");
		dos_sec->HandleInputline(test);
		dos_sec->ExecuteInit(false);
     }

public:
   
	void Run(void) {
		int usefile_1=-1;
		int usefile_2=-1;
		Bitu i; 
		Bit32u floppysize;
		Bit32u rombytesize_1=0;
		Bit32u rombytesize_2=0;
		Bit8u drive;
		std::string cart_cmd="";

		if(!cmd->GetCount()) {
			printError();
			return;
		}
		i=0;
		drive = 'A';
		while(i<cmd->GetCount()) {
			if(cmd->FindCommand(i+1, temp_line)) {
				if((temp_line == "-l") || (temp_line == "-L")) {
					/* Specifying drive... next argument then is the drive */
					i++;
					if(cmd->FindCommand(i+1, temp_line)) {
						drive=toupper(temp_line[0]);
						if ((drive != 'A') && (drive != 'C') && (drive != 'D')) {
							printError();
							return;
						}

					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				if((temp_line == "-e") || (temp_line == "-E")) {
					/* Command mode for PCJr cartridges */
					i++;
					if(cmd->FindCommand(i + 1, temp_line)) {
						for(size_t ct = 0;ct < temp_line.size();ct++) temp_line[ct] = toupper(temp_line[ct]);
						cart_cmd = temp_line;
					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), temp_line.c_str());
				Bit32u rombytesize;
				int usefile = fileno(getFSFile(temp_line.c_str(), &floppysize, &rombytesize));
				if(usefile != -1) {
					if(diskSwap[i] != NULL) delete diskSwap[i];
					diskSwap[i] = new imageDisk(usefile, (Bit8u *)temp_line.c_str(), floppysize, false);
					if (usefile_1==-1) {
						usefile_1=usefile;
						rombytesize_1=rombytesize;
					} else {
						usefile_2=usefile;
						rombytesize_2=rombytesize;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}

			}
			i++;
		}

		swapPosition = 0;

		swapInDisks();

		if(imageDiskList[drive-65]==NULL) {
			WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
			return;
		}

		bootSector bootarea;
		imageDiskList[drive-65]->Read_Sector(0,0,1,(Bit8u *)&bootarea);
		if ((bootarea.rawdata[0]==0x50) && (bootarea.rawdata[1]==0x43) && (bootarea.rawdata[2]==0x6a) && (bootarea.rawdata[3]==0x72)) {
			if (machine!=MCH_PCJR) WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
			else {
				Bit8u rombuf[65536];
				Bits cfound_at=-1;
				if (cart_cmd!="") {
					/* read cartridge data into buffer */
					lseek(usefile_1,0x200L, SEEK_SET);
					read(usefile_1, rombuf, rombytesize_1-0x200);

					char cmdlist[1024];
					cmdlist[0]=0;
					Bitu ct=6;
					Bits clen=rombuf[ct];
					char buf[257];
					if (cart_cmd=="?") {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							strcat(cmdlist," ");
							strcat(cmdlist,buf);
							ct+=1+clen+3;
							if (ct>0x1000) break;
							clen=rombuf[ct];
						}
						if (ct>6) {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
						} else {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
						}
						for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
							if(diskSwap[dct]!=NULL) {
								delete diskSwap[dct];
								diskSwap[dct]=NULL;
							}
						}
						close(usefile_1);
						return;
					} else {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							strcat(cmdlist," ");
							strcat(cmdlist,buf);
							ct+=1+clen;

							if (cart_cmd==buf) {
								cfound_at=ct;
								break;
							}

							ct+=3;
							if (ct>0x1000) break;
							clen=rombuf[ct];
						}
						if (cfound_at<=0) {
							if (ct>6) {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
							} else {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
							}
							for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
								if(diskSwap[dct]!=NULL) {
									delete diskSwap[dct];
									diskSwap[dct]=NULL;
								}
							}
							close(usefile_1);
							return;
						}
					}
				}

				disable_umb_ems_xms();
				void PreparePCJRCartRom(void);
				PreparePCJRCartRom();

				if (usefile_1==-1) return;

				Bit32u sz1,sz2;
				FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
				if (tfile!=NULL) {
					fseek(tfile, 0x3000L, SEEK_SET);
					Bit32u drd=fread(rombuf, 1, 0xb000, tfile);
					if (drd==0xb000) {
						for(i=0;i<0xb000;i++) phys_writeb(0xf3000+i,rombuf[i]);
					}
					fclose(tfile);
				}

				if (usefile_2!=-1) {
					lseek(usefile_2, 0x0L, SEEK_SET);
					read(usefile_2, rombuf, 0x200);
					PhysPt romseg_pt=host_readw(&rombuf[0x1ce])<<4;

					/* read cartridge data into buffer */
					lseek(usefile_2, 0x200L, SEEK_SET);
					read(usefile_2, rombuf, rombytesize_2-0x200);
					close(usefile_2);

					/* write cartridge data into ROM */
					for(i=0;i<rombytesize_2-0x200;i++) phys_writeb(romseg_pt+i,rombuf[i]);
				}

				lseek(usefile_1, 0x0L, SEEK_SET);
				read(usefile_1, rombuf, 0x200);
				Bit16u romseg=host_readw(&rombuf[0x1ce]);

				/* read cartridge data into buffer */
				lseek(usefile_1,0x200L, SEEK_SET);
				read(usefile_1,rombuf, rombytesize_1-0x200);
				close(usefile_1);

				/* write cartridge data into ROM */
				for(i=0;i<rombytesize_1-0x200;i++) phys_writeb((romseg<<4)+i,rombuf[i]);


				if (cart_cmd=="") {
					Bit32u old_int18=mem_readd(0x60);
					/* run cartridge setup */
					SegSet16(ds,romseg);
					SegSet16(es,romseg);
					SegSet16(ss,0x8000);
					reg_esp=0xfffe;
					CALLBACK_RunRealFar(romseg,0x0003);

					Bit32u new_int18=mem_readd(0x60);
					if (old_int18!=new_int18) {
						/* boot cartridge (int18) */
						SegSet16(cs,RealSeg(new_int18));
						reg_ip = RealOff(new_int18);
					} else {
						for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
							if(diskSwap[dct]!=NULL) {
								delete diskSwap[dct];
								diskSwap[dct]=NULL;
							}
						}
					}
				} else {
					if (cfound_at>0) {
						/* run cartridge setup */
						SegSet16(ds,dos.psp());
						SegSet16(es,dos.psp());
						CALLBACK_RunRealFar(romseg,cfound_at);
					} else {
						for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
							if(diskSwap[dct]!=NULL) {
								delete diskSwap[dct];
								diskSwap[dct]=NULL;
							}
						}
					}
				}
			}
		} else {
			disable_umb_ems_xms();
			WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);
			for(i=0;i<512;i++) real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

			/* revector some dos-allocated interrupts */
			real_writed(0,0x01*4,0xf000ff53);
			real_writed(0,0x03*4,0xf000ff53);

			SegSet16(cs, 0);
			reg_ip = 0x7c00;
			SegSet16(ds, 0);
			SegSet16(es, 0);
			/* set up stack at a safe place */
			SegSet16(ss, 0x7000);
			reg_esp = 0x400;
			reg_esi = 0;
			reg_ecx = 1;
			reg_ebp = 0;
			reg_eax = 0;
			reg_edx = 0; //Head 0 drive 0
			reg_ebx= 0x7c00; //Real code probably uses bx to load the image
		}
	}
};


static void BOOT_ProgramStart(Program * * make) {
	*make=new BOOT;
}



// LOADFIX

class LOADFIX : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void) 
{
	Bit16u commandNr	= 1;
	Bit16u kb			= 64;
	if (cmd->FindCommand(commandNr,temp_line)) {
		if (temp_line[0]=='-') {
			char ch = temp_line[1];
			if ((*upcase(&ch)=='D') || (*upcase(&ch)=='F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	Bit16u segment;
	Bit16u blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		DOS_MCB mcb((Bit16u)(segment-1));
		mcb.SetPSPSeg(0x40);			// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			safe_strncpy(filename,temp_line.c_str(),128);
			// Setup commandline
			bool ok;
			char args[256];
			args[0] = 0;
			do {
				ok = cmd->FindCommand(commandNr++,temp_line);
				strncat(args,temp_line.c_str(),256);
				strncat(args," ",256);
			} while (ok);			
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);		
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);	
	}
};

static void LOADFIX_ProgramStart(Program * * make) {
	*make=new LOADFIX;
}

// RESCAN

class RESCAN : public Program {
public:
	void Run(void);
};

void RESCAN::Run(void) 
{
	// Get current drive
	Bit8u drive = DOS_GetDefaultDrive();
	if (Drives[drive]) {
		Drives[drive]->EmptyCache();
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	}
};

static void RESCAN_ProgramStart(Program * * make) {
	*make=new RESCAN;
}

class INTRO : public Program {
public:
	void DisplayMount(void) {
		/* Basic mounting has a version for each operating system.
		 * This is done this way so both messages appear in the language file*/
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#if (WIN32)
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else			
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
	}

	void Run(void) {
		/* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
		if(DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
		if(cmd->FindExist("cdrom",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
			return;
		}
		if(cmd->FindExist("mount",false)) {
			WriteOut("\033[2J");//Clear screen before printing
			DisplayMount();
			return;
		}
		if(cmd->FindExist("special",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
			return;
		}
		/* Default action is to show all pages */
		WriteOut(MSG_Get("PROGRAM_INTRO"));
		Bit8u c;Bit16u n=1;
		DOS_ReadFile (STDIN,&c,&n);
		DisplayMount();
		DOS_ReadFile (STDIN,&c,&n);
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
		DOS_ReadFile (STDIN,&c,&n);
		WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
	}
};

static void INTRO_ProgramStart(Program * * make) {
	*make=new INTRO;
}

class IMGMOUNT : public Program {
public:
	void Run(void)
	{
		DOS_Drive * newdrive;
		imageDisk * newImage;
		Bit32u imagesize;
		char drive;
		std::string label;
		std::vector<std::string> paths;
		std::string umount;
		/* Check for unmounting */
		if (cmd->FindString("-u",umount,false)) {
			umount[0] = toupper(umount[0]);
			int i_drive = umount[0]-'A';
				if (i_drive < DOS_DRIVES && Drives[i_drive]) {
					switch (DriveManager::UnmountDrive(i_drive)) {
					case 0:
						Drives[i_drive] = 0;
						if (i_drive == DOS_GetDefaultDrive()) 
							DOS_SetDrive(toupper('Z') - 'A');
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCES"),umount[0]);
						break;
					case 1:
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
						break;
					case 2:
						WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
						break;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"),umount[0]);
				}
			return;
		}


		std::string type="hdd";
		std::string fstype="fat";
		cmd->FindString("-t",type,true);
		cmd->FindString("-fs",fstype,true);
		if(type == "cdrom") type = "iso"; //Tiny hack for people who like to type -t cdrom
		Bit8u mediaid;
		if (type=="floppy" || type=="hdd" || type=="iso") {
			Bit16u sizes[4];
			bool imgsizedetect=false;
			
			std::string str_size;
			mediaid=0xF8;

			if (type=="floppy") {
				mediaid=0xF0;		
			} else if (type=="iso") {
				str_size="650,127,16513,1700";
				mediaid=0xF8;		
				fstype = "iso";
			} 
			cmd->FindString("-size",str_size,true);
			if ((type=="hdd") && (str_size.size()==0)) {
				imgsizedetect=true;
			} else {
				char number[20];
				const char * scan=str_size.c_str();
				Bitu index=0;Bitu count=0;
				
				while (*scan) {
					if (*scan==',') {
						number[index]=0;sizes[count++]=atoi(number);
						index=0;
					} else number[index++]=*scan;
					scan++;
				}
				number[index]=0;sizes[count++]=atoi(number);
			}
		
			if(fstype=="fat" || fstype=="iso") {
				// get the drive letter
				if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
				drive=toupper(temp_line[0]);
				if (!isalpha(drive)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
			} else if (fstype=="none") {
				cmd->FindCommand(1,temp_line);
				if ((temp_line.size() > 1) || (!isdigit(temp_line[0]))) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
				drive=temp_line[0];
				if((drive-'0')>3) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
			} else {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"));
				return;
			}
			
			// find all file parameters, assuming that all option parameters have been removed
			while(cmd->FindCommand(paths.size() + 2, temp_line) && temp_line.size()) {
				
				struct stat test;
#ifdef PSP
				if (temp_line == "msstor0:"); else
#endif
				if (stat(temp_line.c_str(),&test)) {
					//See if it works if the ~ are written out
					std::string homedir(temp_line);
					ResolveHomedir(homedir);
					if(!stat(homedir.c_str(),&test)) {
						temp_line = homedir;
					} else {
						// convert dosbox filename to system filename
						char fullname[CROSS_LEN];
						char tmp[CROSS_LEN];
						safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);

						Bit8u dummy;
						if (!DOS_MakeName(tmp, fullname, &dummy) || strncmp(Drives[dummy]->GetInfo(),"local directory",15)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
							return;
						}

						localDrive *ldp = (localDrive*)Drives[dummy];
						ldp->GetSystemFilename(tmp, fullname);
						temp_line = tmp;

						if (stat(temp_line.c_str(),&test)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
							return;
						}

						if ((test.st_mode & S_IFDIR)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT"));
							return;
						}
					}
				}
				paths.push_back(temp_line);
			}
			if (paths.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
				return;	
			}
			if (paths.size() == 1)
				temp_line = paths[0];
			if (paths.size() > 1 && fstype != "iso") {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES"));
				return;
			}

			if(fstype=="fat") {
				if (imgsizedetect) {
					int diskfile = open(temp_line.c_str(), O_RDONLY);
					if(!diskfile) {
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
						return;
					}
					Bit32u fcsize = lseek(diskfile, 0L, SEEK_END) / 512L;
					Bit8u buf[512];
					lseek(diskfile, 0L, SEEK_SET);
					if (read(diskfile,buf,sizeof(Bit8u)*512)<512) {
						close(diskfile);
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
						return;
					}
					close(diskfile);
					if ((buf[510]!=0x55) || (buf[511]!=0xaa)) {
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
						return;
					}
					Bitu sectors=(Bitu)(fcsize/(16*63));
					if (sectors*16*63!=fcsize) {
						sizes[0]=512;
						sizes[2]=ceilf(fcsize/(62.0f*1024.0f));
						sizes[1]=ceilf(fcsize/(1024.0f*sizes[2]));
						sizes[3]=fcsize/(sizes[1]*sizes[2]);
					} else sizes[0]=512;	sizes[1]=63;	sizes[2]=16;	sizes[3]=sectors;
					LOG_MSG("autosized image file: %d:%d:%d:%d",sizes[0],sizes[1],sizes[2],sizes[3]);
				}

				newdrive=new fatDrive(temp_line.c_str(),sizes[0],sizes[1],sizes[2],sizes[3],0);
				if(!(dynamic_cast<fatDrive*>(newdrive))->created_succesfully) {
					delete newdrive;
					newdrive = 0;
				}
			} else if (fstype=="iso") {
			} else {
				int newDisk = open(temp_line.c_str(), O_RDONLY);
				imagesize = lseek(newDisk,0L, SEEK_END)/1024L;

				newImage = new imageDisk(newDisk, (Bit8u *)temp_line.c_str(), imagesize, (imagesize > 2880));
				if(imagesize>2880) newImage->Set_Geometry(sizes[2],sizes[3],sizes[1],sizes[0]);
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED"),type.c_str());
			return;
		}

		if(fstype=="fat") {
			if (Drives[drive-'A']) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
				if (newdrive) delete newdrive;
				return;
			}
			if (!newdrive) {WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));return;}
			Drives[drive-'A']=newdrive;
			// Set the correct media byte in the table 
			mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,mediaid);
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,temp_line.c_str());
			if(((fatDrive *)newdrive)->loadedDisk->hardDrive) {
				if(imageDiskList[2] == NULL) {
					imageDiskList[2] = ((fatDrive *)newdrive)->loadedDisk;
					updateDPT();
					return;
				}
				if(imageDiskList[3] == NULL) {
					imageDiskList[3] = ((fatDrive *)newdrive)->loadedDisk;
					updateDPT();
					return;
				}
			}
			if(!((fatDrive *)newdrive)->loadedDisk->hardDrive) {
				imageDiskList[0] = ((fatDrive *)newdrive)->loadedDisk;
			}
		} else if (fstype=="iso") {
			if (Drives[drive-'A']) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
				return;
			}
			MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
			// create new drives for all images
			std::vector<DOS_Drive*> isoDisks;
			std::vector<std::string>::size_type i;
			std::vector<DOS_Drive*>::size_type ct;
			for (i = 0; i < paths.size(); i++) {
				int error = -1;
				DOS_Drive* newDrive = new isoDrive(drive, paths[i].c_str(), mediaid, error);
				isoDisks.push_back(newDrive);
				switch (error) {
					case 0  :	break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));				break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					case 6  :	WriteOut(MSG_Get("MSCDEX_INVALID_FILEFORMAT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
				}
				// error: clean up and leave
				if (error) {
					for(ct = 0; ct < isoDisks.size(); ct++) {
						delete isoDisks[ct];
					}
					return;
				}
			}
			// Update DriveManager
			for(ct = 0; ct < isoDisks.size(); ct++) {
				DriveManager::AppendDisk(drive - 'A', isoDisks[ct]);
			}
			DriveManager::InitializeDrive(drive - 'A');
			
			// Set the correct media byte in the table 
			mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 2, mediaid);
			
			// Print status message (success)
			WriteOut(MSG_Get("MSCDEX_SUCCESS"));
			std::string tmp(paths[0]);
			for (i = 1; i < paths.size(); i++) {
				tmp += "; " + paths[i];
			}
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());
			
		} else if (fstype=="none") {
			if(imageDiskList[drive-'0'] != NULL) delete imageDiskList[drive-'0'];
			imageDiskList[drive-'0'] = newImage;
			updateDPT();
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),drive-'0',temp_line.c_str());
		}

		// check if volume label is given
		//if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
		return;
	}
};

void IMGMOUNT_ProgramStart(Program * * make) {
	*make=new IMGMOUNT;
}

#ifdef USE_ALT_KEYB

Bitu DOS_SwitchKeyboardLayout(const char* new_layout);
Bitu DOS_LoadKeyboardLayout(const char * layoutname, Bit32s codepage, const char * codepagefile);

class KEYB : public Program {
public:
	void Run(void);
};

void KEYB::Run(void) {
	if (cmd->FindCommand(1,temp_line)) {
		if (cmd->FindString("?",temp_line,false)) {
			WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
		} else {
			/* first parameter is layout ID */
			Bitu keyb_error=0;
			std::string cp_string;
			if (cmd->FindCommand(2,cp_string)) {
				/* second parameter is codepage number */
				Bit32s par_cp=atoi(cp_string.c_str());
				char cp_file_name[256];
				if (cmd->FindCommand(3,cp_string)) {
					/* third parameter is codepage file */
					strcpy(cp_file_name, cp_string.c_str());
				} else {
					/* no codepage file specified, use automatic selection */
					strcpy(cp_file_name, "auto");
				}

				keyb_error=DOS_LoadKeyboardLayout(temp_line.c_str(), par_cp, cp_file_name);
			} else keyb_error=DOS_SwitchKeyboardLayout(temp_line.c_str());
			switch (keyb_error) {
				case KEYB_NOERROR:
					WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"),temp_line.c_str(),dos.loaded_codepage);
					break;
				case KEYB_FILENOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				case KEYB_INVALIDFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"),temp_line.c_str());
					break;
				case KEYB_LAYOUTNOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),temp_line.c_str(),dos.loaded_codepage);
					break;
				case KEYB_INVALIDCPFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				default:
					LOG(LOG_DOSMISC,LOG_ERROR)("KEYB:Invalid returncode %x",keyb_error);
					break;
			}
		}
	} else {
		/* no parameter in the command line, just output codepage info */
		WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
	}
};

static void KEYB_ProgramStart(Program * * make) {
	*make=new KEYB;
}
#endif

#ifdef PSP
#include <pspctrl.h>
#include <psppower.h>
#include "keyboard.h"

// INPUTMAP

extern keymap inputmap[];
extern bool control_map;
extern bool joy_state;
typedef struct keyinfo {
	KBD_KEYS key;
	char *ascii;
} keyinfo;

class INPUTMAP : public Program {
public:
	void Run(void);
private:
	static const char *button[]; 
	static const keyinfo chartab[];
};

const keyinfo INPUTMAP::chartab[] = 
	{{KBD_1,"1"},{KBD_2,"2"},{KBD_3,"3"},{KBD_4,"4"},{KBD_5,"5"},{KBD_6,"6"},{KBD_7,"7"},{KBD_8,"8"},{KBD_9,"9"},{KBD_0,"0"},
	{KBD_q,"q"},{KBD_w,"w"},{KBD_e,"e"},{KBD_r,"r"},{KBD_t,"t"},{KBD_y,"y"},{KBD_u,"u"},{KBD_i,"i"},{KBD_o,"o"},{KBD_p,"p"},
	{KBD_a,"a"},{KBD_s,"s"},{KBD_d,"d"},{KBD_f,"f"},{KBD_g,"g"},{KBD_h,"h"},{KBD_j,"j"},{KBD_k,"k"},{KBD_l,"l"},{KBD_z,"z"},
	{KBD_x,"x"},{KBD_c,"c"},{KBD_v,"v"},{KBD_b,"b"},{KBD_n,"n"},{KBD_m,"m"},
	{KBD_f1,"f1"},{KBD_f2,"f2"},{KBD_f3,"f3"},{KBD_f4,"f4"},{KBD_f5,"f5"},{KBD_f6,"f6"},{KBD_f7,"f7"},
	{KBD_f8,"f8"},{KBD_f9,"f9"},{KBD_f10,"f10"},{KBD_f11,"f11"},{KBD_f12,"f12"},
	{KBD_esc,"esc"},{KBD_tab,"tab"},{KBD_backspace,"bs"},{KBD_enter,"enter"},{KBD_space,"space"},
	{KBD_leftalt,"lalt"},{KBD_rightalt,"ralt"},{KBD_leftctrl,"lctrl"},{KBD_rightctrl,"rctrl"},{KBD_leftshift,"lshift"},{KBD_rightshift,"rshift"},
	{KBD_capslock,"caplock"},{KBD_scrolllock,"scrllock"},{KBD_numlock,"numlock"},
	{KBD_grave,"`"},{KBD_minus,"-"},{KBD_equals,"="},{KBD_backslash,"\\"},{KBD_leftbracket,"["},{KBD_rightbracket,"]"},
	{KBD_semicolon,";"},{KBD_quote,"'"},{KBD_period,"."},{KBD_comma,","},{KBD_slash,"/"},
	{KBD_insert,"insert"},{KBD_home,"home"},{KBD_pageup,"pgup"},{KBD_delete,"del"},{KBD_end,"end"},{KBD_pagedown,"pgdn"},
	{KBD_left,"left"},{KBD_up,"up"},{KBD_down,"down"},{KBD_right,"right"},
	{KBD_kp1,"n1"},{KBD_kp2,"n2"},{KBD_kp3,"n3"},{KBD_kp4,"n4"},{KBD_kp5,"n5"},{KBD_kp6,"n6"},{KBD_kp7,"n7"},{KBD_kp8,"n8"},{KBD_kp9,"n9"},{KBD_kp0,"n9"},
	{KBD_kpdivide,"n/"},{KBD_kpmultiply,"n*"},{KBD_kpminus,"n-"},{KBD_kpplus,"n+"},{KBD_kpenter,"nenter"},{KBD_kpperiod,"n."},
	{KBD_button1,"button1"},{KBD_button2,"button2"},{KBD_LAST, NULL}};

const char *INPUTMAP::button[] = {"select", "start", "up", "right", "down", "left", "triangle", "circle", "cross", "square", "ltrigger", "rtrigger", NULL}; 


void INPUTMAP::Run(void) {
	if(!cmd->GetCount()) {
		WriteOut(MSG_Get("PROGRAM_INPUTMAP_USAGE"));
		return;
	}
	if(cmd->FindExist("exec", true)) {
		Bitu commandNr = 1;
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			strncpy(filename,temp_line.c_str(),128);
			// Setup commandline
			bool ok;
			char args[256];
			args[0] = 0;
			do {
				ok = cmd->FindCommand(commandNr++,temp_line);
				strncat(args,temp_line.c_str(),256);
				strncat(args," ",256);
			} while (ok);			
			// Use shell to start program
			DOS_Shell shell;
			control_map = true;
			shell.Execute(filename,args);
			//control_map = false;
			return;
		}
	} else if(cmd->FindExist("analog", true)) {
		if(cmd->FindExist("mouse", true)) joy_state=false;
		else if(cmd->FindExist("joystick", true)) joy_state=true;
		else WriteOut(MSG_Get("PROGRAM_INPUTMAP_INVALID"));
		return;
	} else {
		std::string key;
		for(Bitu i=0; button[i]; i++) {
			if(cmd->FindString(button[i], key)==true) {
				for(Bitu j=0; chartab[j].ascii; j++) {
					if(key == chartab[j].ascii) {
						if(chartab[j].key == KBD_LAST) break;
						inputmap[i].key = chartab[j].key;
						WriteOut(MSG_Get("PROGRAM_INPUTMAP_MAPPED"), chartab[j].ascii, button[i]);
						return;
					}
				}
				break;
			}
		}
	}
	WriteOut(MSG_Get("PROGRAM_INPUTMAP_INVALID"));
}

static void INPUTMAP_ProgramStart(Program * * make) {
	*make = new INPUTMAP;
}

class SYSOPT : public Program {
public:
	void Run(void);
};

void SYSOPT::Run(void) {
	bool set = false;
	if(cmd->GetCount() == 2) set = true;
	if(cmd->FindExist("clock")) {
		if(set == true) {
			int val;
			cmd->FindInt("clock", val);
			if((val > 333) || (val < 200)) goto error;
			scePowerSetClockFrequency(val, val, val>>1);
		}
		WriteOut(MSG_Get("PROGRAM_SYSOPT_CLOCK"),scePowerGetCpuClockFrequency());
		return;
	}
error:
	WriteOut(MSG_Get("PROGRAM_SYSOPT_USAGE"));
}

static void SYSOPT_ProgramStart(Program * * make) {
	*make = new SYSOPT;
}

#include <pspdebug.h>
#include <stdarg.h>
extern profile_regs_ll regs;
extern bool profile, fixup;

class PROF : public Program {
public:
	void Run(void);
private:
	static void shell_fprintf(Bit16u handle, const char *format,...) {
		char buf[1024];
		va_list msg;
	
		va_start(msg,format);
		vsprintf(buf,format,msg);
		va_end(msg);

		Bit16u size=strlen(buf);
		DOS_WriteFile(handle,(Bit8u *)buf,&size);
	}
};

void PROF::Run(void) {
	Bitu commandNr = 1;
	std::string output = "";
	cmd->FindString("-o", output, true);
	if (cmd->FindCommand(commandNr++,temp_line)) {
		// get Filename
		char filename[128];
		strncpy(filename,temp_line.c_str(),128);
		// Setup commandline
		bool ok;
		char args[256];
		args[0] = 0;
		do {
			ok = cmd->FindCommand(commandNr++,temp_line);
			strncat(args,temp_line.c_str(),256);
			strncat(args," ",256);
		} while (ok);			
		// Use shell to start program
		DOS_Shell shell;
		profile = true;
		memset(&regs, '\0', sizeof(profile_regs_ll));
		pspDebugProfilerClear();
		pspDebugProfilerEnable();
		shell.Execute(filename,args);
		profile = false;
		pspDebugProfilerDisable();
		
		Bit16u handle=STDOUT;
		if(output != "")
		{
			char filename[255];
			strncpy(filename, output.c_str(), 255);
			DOS_CreateFile(filename, 0, &handle);
		}
		unsigned long long stall = regs.internal + regs.memory + regs.copz + regs.vfpu;
		shell_fprintf(handle,"\n********** Profile ***********\n");
        	shell_fprintf(handle,"systemck       : %20llu [cycles]\n", regs.systemck);
	        shell_fprintf(handle,"cpu ck         : %20llu [cycles]\n", regs.cpuck);
        	shell_fprintf(handle,"stall          : %20llu [cycles]\n", stall);
	        shell_fprintf(handle,"+(internal)    : %20llu [cycles]\n", regs.internal);
        	shell_fprintf(handle,"+--(memory)    : %20llu [cycles]\n", regs.memory);
	        shell_fprintf(handle,"+----(COPz)    : %20llu [cycles]\n", regs.copz);
        	shell_fprintf(handle,"+----(VFPU)    : %20llu [cycles]\n", regs.vfpu);
	        shell_fprintf(handle,"sleep          : %20llu [cycles]\n", regs.sleep);
        	shell_fprintf(handle,"bus access     : %20llu [cycles]\n", regs.bus_access);
	        shell_fprintf(handle,"uncached load  : %20llu [times]\n", regs.uncached_load);
        	shell_fprintf(handle,"uncached store : %20llu [times]\n", regs.uncached_store);
	        shell_fprintf(handle,"cached load    : %20llu [times]\n", regs.cached_load);
        	shell_fprintf(handle,"cached store   : %20llu [times]\n", regs.cached_store);
	        shell_fprintf(handle,"I cache miss   : %20llu [times]\n", regs.i_miss);
        	shell_fprintf(handle,"D cache miss   : %20llu [times]\n", regs.d_miss);
	        shell_fprintf(handle,"D cache wb     : %20llu [times]\n", regs.d_writeback);
        	shell_fprintf(handle,"COP0 inst.     : %20llu [inst.]\n", regs.cop0_inst);
	        shell_fprintf(handle,"FPU  inst.     : %20llu [inst.]\n", regs.fpu_inst);
        	shell_fprintf(handle,"VFPU inst.     : %20llu [inst.]\n", regs.vfpu_inst);
	        shell_fprintf(handle,"local bus      : %20llu [cycles]\n\n", regs.local_bus);
		shell_fprintf(handle,"exec inst      : %20llu [inst.]\n", regs.cpuck - (stall + regs.sleep));
		shell_fprintf(handle,"cached mem acc : %20llu [times]\n", regs.cached_load + regs.cached_store);
		shell_fprintf(handle,"I$ miss rateck : %20llf [%%]\n", ((double)regs.i_miss / (double)(regs.cpuck - (stall + regs.sleep)))*100);
		shell_fprintf(handle,"D$ miss ratenum: %20llf [%%]\n", ((double)regs.d_miss / (double)(regs.cached_load + regs.cached_store))*100);
		shell_fprintf(handle,"D$ miss rateck : %20llf [%%]\n", ((double)regs.d_miss / (double)(regs.cpuck - (stall + regs.sleep)))*100);
		shell_fprintf(handle,"bus util       : %20llf [%%]\n", ((double)regs.bus_access / (double)regs.systemck)*100);
		shell_fprintf(handle,"cycle per inst : %20llf [cpi]\n", (double)regs.cpuck /  (double)(regs.cpuck - (stall + regs.sleep)));
		
	}
}

static void PROF_ProgramStart(Program * * make) {
	*make = new PROF;
}
#endif

void DOS_SetupPrograms(void) {
	/*Add Messages */

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1","Current mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_USAGE","Usage \033[34;1mMOUNT Drive-Letter Local-Directory\033[0m\nSo a MOUNT c c:\\windows mounts windows directory as the c: drive in DOSBox\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCES","Drive %c has succesfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","\033[31;1mMounting c:\\ is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","\033[31;1mMounting / is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");

	MSG_Add("PROGRAM_MEM_CONVEN","%10d Kb free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND","%10d Kb free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND","%10d Kb free expanded memory\n");
	MSG_Add("PROGRAM_MEM_UPPER","%10d Kb free upper memory in %d blocks (largest UMB %d Kb)\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC","%d kb allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC","%d kb freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

	MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CDRom-drives have to be continuous.\n");
	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
	MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Failure: Path not valid.\n");
	MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CDRom-drives (max: 5). MSCDEX Installation failed.\n");
	MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
	MSG_Add("MSCDEX_INVALID_FILEFORMAT","MSCDEX: Failure: File is either no iso/cue image or contains errors.\n");
	MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");

	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

	MSG_Add("PROGRAM_INTRO",
		"\033[2J\033[32;1mWelcome to DOSBox\033[0m, an x86 emulator with sound and graphics.\n"
		"DOSBox creates a shell for you which looks like old plain DOS.\n"
		"\n"
		"For information about basic mount type \033[34;1mintro mount\033[0m\n"
		"For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
		"For information about special keys type \033[34;1mintro special\033[0m\n"
		"For more information about DOSBox, go to \033[34;1mhttp://dosbox.sourceforge.net/wiki\033[0m\n"
		"\n"
		"\033[31;1mDOSBox will stop/exit without a warning if an error occured!\033[0m\n"
		"\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_START",
		"\033[32;1mHere are some commands to get you started:\033[0m\n"
		"Before you can use the files located on your own filesystem,\n"
		"You have to mount the directory containing the files.\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c c:\\dosprog\\\033[37m will create a C drive with c:\\dosprog as contents. \xBA\n"
		"\xBA                                                                        \xBA\n"
		"\xBA \033[32mc:\\dosprog\\\033[37m is an example. Replace it with your own games directory.  \033[37m \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c ~/dosprog\033[37m will create a C drive with ~/dosprog as contents. \xBA\n"
		"\xBA                                                                     \xBA\n"
		"\xBA \033[32m~/dosprog\033[37m is an example. Replace it with your own games directory. \033[37m \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_END",
		"When the mount has succesfully completed you can type \033[34;1mc:\033[0m to go to your freshly\n"
		"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
		" \033[34;1mcd\033[0m will allow you to\n"
		"enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
		"You can run programs/files which end with \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM",
		"\033[2J\033[32;1mHow to mount a Real/Virtual CD-ROM Drive in DOSBox:\033[0m\n"
		"DOSBox provides CD-ROM emulation on several levels.\n"
		"\n"
		"The \033[33mbasic\033[0m level works on all CD-ROM drives and normal directories.\n"
		"It installs MSCDEX and marks the files read-only.\n"
		"Usually this is enough for most games:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom\033[0m   or   \033[34;1mmount d C:\\example -t cdrom\033[0m\n"
		"If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
		"\033[34;1mmount d C:\\example -t cdrom -label CDLABEL\033[0m\n"
		"\n"
		"The \033[33mnext\033[0m level adds some low-level support.\n"
		"Therefore only works on CD-ROM drives:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0\033[0m\n"
		"\n"
		"The \033[33mlast\033[0m level of support depends on your Operating System:\n"
		"For \033[1mWindows 2000\033[0m, \033[1mWindows XP\033[0m and \033[1mLinux\033[0m:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-ioctl\033[0m\n"
		"For \033[1mWindows 9x\033[0m with a ASPI layer installed:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-aspi\033[0m\n"
		"\n"
		"Replace \033[0;31mD:\\\033[0m with the location of your CD-ROM.\n"
		"Replace the \033[33;1m0\033[0m in \033[34;1m-usecd \033[33m0\033[0m with the number reported for your CD-ROM if you type:\n"
		"\033[34;1mmount -cd\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_SPECIAL",
		"\033[2J\033[32;1mSpecial keys:\033[0m\n"
		"These are the default keybindings.\n"
		"They can be changed in the \033[33mkeymapper\033[0m.\n"
		"\n"
		"\033[33;1mALT-ENTER\033[0m   : Go full screen and back.\n"
		"\033[33;1mALT-PAUSE\033[0m   : Pause DOSBox.\n"
		"\033[33;1mCTRL-F1\033[0m     : Start the \033[33mkeymapper\033[0m.\n"
		"\033[33;1mCTRL-F4\033[0m     : Update directory cache for all drives! Swap mounted disk-image.\n"
		"\033[33;1mCTRL-ALT-F5\033[0m : Start/Stop creating a movie of the screen.\n"
		"\033[33;1mCTRL-F5\033[0m     : Save a screenshot.\n"
		"\033[33;1mCTRL-F6\033[0m     : Start/Stop recording sound output to a wave file.\n"
		"\033[33;1mCTRL-ALT-F7\033[0m : Start/Stop recording of OPL commands.\n"
		"\033[33;1mCTRL-ALT-F8\033[0m : Start/Stop the recording of raw MIDI commands.\n"
		"\033[33;1mCTRL-F7\033[0m     : Decrease frameskip.\n"
		"\033[33;1mCTRL-F8\033[0m     : Increase frameskip.\n"
		"\033[33;1mCTRL-F9\033[0m     : Kill DOSBox.\n"
		"\033[33;1mCTRL-F10\033[0m    : Capture/Release the mouse.\n"
		"\033[33;1mCTRL-F11\033[0m    : Slow down emulation (Decrease DOSBox Cycles).\n"
		"\033[33;1mCTRL-F12\033[0m    : Speed up emulation (Increase DOSBox Cycles).\n"
		"\033[33;1mALT-F12\033[0m     : Unlock speed (turbo button).\n"
		);
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR","This command boots DOSBox from either a floppy or hard disk image.\n\n"
		"For this command, one can specify a succession of floppy disks swappable\n"
		"by pressing Ctrl-F4, and -l specifies the mounted drive to boot from.  If\n"
		"no drive letter is specified, this defaults to booting from the A drive.\n"
		"The only bootable drive letters are A, C, and D.  For booting from a hard\n"
		"drive (C or D), the image should have already been mounted using the\n"
		"\033[34;1mIMGMOUNT\033[0m command.\n\n"
		"The syntax of this command is:\n\n"
		"\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n"
		);
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS","Available PCjr cartridge commandos:%s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS","No PCjr cartridge commandos found");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE","Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2","Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
		"\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
		"Check that the path is correct and the image is accessible.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
		"Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED","Type \"%s\" is unsupported. Specify \"hdd\" or \"floppy\" or\"iso\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host or local drive.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");
#ifdef USE_ALT_KEYB
	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_SHOWHELP",
		"\033[32;1mKEYB\033[0m [keyboard layout ID[ codepage number[ codepage file]]]\n\n"
		"Some examples:\n"
		"  \033[32;1mKEYB\033[0m: Display currently loaded codepage.\n"
		"  \033[32;1mKEYB\033[0m sp: Load the spanish (SP) layout, use an appropriate codepage.\n"
		"  \033[32;1mKEYB\033[0m sp 850: Load the spanish (SP) layout, use codepage 850.\n"
		"  \033[32;1mKEYB\033[0m sp 850 mycp.cpi: Same as above, but use file mycp.cpi.\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");
	PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart);
#endif
#ifdef PSP
	MSG_Add("PROGRAM_INPUTMAP_MAPPED", "Key %s is mapped to button %s.\n");
	MSG_Add("PROGRAM_INPUTMAP_INVALID", "Invalid arguments.\n");
	MSG_Add("PROGRAM_INPUTMAP_USAGE", "usage: inputmap [option]\nexec <cmdline>: run cmdline with input map\nanalog <mode>: mode sets mouse or joystick\n"
		"<button> <keycode>: map button to keycode\n");

	MSG_Add("PROGRAM_SYSOPT_CLOCK", "CPU Clock is %dMhz\n");
	MSG_Add("PROGRAM_SYSOPT_USAGE", "usage: sysopt [option]\nclock <rate>: get clock rate or set clock to rate, rate must be between 200 and 333\n");
	PROGRAMS_MakeFile("INPUTMAP.COM",INPUTMAP_ProgramStart);
	PROGRAMS_MakeFile("SYSOPT.COM",SYSOPT_ProgramStart);
	if(fixup)
		PROGRAMS_MakeFile("PROF.COM",PROF_ProgramStart);
#endif

	/*regular setup*/
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM",MEM_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM",LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM",RESCAN_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM",INTRO_ProgramStart);
	PROGRAMS_MakeFile("BOOT.COM",BOOT_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);
}
