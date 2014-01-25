/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Tobias Jakobi
 *  Copyright (C) 2013-2014 - Daniel Mehrwald
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

#include <stdlib.h>
#include <string.h>
#include <limare.h>
#include <GLES2/gl2.h>

#include "../general.h"
#include "gfx_common.h"
#include "fonts/fonts.h"

/* Rename to LIMA_GFX_DEBUG to enable debugging code.*/
#define NO_LIMA_GFX_DEBUG 1

/* Current limare only natively supports a limited amount of formats for texture  *
 * data. We compensate for this limitation by swizzling the texture data in the   *
 * pixel shader.                                                                  */

#define LIMA_TEXEL_FORMAT_BGR_565           0x0e
#define LIMA_TEXEL_FORMAT_RGBA_5551         0x0f
#define LIMA_TEXEL_FORMAT_RGBA_4444         0x10
#define LIMA_TEXEL_FORMAT_RGBA_8888         0x16

/* Limare is currently unable to deallocate individual texture objects and *
 * only allows to destroy all objects at once.                             *
 * We only create a maximum of 12 objects, before doing a full "reset", or *
 * sooner, under the condition that limare's texture memory runs out.      */
static const unsigned num_max_textures = 12;

typedef struct limare_state limare_state_t;

typedef struct limare_texture {
  unsigned width;
  unsigned height;

  int handle;
  unsigned format;

  bool rgui;
} limare_texture_t;

typedef struct vec2f {
  float x,y;
} vec2f_t;

typedef struct vec3f {
  float x, y, z;
} vec3f_t;

/* Create three shader programs. One is for displaying only the emulator core pixel data. *
 * The other two are for displaying the RGUI, where the pixel data can be provided in     *
 * two different formats. Current RetroArch only seems to ever use a single format, but   *
 * this is not set in stone, therefore making two programs necessary.                     */

typedef struct limare_data {
  limare_state_t *state;

  int program;

  int program_rgui_rgba16;
  int program_rgui_rgba32;

  float screen_aspect;
  float frame_aspect;

  unsigned upload_format;
  unsigned upload_bpp; /* bytes per pixel */

  vec3f_t *vertices;
  vec2f_t *coords;

  /* Generic buffer to create contiguous pixel data for limare
   * or to use for font blitting. */
  void *buffer;
  unsigned buffer_size;

  limare_texture_t **textures;
  unsigned texture_slots;
  limare_texture_t *cur_texture;
  limare_texture_t *cur_texture_rgui;
} limare_data_t;

/* Header for simple vertex shader. */
static const char *vshader_src =
  "attribute vec4 in_vertex;\n"
  "attribute vec2 in_coord;\n"
  "\n"
  "varying vec2 coord;\n"
  "\n"
  "void main()\n"
  "{\n"
  "    gl_Position = in_vertex;\n"
  "    coord = in_coord;\n"
  "}\n";

/* Header for simple fragment shader. */
static const char *fshader_header_src =
  "precision highp float;\n"
  "\n"
  "varying vec2 coord;\n"
  "\n"
  "uniform sampler2D in_texture;\n"
  "\n";

/* Main (template) for simple fragment shader. */
static const char *fshader_main_src =
  "void main()\n"
  "{\n"
  "    vec3 pixel = texture2D(in_texture, coord)%s;\n"
  "    gl_FragColor = vec4(pixel, 1.0);\n"
  "}\n";

/* Header for RGUI fragment shader. */
/* Use mediump, which makes uColor into a (single-precision) float[4]. */
static const char *fshader_rgui_header_src =
  "precision mediump float;\n"
  "\n"
  "varying vec2 coord;\n"
  "uniform vec4 uColor;\n"
  "\n"
  "uniform sampler2D in_texture;\n"
  "\n";

/* Main (template) for RGUI fragment shader. */
static const char *fshader_rgui_main_src =
  "void main()\n"
  "{\n"
  "    vec4 pixel = texture2D(in_texture, coord)%s;\n"
  "    gl_FragColor = pixel * uColor;\n"
  "}\n";

float get_screen_aspect(limare_state_t *state) {
  unsigned w = 0, h = 0;

  limare_buffer_size(state, &w, &h);

  if (w != 0 && h != 0) {
    return (float)w / (float)h;
  }

  return 0.0f;
}

