.set noreorder

#include "pspimport.s"

IMPORT_START	"sceAudio_driver",0x00010000
IMPORT_FUNC	"sceAudio_driver",0xE0727056,sceAudio_driver_E0727056 
IMPORT_FUNC	"sceAudio_driver",0x38553111,sceAudio_driver_38553111 
