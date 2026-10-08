/* Compile-only stand-in for PSL1GHT's <rsx/nv40.h>: the raw NV40
 * method numbers. gfx/drivers/rsx_gfx.c tests for this one to pick
 * the rsxInit() of PSL1GHT since 2020-07-10, which this tree's
 * <rsx/rsx.h> declares. */
#ifndef PS3STUB_RSX_NV40_H
#define PS3STUB_RSX_NV40_H
#define NV40TCL_RENDER_ENABLE 0x00001e98
#endif