void apply_aspect(limare_data_t *pdata, float ratio) {
  vec3f_t *vertices = pdata->vertices;
  float x, y;

  if (fabsf(pdata->screen_aspect - pdata->frame_aspect) < 0.0001f) {
    x = 1.0f;
    y = 1.0f;
  } else {
    if (pdata->screen_aspect > pdata->frame_aspect) {
      x = pdata->frame_aspect / pdata->screen_aspect;
      y = 1.0f;
    } else {
      x = 1.0f;
      y = pdata->screen_aspect / pdata->frame_aspect;
    }
  }

  /* TODO: use ratio parameter */

  vertices[0].x = vertices[2].x = -x;
  vertices[1].x = vertices[3].x =  x;

  vertices[0].y = vertices[1].y = -y;
  vertices[2].y = vertices[3].y =  y;
}

int destroy_textures(limare_data_t *pdata) {
  unsigned i;
  int ret;

  pdata->cur_texture = NULL;
  pdata->cur_texture_rgui = NULL;

  for (i = 0; i < pdata->texture_slots; ++i) {
    free(pdata->textures[i]);
    pdata->textures[i] = NULL;
  }

  ret = limare_texture_cleanup(pdata->state);
  pdata->texture_slots = 0;

  return ret;
}

static limare_texture_t *get_texture_handle(limare_data_t *pdata,
                            unsigned width, unsigned height, unsigned format) {
  unsigned i;

  format = (format == 0) ? pdata->upload_format : format;

  for (i = 0; i < pdata->texture_slots; ++i) {
    if (pdata->textures[i]->width == width &&
        pdata->textures[i]->height == height &&
        pdata->textures[i]->format == format) return pdata->textures[i];
  }

  if (pdata->texture_slots == num_max_textures) {
    /* All texture slots are used, do a reset. */
    if (destroy_textures(pdata)) {
      RARCH_ERR("video_lima: failed to reset texture storage\n");
    }
  }

  return NULL;
}

static limare_texture_t *add_texture(limare_data_t *pdata,
                            unsigned width, unsigned height,
                            const void *pixels, unsigned format) {
  int texture = -1;
  unsigned retries = 2;
  const unsigned i = pdata->texture_slots;

  format = (format == 0) ? pdata->upload_format : format;

  /* limare_texture_upload returns -1 when the upload fails for some reason. */
  while (texture == -1 && retries > 0) {
    texture = limare_texture_upload(pdata->state, pixels, width, height, format, 0);

    if (texture != -1) break;
    
    destroy_textures(pdata);
    retries--;
  }

  if (texture == -1) return NULL;

  pdata->textures[i] = calloc(1, sizeof(limare_texture_t));

  pdata->textures[i]->width = width;
  pdata->textures[i]->height = height;
  pdata->textures[i]->handle = texture;
  pdata->textures[i]->format = format;

  pdata->texture_slots++;

  return pdata->textures[i];
}

static const void *make_contiguous(limare_data_t *pdata,
                                   unsigned width, unsigned height,
                                   const void *pixels, unsigned bpp,
                                   unsigned pitch) {
  unsigned i;
  unsigned full_pitch;

  bpp = (bpp == 0) ? pdata->upload_bpp : bpp;
  full_pitch = width * bpp;

  if (full_pitch == pitch) return pixels;

  RARCH_LOG("video_lima: input buffer not contiguous\n");

  /* Enlarge our buffer, if it is currently too small. */
  if (pdata->buffer_size < full_pitch * height) {
    free(pdata->buffer);
    pdata->buffer = NULL;

    pdata->buffer = malloc(full_pitch * height);
    if (pdata->buffer == NULL) {
      RARCH_ERR("video_lima: failed to allocate buffer to make pixel data contiguous\n");
      return NULL;
    }
  }

  for (i = 0; i < height; ++i) {
    memcpy(pdata->buffer + i * full_pitch, pixels + i * pitch, full_pitch);
  }

  return pdata->buffer;
}

