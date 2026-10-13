/* Hermetic compile-only stand-in: only the names libretro-common uses. */
#ifndef STUB_ctr_3ds_services_cfgu_h
#define STUB_ctr_3ds_services_cfgu_h
#include <stdint.h>
#include <3ds/types.h>
Result cfguInit(void);
void cfguExit(void);
Result CFGU_GetSystemModel(uint8_t *model);
#endif
