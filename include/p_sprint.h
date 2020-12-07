
#include <pspirkeyb.h>
#ifdef __cplusplus
extern "C" {
#endif
int p_spReadKey(SIrKeybScanCodeData * newKey, unsigned int Buttons);
unsigned int *p_spGetHintList(int bpp);
#ifdef __cplusplus
}
#endif
