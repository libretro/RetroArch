#ifndef KERNEL_FUNCTIONS_PRX_H
#define KERNEL_FUNCTIONS_PRX_H

#include <pspkerneltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int read_system_buttons(void);

void exitspawn_kernel( const char* fileName, SceSize args, void * argp);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEMBUTTONS_PRX_H */
