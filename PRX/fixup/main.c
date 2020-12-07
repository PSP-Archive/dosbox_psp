#include <pspkernel.h>

PSP_MODULE_INFO("fixup", 0x1000, 1, 1);

int module_start(SceSize args, void *argp)
{
   sceKernelSetDdrMemoryProtection((void *)0x88300000, 1024*1024, 0xf);   // this is unmixed on 3.7
   *(int *)0xBC100050 &= ~1;   // kill the me so it doesn't mess with the code cache
               // do it directly to avoid 3.7 nid issues
   *(int *)0xBC000030 |= 0x300;   // allow user access to the profiler (0x5c4xxxxx)
               // hope this works on the slim
   return -1;
}