#ifdef LIMA_GFX_DEBUG
void print_status(limare_data_t *pdata) {
  unsigned i;

  RARCH_LOG("video_lima: upload format = 0x%x, upload bpp = %u\n", pdata->upload_format, pdata->upload_bpp);
  RARCH_LOG("video_lima: buffer at %p, buffer size = %u\n", pdata->buffer, pdata->buffer_size);
  RARCH_LOG("video_lima: used texture slots = %u (from %u)\n", pdata->texture_slots, num_max_textures);

  for (i = 0; i < pdata->texture_slots; ++i) {
    RARCH_LOG("video_lima: texture slot %u, width = %u, height = %u, handle = %u, format = 0x%x\n",
              i, pdata->textures[i]->width, pdata->textures[i]->height,
              pdata->textures[i]->handle, pdata->textures[i]->format);
  }
}
#endif

void destroy_data(limare_data_t *pdata) {
  free(pdata->vertices);
  free(pdata->coords);
}

static int setup_data(limare_data_t *pdata) {
  static const unsigned num_verts = 4;
  static const unsigned num_coords = 4 * 4;
  unsigned i;

  static const vec3f_t vertices[4] = {
    {-1.0f, -1.0f,  0.0f},
    { 1.0f, -1.0f,  0.0f},
    {-1.0f,  1.0f,  0.0f},
    { 1.0f,  1.0f,  0.0f}
  };

  static const vec2f_t coords[16] = {
	  {0.0f, 1.0f}, {1.0f, 1.0f}, /*  0 degrees */
	  {0.0f, 0.0f}, {1.0f, 0.0f},
	  {0.0f, 0.0f}, {0.0f, 1.0f}, /* 90 degrees */
	  {1.0f, 0.0f}, {1.0f, 1.0f},
	  {1.0f, 0.0f}, {0.0f, 0.0f}, /* 180 degrees */
	  {1.0f, 1.0f}, {0.0f, 1.0f},
	  {1.0f, 1.0f}, {1.0f, 0.0f}, /* 270 degrees */
	  {0.0f, 1.0f}, {0.0f, 0.0f}
  };

  pdata->vertices = calloc(num_verts, sizeof(vec3f_t));
  if (pdata->vertices == NULL) goto fail;

  pdata->coords = calloc(num_coords, sizeof(vec2f_t));
  if (pdata->coords == NULL) goto fail;

  for (i = 0; i < num_verts; ++i) {
    pdata->vertices[i] = vertices[i];
  }
  
  for (i = 0; i < num_coords; ++i) {
    pdata->coords[i] = coords[i];
  }

  return 0;

fail:
  return -1;
}

static int create_programs(limare_data_t *pdata) {
  char tmpbufm[1024]; /* temp buffer for main function */
  char tmpbuf[1024]; /* temp buffer for whole program */

  const char* swz = (pdata->upload_bpp == 4) ? ".bgr" : ".rgb";

  /* Create shader program for regular operation first. */
  pdata->program = limare_program_new(pdata->state);
  if (pdata->program < 0) goto fail;

  snprintf(tmpbufm, 1024, fshader_main_src, swz);
  strncpy(tmpbuf, fshader_header_src, 1024);
  strcat(tmpbuf, tmpbufm);

  if (vertex_shader_attach(pdata->state, pdata->program, vshader_src)) goto fail;
  if (fragment_shader_attach(pdata->state, pdata->program, tmpbuf)) goto fail;
  if (limare_link(pdata->state)) goto fail;

  /* Create shader program for RGUI with RGBA4444 pixel data. */
  pdata->program_rgui_rgba16 = limare_program_new(pdata->state);
  if (pdata->program_rgui_rgba16 < 0) goto fail;

  snprintf(tmpbufm, 1024, fshader_rgui_main_src, ".abgr");
  strncpy(tmpbuf, fshader_rgui_header_src, 1024);
  strcat(tmpbuf, tmpbufm);

  if (vertex_shader_attach(pdata->state, pdata->program_rgui_rgba16, vshader_src)) goto fail;
  if (fragment_shader_attach(pdata->state, pdata->program_rgui_rgba16, tmpbuf)) goto fail;
  if (limare_link(pdata->state)) goto fail;

  /* Create shader program for RGUI with RGBA8888 pixel data. */
  pdata->program_rgui_rgba32 = limare_program_new(pdata->state);
  if (pdata->program_rgui_rgba32 < 0) goto fail;

  snprintf(tmpbufm, 1024, fshader_rgui_main_src, ".abgr");
  strncpy(tmpbuf, fshader_rgui_header_src, 1024);
  strcat(tmpbuf, tmpbufm);

  if (vertex_shader_attach(pdata->state, pdata->program_rgui_rgba32, vshader_src)) goto fail;
  if (fragment_shader_attach(pdata->state, pdata->program_rgui_rgba32, tmpbuf)) goto fail;
  if (limare_link(pdata->state)) goto fail;

  return 0;

fail:
  return -1;
}

