/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* using code the from GPU example in crtulib */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "ctr_blit_shader_shbin.h"


#include "../../general.h"
#include "../../driver.h"
#include "../video_viewport.h"
#include "../video_monitor.h"

#include "retroarch.h"

typedef struct ctr_video
{
   bool rgb32;
   bool vsync;
   bool smooth;
   bool menu_texture_enable;
   unsigned rotation;
} ctr_video_t;

DVLB_s*         dvlb;
shaderProgram_s shader;
u32*            texData;
u32*            texData2;

//GPU framebuffer address
u32*            gpuOut = (u32*)0x1F119400;
//GPU depth buffer address
u32*            gpuDOut = (u32*)0x1F370800;

typedef struct
{
   struct
   {
      float x, y, z;
   } position;
   float texcoord[2];
} vertex_s;


u32  gpuCmdSize;
u32* gpuCmd;
u32* gpuCmdRight;


u32* texture_bin;

#define tex_w 512
#define tex_h 512
#define gpu_tex_w 512
#define gpu_tex_h 512

#define TEX_MAKE_SIZE(W,H)    (((u32)(W))|((u32)(H)<<16))
#define tex_size              TEX_MAKE_SIZE(tex_w, tex_h)
#define gpu_tex_size          TEX_MAKE_SIZE(gpu_tex_w, gpu_tex_h)

#define texture_bin_size      (tex_w * tex_h * sizeof(*texture_bin))
#define gpu_texture_bin_size  (gpu_tex_w * gpu_tex_h * 4)

#define fbwidth  400
#define fbheight 240

#define CTR_MATRIX(X0,Y0,Z0,W0,X1,Y1,Z1,W1,X2,Y2,Z2,W2,X3,Y3,Z3,W3) {W0,Z0,Y0,X0,W1,Z1,Y1,X1,W2,Z2,Y2,X2,W3,Z3,Y3,X3}

float proj_m[16] = CTR_MATRIX
                   (
                      0.0,           -2.0 / fbheight,  0.0,     1.0,
                      -2.0 / fbwidth,  0.0,            0.0,     1.0,
                      0.0,           0.0,            1.0,     0.0,
                      1.0 / gpu_tex_w,             -1.0 / gpu_tex_h,           1.0,     1.0
                   );




