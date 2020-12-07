#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <malloc.h>
#include "dosbox.h"
#include "render.h"
#include "video.h"
#include "cpu.h"
#include "setup.h"
#include "keyboard.h"
#include "p_sprint.h"

/* Much from psp SDL */

#define PSP_SLICE_SIZE 32

struct color {
	Bit8u red;
	Bit8u green;
	Bit8u blue;
	Bit8u pad;
} __attribute__ ((packed));

struct texVertex
{
    unsigned short u, v;
    short x, y, z;
};     

static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		Bitu flags;
	} src;
	struct {
		float wscale;
		float hscale;
		Bitu pitch;
		Bitu pos;
		Bitu size;
	} dst;
	Bitu frameskip;
	bool pal_change;
	bool update;
	bool delay_update;
	bool buffer2;
	bool aspect;
	bool invert;
	struct color pal[256] __attribute__((aligned (16)));
	unsigned int list[2][256] __attribute__((aligned (16)));
	Bit8u *draw_buf;
} render_int;

extern bool autocycles, show_key_hint;
extern Bit32s CPU_CyclesMax, CPU_CycleUp, CPU_CycleDown;

ScalerLineHandler_t RENDER_DrawLine;

void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render_int.pal_change = true;
	render_int.pal[entry].red=red;
	render_int.pal[entry].green=green;
	render_int.pal[entry].blue=blue;
}

static void GuInit() {
	int psm;

	switch(render_int.src.bpp) {
	case 4:
	case 8:
	case 32:
	default:
		psm = GU_PSM_8888;
		break;
	case 16:
		psm = GU_PSM_5650;
	}
	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, psm, 1);

	sceGuInit();
	sceGuStart(GU_DIRECT, render_int.list[1]); 
	sceGuDispBuffer(480, 272, (void *)0, 512);
	sceGuClutMode(psm, 0, 255, 0);
	sceGuDrawBuffer(psm, (void *)0, 512);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);

	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

static inline int roundUpToPowerOfTwo (int x) {
    	return 1 << (32 - __builtin_allegrex_clz(x - 1));
}    

static void GuMakeList(Bit8u * src, unsigned int * list, Bitu x,Bitu y,Bitu dx,Bitu dy,bool extra,Bitu pitch) {
	unsigned short old_slice = 0; /* set when we load 2nd tex */
	unsigned int slice, num_slices, width, height, tbw, off_bytes;
	struct texVertex *vertices;
	Bit8u *pixels;
	int tpsm, off_x, hbpp = render_int.src.bpp/4;
	int dstx, dsty, dstdx, dstdy;

	// if x != 0 ever, this will have to be fixed.
	off_bytes = ((unsigned int)src + ((x*hbpp)>>1)) & 0xf;
	off_x = ((off_bytes<<1) / hbpp);
	width = roundUpToPowerOfTwo(dx + off_bytes);
	height = roundUpToPowerOfTwo(dy);
	tbw = ((pitch<<1) / hbpp);
	dstx = (int)((float)x * render_int.dst.wscale) + (render_int.aspect?60:0);
	dstdx = (int)((float)dx * render_int.dst.wscale);
	dsty = (int)((float)y * render_int.dst.hscale);
	dstdy = (int)((float)dy * render_int.dst.hscale);

	switch(hbpp) {
	case 1:
		tpsm = GU_PSM_T4;
		break;
	case 2:
	default:
		tpsm = GU_PSM_T8;
		break;
	case 4:
		tpsm = GU_PSM_5650;
		break;
	case 8:
		tpsm = GU_PSM_8888;
		break;
	}
	
	/* Align the texture prior to srcrect->x */
	pixels = (Bit8u *)((unsigned int)src + (((x - off_x) * hbpp)>>1));  // src always points to the top of the rect
	num_slices = (dx + (PSP_SLICE_SIZE - 1)) / PSP_SLICE_SIZE;

	off_x += extra;

	/* GE doesn't appreciate textures wider than 512 */
	if (width > 512)
		width = 512; 

	sceGuStart(GU_SEND,list);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(tpsm,0,0,0);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexImage(0, width, height, tbw, pixels);
	sceGuTexSync();

	for (slice = 0; slice < num_slices; slice++) {

		vertices = (struct texVertex*)sceGuGetMemory(2 * sizeof(struct texVertex));

		if ((slice * PSP_SLICE_SIZE) < width) {
			vertices[0].u = slice * PSP_SLICE_SIZE + off_x;
		} else {
			if (!old_slice) {
				/* load another texture (src width > 512) */
				pixels += (width * hbpp)>>1;
				sceGuTexImage(0, roundUpToPowerOfTwo(dx - width), height, tbw, pixels);
				sceGuTexSync();
				old_slice = slice;
			}
			vertices[0].u = (slice - old_slice) * PSP_SLICE_SIZE + off_x;
		}
		vertices[1].u = vertices[0].u + PSP_SLICE_SIZE;
		if (vertices[1].u > (dx + off_x))
			vertices[1].u = dx + off_x;

		vertices[0].v = 0;
		vertices[1].v = vertices[0].v + dy;

		if(render_int.invert) {
			vertices[0].x = (dstdx+dstx) - slice * PSP_SLICE_SIZE * dstdx / dx; 
			vertices[1].x = vertices[0].x - PSP_SLICE_SIZE * dstdx / dx;
			if (vertices[0].x < dstx)
				vertices[0].x = dstx;

			vertices[1].y = dsty;
			vertices[0].y = vertices[1].y + dstdy;
		} else {
			vertices[0].x = dstx + slice * PSP_SLICE_SIZE * dstdx / dx; 
			vertices[1].x = vertices[0].x + PSP_SLICE_SIZE * dstdx / dx;
			if (vertices[1].x > (dstx + dstdx))
				vertices[1].x = dstx + dstdx;

			vertices[0].y = dsty;
			vertices[1].y = vertices[0].y + dstdy;
		}
		vertices[0].z = 0;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,vertices);
	}

	sceGuFinish();
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps, double ratio, bool dblw, bool dblh) {
	if ((!width) || (!height))  return;
	render_int.src.width=width;
	render_int.src.height=height;
	render_int.src.bpp=bpp;
	render_int.dst.wscale=(render_int.aspect?360.0f:480.0f)/(float)width;
	render_int.dst.hscale=272.0f/(float)height;
 	render_int.dst.pitch = ((width + 15) & ~15)*(bpp/8);
	render_int.dst.size = (height*render_int.dst.pitch);
	render_int.draw_buf = (Bit8u *)(((Bit32u)sceGeEdramGetAddr()+sceGeEdramGetSize()-(render_int.dst.size*2))&~15);
	render_int.buffer2 = false;
	render_int.update = false;
	GuInit();
	GuMakeList(render_int.draw_buf, render_int.list[0], 0, 0, render_int.src.width, render_int.src.height, 0, render_int.dst.pitch);
	GuMakeList(&render_int.draw_buf[render_int.dst.size], render_int.list[1], 0, 0, render_int.src.width, render_int.src.height, 0, render_int.dst.pitch);
}