typedef struct lima_video {
  limare_data_t *lima;

  void *font;
  const font_renderer_driver_t *font_driver;
  uint8_t font_rgb[4];

  unsigned bytes_per_pixel;

  /* current dimensions */
  unsigned width;
  unsigned height;

  /* RGUI data */
  void *rgui_buffer;
  int rgui_rotation;
  float rgui_alpha;
  bool rgui_active;
  bool rgui_rgb32;

  bool aspect_changed;

} lima_video_t;

static void lima_gfx_free(void *data) {
  lima_video_t *vid = data;
  if (!vid) return;

  if (vid->lima && vid->lima->state) limare_finish(vid->lima->state);
  if (vid->font) vid->font_driver->free(vid->font);

  destroy_data(vid->lima);
  destroy_textures(vid->lima);
  free(vid->lima->textures);

  free(vid->rgui_buffer);
  free(vid->lima);
  free(vid);
}

static void *lima_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data) {
  lima_video_t *vid = NULL;
  limare_data_t *lima = NULL;
  void *lima_input = NULL;
  struct limare_windowsys_drm limare_config = { 0 };

  vid = calloc(1, sizeof(lima_video_t));
  if (!vid) return NULL;

  vid->rgui_alpha = 1.0f;

  lima = calloc(1, sizeof(limare_data_t));
  if (!lima) return NULL;

  vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

  /* Request the Exynos DRM backend for rendering. */
  limare_config.type = LIMARE_WINDOWSYS_DRM;
  limare_config.connector_index = g_settings.video.monitor_index;

  lima->state = limare_init(&limare_config);

  if (!lima->state) {
    RARCH_ERR("video_lima: limare initialization failed\n");
    goto fail;
  }

  limare_buffer_clear(lima->state);

  if (limare_state_setup(lima->state, g_settings.video.fullscreen_x,
                         g_settings.video.fullscreen_y, 0xff000000)) {
    RARCH_ERR("video_lima: limare state setup failed\n");
    goto fail_lima;
  }

  lima->screen_aspect = get_screen_aspect(lima->state);

  lima->upload_format = (vid->bytes_per_pixel == 4) ?
    LIMA_TEXEL_FORMAT_RGBA_8888 : LIMA_TEXEL_FORMAT_BGR_565;
  lima->upload_bpp = vid->bytes_per_pixel;

  limare_enable(lima->state, GL_DEPTH_TEST);
  limare_depth_func(lima->state, GL_ALWAYS);
  limare_depth_mask(lima->state, GL_TRUE);

  limare_enable(lima->state, GL_CULL_FACE);

  limare_blend_func(lima->state, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (setup_data(lima)) {
    RARCH_ERR("video_lima: data setup failed\n");
    goto fail_lima;
  }

  if (create_programs(lima)) {
    RARCH_ERR("video_lima: creating shader programs failed\n");
    goto fail_lima;
  }

  lima->textures = calloc(num_max_textures, sizeof(limare_texture_t*));

  if (input && input_data) {
#ifdef HAVE_UDEV
    lima_input  = input_udev.init();
    if (lima_input) {
      *input      = lima_input ? &input_udev : NULL;
      *input_data = lima_input;
    }
#else
    lima_input  = input_linuxraw.init();
    if (lima_input) {
      *input      = lima_input ? &input_linuxraw : NULL;
      *input_data = lima_input;
    }
#endif
    else {
      *input = NULL;
      *input_data = NULL;
    }
  }

  vid->lima = lima;

  /*lima_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);*/

  return vid;

fail_lima:
  limare_finish(lima->state);
fail:
  free(lima);
  free(vid);

  return NULL;
}

static bool lima_gfx_frame(void *data, const void *frame,
                       unsigned width, unsigned height,
                       unsigned pitch, const char *msg) {
  lima_video_t *vid;
  const void *pixels;
  limare_data_t *lima;
  bool upload_frame = true;

  vid = data;

  /* Check if neither RGUI nor emulator framebuffer is to be displayed. */
  if (!vid->rgui_active && frame == NULL) return true;

  lima = vid->lima;

  if (frame != NULL) {

    /* Handle resolution changes from the emulation core. */
    if (width != vid->width || height != vid->height) {
      limare_texture_t* tex;

      if (width == 0 || height == 0) return true;

      RARCH_LOG("video_lima: resolution was changed by core to %ux%u\n", width, height);
      tex = get_texture_handle(lima, width, height, 0);

      if (tex == NULL) {
        pixels = make_contiguous(lima, width, height, frame, 0, pitch);

        tex = add_texture(lima, width, height, pixels, 0);

        if (tex == NULL) {
          RARCH_ERR("video_lima: failed to allocate new texture with dimensions %ux%u\n",
            width, height);
          return false;
        }

        upload_frame = false; /* pixel data already got uploaded during texture allocation */
      }

      lima->cur_texture = tex;

      vid->width = width;
      vid->height = height;

      lima->frame_aspect = (float)width / (float)height;
      vid->aspect_changed = true;
    }

    if (upload_frame) {
      pixels = make_contiguous(lima, width, height, frame, 0, pitch);
      limare_texture_mipmap_upload(lima->state, lima->cur_texture->handle, 0, pixels);
    }
  }

  /*if (msg) lima_render_msg(vid, vid->screen, msg, vid->screen->w, vid->screen->h);

   char buffer[128], buffer_fps[128];
   bool fps_draw = g_settings.fps_show;
   if (fps_draw)
   {
      gfx_get_fps(buffer, sizeof(buffer), fps_draw ? buffer_fps : NULL, sizeof(buffer_fps));
      msg_queue_push(g_extern.msg_queue, buffer_fps, 1, 1);
   }*/

  if (vid->aspect_changed) {
    apply_aspect(lima, g_extern.system.aspect_ratio);
    vid->aspect_changed = false;
  }

  limare_frame_new(lima->state);

  if (lima->cur_texture != NULL) {
    limare_program_current(lima->state, lima->program);

    limare_attribute_pointer(lima->state, "in_vertex", LIMARE_ATTRIB_FLOAT,
				 3, 0, 4, lima->vertices);
    limare_attribute_pointer(lima->state, "in_coord", LIMARE_ATTRIB_FLOAT,
				 2, 0, 4, lima->coords + vid->rgui_rotation * 4);

    limare_texture_attach(lima->state, "in_texture", lima->cur_texture->handle);

    if (limare_draw_arrays(lima->state, GL_TRIANGLE_STRIP, 0, 4)) return false;
  }

  if (vid->rgui_active && lima->cur_texture_rgui != NULL) {
    float color[4] = {1.0f, 1.0f, 1.0f, vid->rgui_alpha};

    if (vid->rgui_rgb32)
      limare_program_current(lima->state, lima->program_rgui_rgba32);
    else
      limare_program_current(lima->state, lima->program_rgui_rgba16);

    limare_attribute_pointer(lima->state, "in_vertex", LIMARE_ATTRIB_FLOAT,
				 3, 0, 4, lima->vertices);
    limare_attribute_pointer(lima->state, "in_coord", LIMARE_ATTRIB_FLOAT,
				 2, 0, 4, lima->coords + vid->rgui_rotation * 4);

    limare_texture_attach(lima->state, "in_texture", lima->cur_texture_rgui->handle);
    limare_uniform_attach(lima->state, "uColor", 4, color);

    limare_enable(lima->state, GL_BLEND);
      if (limare_draw_arrays(lima->state, GL_TRIANGLE_STRIP, 0, 4)) return false;
    limare_disable(lima->state, GL_BLEND);
  }

  if (limare_frame_flush(lima->state)) return false;

  limare_buffer_swap(lima->state);

  g_extern.frame_count++;

#ifdef LIMA_GFX_DEBUG
  print_status(lima);
#endif

  return true;
}

static void lima_gfx_set_nonblock_state(void *data, bool state) {
  (void)data; /* limare doesn't export vsync control yet */
  (void)state;
}

static bool lima_gfx_alive(void *data) {
  (void)data;
  return true; /* always alive */
}

static bool lima_gfx_focus(void *data){
  (void)data;
  return true; /* limare doesn't use windowing, so we always have focus */
}

static void lima_gfx_set_rotation(void *data, unsigned rotation) {
  lima_video_t *vid = data;

  vid->rgui_rotation = rotation;
}

static void lima_gfx_viewport_info(void *data, struct rarch_viewport *vp){
  lima_video_t *vid = data;
  vp->x = vp->y = 0;

  vp->width  = vp->full_width  = vid->width;
  vp->height = vp->full_height = vid->height;
}

static void lima_set_aspect_ratio(void *data, unsigned aspect_ratio_idx) {
  lima_video_t *vid = data;

  switch (aspect_ratio_idx) {
    case ASPECT_RATIO_SQUARE:
      gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
    break;

    case ASPECT_RATIO_CORE:
      gfx_set_core_viewport();
    break;

    case ASPECT_RATIO_CONFIG:
      gfx_set_config_viewport();
    break;

    default:
    break;
  }

  g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
  vid->aspect_changed = true;
}

static void lima_apply_state_changes(void *data) {
  (void)data;
}

static void lima_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha) {
  lima_video_t *vid = data;
  limare_texture_t* tex;
  const unsigned format = rgb32 ? LIMA_TEXEL_FORMAT_RGBA_8888 :
                                  LIMA_TEXEL_FORMAT_RGBA_4444;

  vid->rgui_rgb32 = rgb32;
  vid->rgui_alpha = alpha;

  tex = vid->lima->cur_texture_rgui;

  /* Current RGUI doesn't change dimensions, so we should hit this most of the time. */
  if (tex != NULL && tex->width == width &&
      tex->height == height && tex->format == format) goto upload;

  if (tex == NULL) {
    tex = get_texture_handle(vid->lima, width, height, format);
    if (tex == NULL) {
      tex = add_texture(vid->lima, width, height, frame, format);

      if (tex != NULL) {
        vid->lima->cur_texture_rgui = tex;
        goto upload;
      }

      RARCH_ERR("video_lima: failed to allocate new RGUI texture with dimensions %ux%u\n",
            width, height);
    }
  }

  return;

upload:
  limare_texture_mipmap_upload(vid->lima->state, tex->handle, 0, frame);
}