const vertex_s  modelVboData[] =
{
   {{  40 + 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
   {{  40 + 320,  0.0f, -1.0f}, {320, 0.0f}},
   {{  40 + 320,  tex_h, -1.0f}, {320, tex_h}},

   {{  40 + 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
   {{  40 + 320,  tex_h, -1.0f}, {320, tex_h}},
   {{  40 + 0.0f,  tex_h, -1.0f}, {0.0f, tex_h}}
};

void* vbo_buffer;

//stolen from staplebutt
void GPU_SetDummyTexEnv(u8 num)
{
   GPU_SetTexEnv(num,
                 GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
                 GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_REPLACE,
                 GPU_REPLACE,
                 0xFFFFFFFF);
}

// topscreen
void renderFrame()
{
   GPU_SetViewport((u32*)osConvertVirtToPhys((u32)gpuDOut),
                   (u32*)osConvertVirtToPhys((u32)gpuOut), 0, 0, fbheight * 2, fbwidth);

   GPU_DepthMap(-1.0f, 0.0f);
   GPU_SetFaceCulling(GPU_CULL_NONE);
   GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
   GPU_SetStencilOp(GPU_KEEP, GPU_KEEP, GPU_KEEP);
   GPU_SetBlendingColor(0, 0, 0, 0);
   //      GPU_SetDepthTestAndWriteMask(true, GPU_GREATER, GPU_WRITE_ALL);
   GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_ALL);
   //   GPU_SetDepthTestAndWriteMask(true, GPU_ALWAYS, GPU_WRITE_ALL);

   GPUCMD_AddMaskedWrite(GPUREG_0062, 0x1, 0);
   GPUCMD_AddWrite(GPUREG_0118, 0);

   GPU_SetAlphaBlending(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                        GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
   GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

   GPU_SetTextureEnable(GPU_TEXUNIT0);

   GPU_SetTexEnv(0,
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR),
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_MODULATE, GPU_MODULATE,
                 0xFFFFFFFF);
   GPU_SetDummyTexEnv(1);
   GPU_SetDummyTexEnv(2);
   GPU_SetDummyTexEnv(3);
   GPU_SetDummyTexEnv(4);
   GPU_SetDummyTexEnv(5);

   //texturing stuff
   GPU_SetTexture(GPU_TEXUNIT0, (u32*)osConvertVirtToPhys((u32)texData), gpu_tex_w, gpu_tex_h,
                  GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR),
                  GPU_RGBA4);

   u32 bufferoffset = 0x00000000;
   u64 bufferpermutations = 0x210;
   u8 numattributes = 2;
   GPU_SetAttributeBuffers(3, (u32*)osConvertVirtToPhys((u32)vbo_buffer),
                           GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 2, GPU_FLOAT),
                           0xFFC, 0x210, 1, &bufferoffset, &bufferpermutations, &numattributes);

   GPU_DrawArray(GPU_TRIANGLES, sizeof(modelVboData) / sizeof(vertex_s));

   GPU_FinishDrawing();
}

void gpu_init_calls(void)
{
   GPU_Init(NULL);
   gpuCmdSize = 0x40000;
   gpuCmd = (u32*)linearAlloc(gpuCmdSize * 4);
   gpuCmdRight = (u32*)linearAlloc(gpuCmdSize * 4);
   GPU_Reset(NULL, gpuCmd, gpuCmdSize);
   dvlb = DVLB_ParseFile((u32*)ctr_blit_shader_shbin, ctr_blit_shader_shbin_size);
   shaderProgramInit(&shader);
   shaderProgramSetVsh(&shader, &dvlb->DVLE[0]);
   shaderProgramUse(&shader);

   // Flush the command buffer so that the shader upload gets executed
   GPUCMD_Finalize();
   GPUCMD_FlushAndRun(NULL);
   gspWaitForP3D();

   //create texture
   texData = (u32*)linearMemAlign(gpu_texture_bin_size,
                                  0x80); //textures need to be 0x80-byte aligned

   texData2 = (u32*)linearMemAlign(texture_bin_size,
                                   0x80); //textures need to be 0x80-byte aligned

   memcpy(texData2, texture_bin, texture_bin_size);
   vbo_buffer = linearAlloc(sizeof(modelVboData));
   memcpy(vbo_buffer, (void*)modelVboData, sizeof(modelVboData));
}

#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b["X";"Y"H"

static void* ctr_init(const video_info_t* video,
                      const input_driver_t** input, void** input_data)
{
   void* ctrinput = NULL;
   global_t* global         = global_get_ptr();
   ctr_video_t* ctr        = (ctr_video_t*)calloc(1, sizeof(ctr_video_t));

   if (!ctr)
      return NULL;

   printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);

//   gfxInitDefault();
//   gfxSet3D(false);
   texture_bin = (typeof(texture_bin))malloc(texture_bin_size);
   int i, j;
   for (j = 0; j < tex_h; j++)
   {
      for (i = 0; i < tex_w; i++)
      {
         if ((i & 0x8) || (j & 0x8))
            texture_bin[i + j * tex_w] =  0x0000FFFF;
         else
            texture_bin[i + j * tex_w] =  0xFFFFFFFF;

         if (i > 64)
            texture_bin[i + j * tex_w] =  0xFF0000FF;

      }

   }

   gpu_init_calls();

   if (input && input_data)
   {
      ctrinput = input_ctr.init();
      *input = ctrinput ? &input_ctr : NULL;
      *input_data = ctrinput;
   }

   printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);
   return ctr;
}