static void RENDER_CopyLine(Bitu vidstart, Bitu line, VGA_Line_Handler handler) {
	if(!render_int.update || (render_int.dst.pos >= render_int.dst.size)) return;
	handler(vidstart, line, &render_int.draw_buf[render_int.dst.pos + (render_int.buffer2?render_int.dst.size:0)]);
	render_int.dst.pos+=render_int.dst.pitch;
}

static void IncreaseFrameSkip(void) {
	if (render_int.frameskip<10) render_int.frameskip++;
	LOG_MSG("Frame Skip at %d",render_int.frameskip);
}

static void DecreaseFrameSkip(void) {
	if (render_int.frameskip>0) render_int.frameskip--;
	LOG_MSG("Frame Skip at %d",render_int.frameskip);
}

static SceUID render_event, render_thid;
static unsigned int *hint_list, *vga_list;

static int RenderThread(SceSize args, void *argp) {
	static PspGeContext ge_context;
	while(1) {
		sceKernelSleepThread();
		sceDisplayWaitVblankStart();
		sceKernelDcacheWritebackAll();
		if (render_int.pal_change && (render_int.src.bpp <= 8)) {
			static unsigned int pallist[8] __attribute__((aligned (16)));
			sceGuStart(GU_DIRECT,pallist);
			sceGuClutLoad(render_int.src.bpp*4, render_int.pal);
			sceGuFinish();
			render_int.pal_change = false;
		}
		sceGuSendList(GU_TAIL, vga_list, &ge_context);
		if(hint_list) sceGuSendList(GU_TAIL, hint_list, &ge_context);
		sceGuSync(0,0);
		sceKernelSetEventFlag(render_event, 1);
	}
}

void RENDER_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);

        render_int.frameskip=section->Get_int("frameskip");
	render_int.aspect=section->Get_bool("aspect");
	render_int.invert=section->Get_bool("invert");
	render_int.pal_change = true;
	render_int.draw_buf = NULL;
	render_int.update = false;
	render_int.buffer2 = false;
	render_int.delay_update = false;
	render_event = sceKernelCreateEventFlag("render_event", 0, 1, NULL);
	render_thid = sceKernelCreateThread("render_thread", &RenderThread, 20, 512, PSP_THREAD_ATTR_USER, NULL); 
	if(render_thid) sceKernelStartThread(render_thid, 0, NULL);
	memset(render_int.pal, '\0', sizeof(struct color) * 256);
}

static void RENDER_EmptyLineHandler(Bitu vidstart, Bitu line, VGA_Line_Handler handler) {}

bool RENDER_StartUpdate() {
	static Bitu prev_vcount = 0, count = 0;
	Bitu vcount;

	if (render_int.update) return false;
	vcount = sceDisplayGetVcount();
	
	// wraparound?
	if(((Bits)vcount >= 0) && ((Bits)prev_vcount < 0)) prev_vcount = 0;
	
	if (vcount == prev_vcount) { 
		if (autocycles && (CPU_CycleMax <= (CPU_CycleUp - 100))) CPU_CycleMax += 100;
		return false;
	}
	if (autocycles && ((vcount - prev_vcount) > 1) && (CPU_CycleMax >= (CPU_CycleDown + 100))) CPU_CycleMax -= 100;

	prev_vcount = vcount;
	count++;
	if (count <= render_int.frameskip) return false;
	count = 0;
	render_int.dst.pos = 0;
	if (render_int.delay_update) {
		if(sceKernelPollEventFlag(render_event, 1, PSP_EVENT_WAITCLEAR, NULL) == 0) {
			sceKernelWakeupThread(render_thid);
			render_int.buffer2 = !render_int.buffer2;
			render_int.delay_update = false;
		}
		return false;
	}
	render_int.update = true;
	RENDER_DrawLine = RENDER_CopyLine;
	return true;
}

void RENDER_EndUpdate() {
	if (!render_int.update) return;
	hint_list = (show_key_hint?p_spGetHintList(render_int.src.bpp):NULL);
	vga_list = render_int.list[render_int.buffer2]; 
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	if(sceKernelPollEventFlag(render_event, 1, PSP_EVENT_WAITCLEAR, NULL) < 0) 
		render_int.delay_update = true;
	else {
		sceKernelWakeupThread(render_thid);
		render_int.buffer2 = !render_int.buffer2;
	}
	render_int.update = false;
}
