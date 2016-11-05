#ifndef GX2_H
#define GX2_H
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/displaylist.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/tessellation.h>
#include <gx2/texture.h>

#define GX2_SCAN_BUFFER_ALIGNMENT      0x1000
#define GX2_SHADER_ALIGNMENT           0x100
#define GX2_CONTEXT_STATE_ALIGNMENT    0x100
#define GX2_DISPLAY_LIST_ALIGNMENT     0x20
#define GX2_VERTEX_BUFFER_ALIGNMENT    0x40
#define GX2_INDEX_BUFFER_ALIGNMENT     0x20

#define GX2_ENABLE                     TRUE
#define GX2_DISABLE                    FALSE

#define GX2_TRUE                       TRUE
#define GX2_FALSE                      FALSE

#endif // GX2_H