static bool ctr_frame(void* data, const void* frame,
                      unsigned width, unsigned height, unsigned pitch, const char* msg)
{
   ctr_video_t* ctr = (ctr_video_t*)data;
   settings_t* settings = config_get_ptr();

//   int i;
   static int frames = 0;

   if (!width || !height)
   {
      gspWaitForEvent(GSPEVENT_VBlank0, true);
      return true;
   }

   if(!aptMainLoop())
   {
      rarch_main_command(RARCH_CMD_QUIT);
      return true;
   }

   extern bool select_pressed;
   if (select_pressed)
   {
      rarch_main_command(RARCH_CMD_QUIT);
      return true;
   }

   hidScanInput();
   u32 kDown = hidKeysDown();
   if (kDown & KEY_START)
   {
      rarch_main_command(RARCH_CMD_QUIT);
      return true;
   }


   printf("frames: %i\r", frames++);fflush(stdout);
//   gfxFlushBuffers();
//   gspWaitForEvent(GSPEVENT_VBlank0, true);

   u32 backgroundColor = 0x00000000;

//   hidScanInput();
//   u32 kDown = hidKeysDown();

//   if (kDown & KEY_START) rarch_main_command(RARCH_CMD_QUIT); // break in order to return to hbmenu

   GPUCMD_SetBufferOffset(0);
   GPU_SetFloatUniform(GPU_VERTEX_SHADER,
                       shaderInstanceGetUniformLocation(shader.vertexShader, "projection"),
                       (u32*)proj_m, 4);



   GSPGPU_FlushDataCache(NULL, (u8*)texData2, texture_bin_size);
   GX_SetDisplayTransfer(NULL, (u32*)texData2, tex_size, (u32*)texData, gpu_tex_size,
                         0x3302); // rgb32=0x0 rgb32=0x0 ??=0x0 linear2swizzeled=0x2
   gspWaitForPPF();

   renderFrame();
   GPUCMD_Finalize();

//   for (i = 0; i < 16; i++)
//      printf(PRINTFPOS_STR("%i", "%i")"%f", i >> 2, 10 * (i & 0x3), proj_m[i]);

//   printf(PRINTFPOS(20, 10)"frames: %i", frames++);

   //draw the frame
   GPUCMD_FlushAndRun(NULL);
   gspWaitForP3D();

   //clear the screen
   GX_SetDisplayTransfer(NULL, (u32*)gpuOut, 0x019001E0,
                         (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019001E0, 0x01001000);
   gspWaitForPPF();

   //clear the screen
   GX_SetMemoryFill(NULL, (u32*)gpuOut, backgroundColor, (u32*)&gpuOut[0x2EE00],
                    0x201, (u32*)gpuDOut, 0x00000000, (u32*)&gpuDOut[0x2EE00], 0x201);
   gspWaitForPSC0();
   gfxSwapBuffersGpu();

   gspWaitForEvent(GSPEVENT_VBlank0, true);


   //      gfxFlushBuffers();
   //      gfxSwapBuffers();

   return true;
}

static void ctr_set_nonblock_state(void* data, bool toggle)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->vsync = !toggle;
}

static bool ctr_alive(void* data)
{
   (void)data;
   return true;
}

static bool ctr_focus(void* data)
{
   (void)data;
   return true;
}

static bool ctr_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool ctr_has_windowed(void* data)
{
   (void)data;
   return false;
}

static void ctr_free(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);
   shaderProgramFree(&shader);
   DVLB_Free(dvlb);
   linearFree(gpuCmd);
   linearFree(gpuCmdRight);
   linearFree(texData);
   linearFree(texData2);
   linearFree(vbo_buffer);
   free(texture_bin);
//   gfxExit();

   free(ctr);
}

static void ctr_set_texture_frame(void* data, const void* frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   int i;
   uint8_t* dst = (uint8_t*)texData2;
   const uint8_t* src = frame;
   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, width*2);
      dst += tex_w*2;
      src += width*2;
   }



}

static void ctr_set_texture_enable(void* data, bool state, bool full_screen)
{
   (void) full_screen;

   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->menu_texture_enable = state;
}


static void ctr_set_rotation(void* data, unsigned rotation)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   ctr->rotation = rotation;
}
static void ctr_set_filtering(void* data, unsigned index, bool smooth)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->smooth = smooth;
}

static void ctr_set_aspect_ratio(void* data, unsigned aspectratio_index)
{
   (void)data;
   (void)aspectratio_index;
   return;
}

static void ctr_apply_state_changes(void* data)
{
   (void)data;
   return;
}

static void ctr_viewport_info(void* data, struct video_viewport* vp)
{
   (void)data;
   return;
}

static const video_poke_interface_t ctr_poke_interface =
{
   NULL,
   ctr_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
#ifdef HAVE_FBO
   NULL,
#endif
   NULL,
   ctr_set_aspect_ratio,
   ctr_apply_state_changes,
#ifdef HAVE_MENU
   ctr_set_texture_frame,
   ctr_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void ctr_get_poke_interface(void* data,
                                   const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &ctr_poke_interface;
}

static bool ctr_read_viewport(void* data, uint8_t* buffer)
{
   (void)data;
   (void)buffer;
   return false;
}

static bool ctr_set_shader(void* data,
                           enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_ctr =
{
   ctr_init,
   ctr_frame,
   ctr_set_nonblock_state,
   ctr_alive,
   ctr_focus,
   ctr_suppress_screensaver,
   ctr_has_windowed,
   ctr_set_shader,
   ctr_free,
   "ctr",

   ctr_set_rotation,
   ctr_viewport_info,
   ctr_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
   ctr_get_poke_interface
};