static void lima_set_texture_enable(void *data, bool state, bool full_screen) {
  lima_video_t *vid = data;
  vid->rgui_active = state;
}

static void lima_set_osd_msg(void *data, const char *msg, void *userdata) {
  lima_video_t *vid = data;

  /* TODO: what does this do? */
  (void)msg;
  (void)userdata;
}

static void lima_show_mouse(void *data, bool state) {
  (void)data;
}

static const video_poke_interface_t lima_poke_interface = {
  NULL, /* set_filtering */
#ifdef HAVE_FBO
  NULL, /* get_current_framebuffer */
  NULL, /* get_proc_address */
#endif
  lima_set_aspect_ratio,
  lima_apply_state_changes,
#if defined(HAVE_RGUI) || defined(HAVE_RMENU) /* TODO: only HAVE_MENU i think */
  lima_set_texture_frame,
  lima_set_texture_enable,
#endif
  lima_set_osd_msg,
  lima_show_mouse
};

static void lima_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface) {
  (void)data;
  *iface = &lima_poke_interface;
}

const video_driver_t video_lima = {
  lima_gfx_init,
  lima_gfx_frame,
  lima_gfx_set_nonblock_state,
  lima_gfx_alive,
  lima_gfx_focus,
  NULL, /* set_shader */
  lima_gfx_free,
  "lima",

#ifdef HAVE_MENU
  NULL, /* restart */
#endif

  lima_gfx_set_rotation,
  lima_gfx_viewport_info,
  NULL, /* read_viewport */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  lima_gfx_get_poke_interface
};
