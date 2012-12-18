#ifndef _DOLLOADER_H_
#define _DOLLOADER_H_

typedef void (*entrypoint) (void);

u32 load_dol_image(const void *dolstart);

#endif
