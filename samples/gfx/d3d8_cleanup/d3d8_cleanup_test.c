/* Reset failure and partial initialization can repeat resource cleanup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct resource { unsigned releases; };
typedef struct resource *LPDIRECT3DVERTEXBUFFER8;
typedef struct resource *LPDIRECT3DTEXTURE8;
typedef struct { void *tex, *vertex_buf; } d3d8_renderchain_t;
typedef struct
{
   void *renderchain_data;
   struct { void *buffer, *decl; } menu_display;
} d3d8_video_t;

static unsigned failures;
#define CHECK(x) do { if (!(x)) { failures++; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); } } while (0)
static void release(struct resource *resource)
{
   CHECK(resource != NULL);
   if (resource)
   {
      CHECK(resource->releases == 0);
      resource->releases++;
   }
}
#define IDirect3DTexture8_Release(p) release(p)
#define IDirect3DVertexBuffer8_Release(p) release(p)
#include "driver_code.inc"

int main(void)
{
   unsigned mask;
   d3d8_deinitialize(NULL);
   for (mask = 0; mask < 16; mask++)
   {
      struct resource texture = { 0 }, vertex = { 0 }, menu = { 0 }, replacement = { 0 };
      d3d8_video_t video;
      d3d8_renderchain_t *chain = NULL;
      memset(&video, 0, sizeof(video));
      if (mask & 8)
      {
         chain = (d3d8_renderchain_t*)calloc(1, sizeof(*chain));
         if (!chain)
            return 1;
         chain->tex        = (mask & 1) ? &texture : NULL;
         chain->vertex_buf = (mask & 2) ? &vertex : NULL;
         video.renderchain_data = chain;
      }
      video.menu_display.buffer = (mask & 4) ? &menu : NULL;
      video.menu_display.decl   = &menu;
      d3d8_deinitialize(&video);
      CHECK(video.renderchain_data == NULL);
      CHECK(video.menu_display.buffer == NULL && video.menu_display.decl == NULL);
      /* Restore releases resources; a failed reset releases them again. */
      d3d8_deinitialize(&video);
      CHECK(texture.releases == ((mask & 8) && (mask & 1) ? 1U : 0U));
      CHECK(vertex.releases == ((mask & 8) && (mask & 2) ? 1U : 0U));
      CHECK(menu.releases == ((mask & 4) ? 1U : 0U));
      video.menu_display.buffer = &replacement;
      d3d8_deinitialize(&video);
      d3d8_deinitialize(&video);
      CHECK(replacement.releases == 1);
   }
   if (!failures)
      puts("ok: D3D8 partial initialization, repeated cleanup and replacement resources");
   return failures ? 1 : 0;
}
