/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef INPUT_OVERLAY_H__
#define INPUT_OVERLAY_H__

#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "input_types.h"

#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)


#define CUSTOM_BINDS_U32_COUNT ((RARCH_CUSTOM_BIND_LIST_END - 1) / 32 + 1)

#define OVERLAY_MAX_TOUCH 16
#define OVERLAY_LIGHTGUN_TRIG_MAX_DELAY 15

RETRO_BEGIN_DECLS

enum overlay_hitbox
{
   OVERLAY_HITBOX_RADIAL = 0,
   OVERLAY_HITBOX_RECT,
   OVERLAY_HITBOX_NONE
};

enum overlay_type
{
   OVERLAY_TYPE_BUTTONS = 0,
   OVERLAY_TYPE_ANALOG_LEFT,
   OVERLAY_TYPE_ANALOG_RIGHT,
   OVERLAY_TYPE_DPAD_AREA,
   OVERLAY_TYPE_ABXY_AREA,
   OVERLAY_TYPE_KEYBOARD,
   OVERLAY_TYPE_LAST
};

/* Superset of overlay_type for menu entries */
enum overlay_menu_type
{
   OVERLAY_TYPE_OSK_TOGGLE = OVERLAY_TYPE_LAST
};

enum overlay_status
{
   OVERLAY_STATUS_NONE = 0,
   OVERLAY_STATUS_DEFERRED_LOAD,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE_PROCESS,
   OVERLAY_STATUS_DEFERRED_LOADING,
   OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE,
   OVERLAY_STATUS_DEFERRED_DONE,
   OVERLAY_STATUS_DEFERRED_ERROR
};

enum overlay_image_transfer_status
{
   OVERLAY_IMAGE_TRANSFER_NONE = 0,
   OVERLAY_IMAGE_TRANSFER_BUSY,
   OVERLAY_IMAGE_TRANSFER_DONE,
   OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_DONE,
   OVERLAY_IMAGE_TRANSFER_ERROR
};

enum overlay_orientation
{
   OVERLAY_ORIENTATION_NONE = 0,
   OVERLAY_ORIENTATION_LANDSCAPE,
   OVERLAY_ORIENTATION_PORTRAIT
};

enum overlay_show_input_type
{
   OVERLAY_SHOW_INPUT_NONE = 0,
   OVERLAY_SHOW_INPUT_TOUCHED,
   OVERLAY_SHOW_INPUT_PHYSICAL,
   OVERLAY_SHOW_INPUT_LAST
};

enum OVERLAY_LOADER_FLAGS
{
   OVERLAY_LOADER_RGBA_SUPPORT = (1 << 0),
   OVERLAY_LOADER_IS_OSK       = (1 << 1),
   /* The driver samples XRGB2101010, so a 16-bit PNG in the pack is
    * decoded at ten bits a channel instead of being flattened to
    * eight. An 8-bit image decodes as it always did. */
   OVERLAY_LOADER_10BIT        = (1 << 2),
   /* A desc of the pack names the LED its image shows (_led). */
   OVERLAY_LOADER_HAS_LEDS     = (1 << 3)
};

enum INPUT_OVERLAY_FLAGS
{
   INPUT_OVERLAY_ENABLE  = (1 << 0),
   INPUT_OVERLAY_ALIVE   = (1 << 1),
   INPUT_OVERLAY_BLOCKED = (1 << 2),
   INPUT_OVERLAY_IS_OSK  = (1 << 3),
   INPUT_OVERLAY_GAMEPAD_HIDDEN = (1 << 4),
   /* The driver declined the pack's textures (load_textures): pages
    * go through load() until the next enable. */
   INPUT_OVERLAY_TEXTURES_DECLINED = (1 << 5),
   /* The pack names its LED images (overlayN_descM_led): the overlay
    * LED driver shows and hides those, and ledN_map is not used. */
   INPUT_OVERLAY_HAS_LEDS = (1 << 6)
};

enum OVERLAY_FLAGS
{
   OVERLAY_FULL_SCREEN        = (1 << 0),
   OVERLAY_BLOCK_SCALE        = (1 << 1),
   OVERLAY_BLOCK_X_SEPARATION = (1 << 2),
   OVERLAY_BLOCK_Y_SEPARATION = (1 << 3),
   OVERLAY_AUTO_X_SEPARATION  = (1 << 4),
   OVERLAY_AUTO_Y_SEPARATION  = (1 << 5),
   OVERLAY_HAS_VIEWPORT       = (1 << 6),
   OVERLAY_VIEWPORT_FILL      = (1 << 7),
   /* At least one desc on the page does something when pressed. A
    * page of nothing but "nul" buttons (an LED or decoration overlay)
    * takes no input, so it has no claim on the menu's mouse. */
   OVERLAY_TAKES_INPUT        = (1 << 8)
};

enum OVERLAY_DESC_FLAGS
{
   OVERLAY_DESC_MOVABLE             = (1 << 0),
   /* If true, blocks input from overlapped hitboxes */
   OVERLAY_DESC_EXCLUSIVE           = (1 << 1),
   /* Similar, but only applies after range_mod takes effect */
   OVERLAY_DESC_RANGE_MOD_EXCLUSIVE = (1 << 2)
};

enum overlay_lightgun_action
{
   OVERLAY_LIGHTGUN_ACTION_NONE = 0,
   OVERLAY_LIGHTGUN_ACTION_TRIGGER,
   OVERLAY_LIGHTGUN_ACTION_RELOAD,
   OVERLAY_LIGHTGUN_ACTION_AUX_A,
   OVERLAY_LIGHTGUN_ACTION_AUX_B,
   OVERLAY_LIGHTGUN_ACTION_AUX_C,
   OVERLAY_LIGHTGUN_ACTION_START,
   OVERLAY_LIGHTGUN_ACTION_SELECT,
   OVERLAY_LIGHTGUN_ACTION_DPAD_UP,
   OVERLAY_LIGHTGUN_ACTION_DPAD_DOWN,
   OVERLAY_LIGHTGUN_ACTION_DPAD_LEFT,
   OVERLAY_LIGHTGUN_ACTION_DPAD_RIGHT,

   OVERLAY_LIGHTGUN_ACTION_END
};

enum overlay_mouse_button
{
   OVERLAY_MOUSE_BTN_NONE = 0,
   OVERLAY_MOUSE_BTN_LMB,
   OVERLAY_MOUSE_BTN_RMB,
   OVERLAY_MOUSE_BTN_MMB,

   OVERLAY_MOUSE_BTN_END
};

/* Overlay driver acts as a medium between input drivers
 * and video driver.
 *
 * Coordinates are fetched from input driver, and an
 * overlay with pressable actions are displayed on-screen.
 *
 * This interface requires that the video driver has support
 * for the overlay interface.
 */

typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data,
         const void *images, unsigned num_images);
   /* Show @num_textures textures video_driver_texture_load() made,
    * in place of load(): the driver takes the same per-image
    * geometry and alpha as for load() but uploads nothing and owns
    * nothing - the textures are the overlay pack's, uploaded once
    * for every page and unloaded by the frontend after enable(false).
    * A page switch is then a pass over indices. Optional; a driver
    * without it takes load() on every switch as before. */
   bool (*load_textures)(void *data,
         const uintptr_t *textures, unsigned num_textures);
   void (*tex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*vertex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, unsigned image, float mod);
} video_overlay_interface_t;

typedef struct overlay_eightway_config
{
   input_bits_t up;
   input_bits_t right;
   input_bits_t down;
   input_bits_t left;

   input_bits_t up_right;
   input_bits_t up_left;
   input_bits_t down_right;
   input_bits_t down_left;

   /* diagonal sensitivity */
   float* slope_high;
   float* slope_low;
} overlay_eightway_config_t;

struct overlay_desc
{
   struct texture_image image;

   enum overlay_hitbox hitbox;
   enum overlay_type type;

   unsigned next_index;
   /* Index into the page's own image list (load_images / the page's
    * textures). Every desc with an image gets its own entry, even when
    * several share one file. */
   unsigned image_index;
   /* Index into the pack's unique images (ol->images and the anim_*
    * arrays), which are deduplicated by path. Only meaningful when the
    * desc has an image. */
   unsigned pack_image_index;

   float alpha_mod;
   float range_mod;
   float analog_saturate_pct;
   float range_x, range_y;
   float range_x_mod, range_y_mod;
   float mod_x, mod_y, mod_w, mod_h;
   float delta_x, delta_y;
   float x;
   float y;
   /* These are 'raw' x/y values shifted
    * by a user-configured offset (c.f.
    * OVERLAY_X/Y_SEPARATION). Used to determine
    * correct hitbox locations. By default,
    * will be equal to x/y */
   float x_shift;
   float y_shift;

   /* These values are used only for hitbox
    * detection. A hitbox can be stretched in
    * any direction(s) by its 'reach' values */
   float x_hitbox;
   float y_hitbox;
   float range_x_hitbox, range_y_hitbox;
   float reach_right, reach_left, reach_up, reach_down;

   /* This is a retro_key value for keyboards */
   unsigned retro_key_idx;

   /* This is a bit mask of all input binds to set with this overlay control */
   input_bits_t button_mask;

   overlay_eightway_config_t *eightway_config;

   char next_index_name[64];

   /* Nonzero if pressed. Lower bits used for pointer indexes */
   uint32_t touch_mask;
   uint32_t old_touch_mask;

   /* The LED (1-based, as ledN_map counts) whose state this desc's
    * image shows under the overlay LED driver; 0 for none. */
   uint8_t led;
   uint8_t flags;
};

struct overlay
{
   struct overlay_desc *descs;
   struct texture_image *load_images;
   /* The pack's texture handle of each load_images entry, valid while
    * the overlay is enabled on a driver with load_textures; views
    * into input_overlay_t::page_textures. */
   uintptr_t *textures;

   struct texture_image image;

   unsigned load_images_size;
   unsigned id;

   size_t size;
   size_t pos;

   float mod_x, mod_y, mod_w, mod_h;
   float x, y, w, h;
   float center_x, center_y;
   float aspect_ratio;

   /* Viewport override - normalized coordinates (0.0-1.0) */
   struct
   {
      float x;
      float y;
      float w;
      float h;
   } viewport;

   struct
   {
      float alpha_mod;
      float range_mod;

      struct
      {
         unsigned size;
         char key[64];
      } descs;

      struct
      {
         char key[64];
         char path[PATH_MAX_LENGTH];
      } paths;

      struct
      {
         char key[64];
      } names;

      struct
      {
         char array[256];
         char key[64];
      } rect;

      bool normalized;
   } config;

   char name[64];

   uint16_t flags;
};

typedef struct input_overlay_state
{
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons;

   /* Input pointers from input_state */
   struct
   {
      int16_t x;
      int16_t y;
   } touch[OVERLAY_MAX_TOUCH];
   int touch_count;
} input_overlay_state_t;

typedef struct input_overlay_mouse_state
{
   float scale_x;
   float scale_y;

   int16_t prev_screen_x;
   int16_t prev_screen_y;

   /* Bits 0-2 used for LMB, RMB, MMB */
   uint8_t click;
   uint8_t hold;
} input_overlay_mouse_state_t;

/* Non-hitbox input state for pointer, mouse, and lightgun */
typedef struct input_overlay_pointer_state
{
   /* Input pointers that missed every hitbox */
   struct
   {
      int16_t x;
      int16_t y;
   } ptr[OVERLAY_MAX_TOUCH];
   unsigned count;

   /* Main pointer, full screen */
   int16_t screen_x;
   int16_t screen_y;

   struct input_overlay_lightgun_state
   {
      /* Input ID based on pointer count */
      unsigned multitouch_id;
   } lightgun;

   input_overlay_mouse_state_t mouse;

   /* Mask of requested devices
    * to avoid unnecessary polling */
   uint8_t device_mask;
} input_overlay_pointer_state_t;

struct input_overlay
{
   struct overlay *overlays;
   const struct overlay *active;
   char *path;
   void *iface_data;
   const video_overlay_interface_t *iface;
   input_overlay_state_t overlay_state;
   input_overlay_pointer_state_t pointer_state;
   struct texture_image **images;
   /* One block: the pack's texture handle per unique image (the first
    * num_images entries), then each page's handle list in page order,
    * which overlay::textures point into. Built by the enable on a
    * driver with load_textures, unloaded and freed by the disable. */
   uintptr_t *page_textures;
   /* An animated image's file bytes and its APNG stream, one entry per
    * unique image, NULL for the still ones. The bytes are kept because
    * the stream decodes from them frame by frame; a still image's
    * bytes and pixels both go once its texture exists. */
   void **anim_data;
   size_t *anim_len;
   void **anim_stream;
   /* When the frame showing now is due to be replaced, in
    * microseconds on the same clock as the rest of the frontend. */
   int64_t *anim_next_us;
   /* A gfx_surface per unique image, holding that texture: the same
    * ownership the animated previews use, so an overlay asset and a
    * preview frame reach the GPU through one path. num_images of
    * them, NULL until the pack is uploaded. */
   void **surfaces;

   /* Two-frame APNGs are treated as a pressed/unpressed pair rather
    * than a looping animation. One entry per unique image; only valid
    * when the corresponding anim_stream entry is non-NULL. */
   uint8_t *anim_2frame;
   uint8_t *anim_2frame_pressed;
   uint8_t *anim_2frame_cur;
   /* Both frames of each two-frame APNG, composed once at load and
    * laid out back to back (width * height pixels each), so a press
    * is a copy rather than a decode. NULL for every other image. */
   uint32_t **anim_2frame_pix;

   /* The alpha last handed to the driver for each image of the active
    * page, and the pass's scratch, alpha_cap entries each in one block
    * (alpha_cap = the most images any page has). An image whose alpha
    * has not changed is not set again. A page load forgets them all:
    * the driver resets its own. NULL when the block could not be had;
    * every alpha is then set every pass. */
   float *alpha_sent;
   float *alpha_want;
   size_t alpha_cap;

   size_t num_images;
   size_t index;
   size_t size;

   unsigned next_index;

   uint8_t flags;
};

/* Holds general layout information for an
 * overlay (overall scaling + positional
 * offset factors) */
typedef struct
{
   float scale_landscape;
   float aspect_adjust_landscape;
   float x_separation_landscape;
   float y_separation_landscape;
   float x_offset_landscape;
   float y_offset_landscape;
   float scale_portrait;
   float aspect_adjust_portrait;
   float x_separation_portrait;
   float y_separation_portrait;
   float x_offset_portrait;
   float y_offset_portrait;
   float touch_scale;
   bool auto_scale;
} overlay_layout_desc_t;

/* Holds derived overlay layout information
 * for a specific display orientation */
typedef struct
{
   float x_scale;
   float y_scale;
   float x_separation;
   float y_separation;
   float x_offset;
   float y_offset;
} overlay_layout_t;

typedef struct overlay_desc overlay_desc_t;

typedef struct input_overlay input_overlay_t;

typedef struct
{
   char *overlay_path;
   struct overlay *overlays;
   struct overlay *active;
   struct string_list *image_list;
   /* Parallel to image_list: for an image that turned out to be an
    * APNG, the file bytes its frames are composed from, as an
    * overlay_anim_src_t in attr.p; NULL entries for the stills. The
    * pack takes them over with the images. */
   struct string_list *anim_list;
   size_t size;
   uint16_t overlay_types;
   uint8_t flags;
} overlay_task_data_t;

void input_overlay_free_overlay(struct overlay *overlay);

/* The overlay LED driver's entry points: whether it is the LED
 * driver, and a core's LED going on or off. */
void input_overlay_leds_enable(bool enable);
void input_overlay_set_led(int led, bool lit);

/* Whether the LED driver hides image @image of the active page: the
 * image of a desc naming an unlit LED (_led) when the pack names its
 * LED images, else the image of a "nul" desc an unlit LED's @led_map
 * entry points at - a button that does something is never an LED.
 * @led_map is NULL when the overlay LED driver is not in use, and
 * nothing is hidden. Bit n of @lit is LED n + 1. */
bool input_overlay_image_hidden(const input_overlay_t *ol,
      unsigned image, uint32_t lit, const unsigned *led_map);

/* Sets alpha 0 on every image of the active page that
 * input_overlay_image_hidden() would hide. */
void input_overlay_hide_leds(input_overlay_t *ol,
      uint32_t lit, const unsigned *led_map);

/* Every image of the active page to its alpha: @mod, 0 for one the LED
 * driver hides, and with @show_input a pressed desc's
 * alpha_mod * @opacity. Only an image whose alpha differs from the
 * last one handed to the driver is set. */
void input_overlay_alpha_pass(input_overlay_t *ol, float mod,
      bool show_input, float opacity,
      uint32_t lit, const unsigned *led_map);

/* The driver was handed a page: nothing it holds is known any more. */
void input_overlay_alpha_forget(input_overlay_t *ol);

/* Attempts to automatically rotate the specified overlay.
 * Depends upon proper naming conventions in overlay
 * config file. */
void input_overlay_auto_rotate_(
      unsigned output_dims,
      bool input_overlay_enable,
      input_overlay_t *ol);

/* The file bytes of one animated overlay image, handed from the
 * loader to the pack, which frees them with the image. */
typedef struct
{
   void  *data;
   size_t len;
} overlay_anim_src_t;

/* Advance the pack's animated images to the frame due at @now, one
 * per poll on the main thread; a pack with none is untouched. The
 * frames update their textures in place, so the pages' handles stand
 * and nothing is uploaded. */
void input_overlay_animate(input_overlay_t *ol, retro_time_t now);

/* Whether a page or a desc has an image, asked of its texture_image.
 *
 * By its size, never by its pixels. The pixels are released once the
 * driver has the pack's textures (input_overlay_load_page), and the
 * struct that release clears is not some private copy: the loader
 * registers the address of the first overlay::image or
 * overlay_desc::image to name a file as the pack's unique image
 * (task_overlay_load_image_texture), so input_overlay::images[] points
 * INTO the pages. A test on .pixels therefore turned false for exactly
 * the descs that had just been given textures; their geometry was
 * never set, and every image of the page was drawn over the whole
 * screen. A desc or page without an image is calloc()ed and has no
 * width; one with an image keeps its width for good. */
#define OVERLAY_HAS_IMAGE(img) ((img)->width != 0)

/* Unload the pack's textures (see video_overlay_interface::load_textures)
 * and forget the page lists. Safe to call with none uploaded. */
void input_overlay_release_textures(input_overlay_t *ol);

/* Upload every unique image of the pack and build each page's list of
 * handles. True when the pack has its textures, already or as of this
 * call; false when it cannot be uploaded this way, or when the uploads
 * are still with the video thread (input_overlay_promote_textures). */
bool input_overlay_upload_textures(input_overlay_t *ol);

/* Whether the pack can still be shown: it has its textures, or the
 * pixels to make them from. */
bool input_overlay_has_source(const input_overlay_t *ol);

/* What the driver holds after input_overlay_load_page(). */
enum input_overlay_page
{
   /* Nothing of this page: the pack had nothing to show it from, or
    * the driver could not load it. Whatever the driver held before -
    * no page, or an older one with fewer images - is what the per-image
    * setters (set_alpha, vertex_geom, tex_geom) would now write to,
    * with this page's indices: the caller leaves them alone. */
   INPUT_OVERLAY_PAGE_NONE = 0,
   INPUT_OVERLAY_PAGE_PIXELS,    /* through load()          */
   INPUT_OVERLAY_PAGE_TEXTURES   /* through load_textures() */
};

/* Hand the active page to the driver: as textures when the driver
 * takes them, as pixels through load() otherwise. */
enum input_overlay_page input_overlay_load_page(input_overlay_t *ol);

/* Under threaded video the pack's uploads finish after the page was
 * shown through load(). Once per poll: when the last handle is in,
 * the active page is handed to the driver again as textures and the
 * pixels go. True on the poll that happens, and the caller applies
 * the page's alpha and geometry again, as after any load; false, at
 * the cost of a few tests, on every other. */
bool input_overlay_promote_textures(input_overlay_t *ol);

/* Unload the textures of the active and the cached pack ahead of the
 * video driver's teardown; they are uploaded again at the next enable
 * on whatever driver comes up. */
void input_overlay_video_teardown(void);

void input_overlay_load_active(input_overlay_t *ol, float opacity);

/**
 * input_overlay_next_move_touch_masks
 * @ol : Overlay handle.
 * 
 * Finds similar descs in the next overlay (i.e. same location and type)
 * and moves touch masks from active overlay to next.
 */
void input_overlay_next_move_touch_masks(input_overlay_t *ol);

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @layout_desc           : Scale + offset factors.
 * @output_dims           : Output size, packed with VIDEO_SCALE_PACK.
 *
 * Scales the overlay and applies any aspect ratio/
 * offset factors.
 **/
void input_overlay_set_scale_factor(
      input_overlay_t *ol, const overlay_layout_desc_t *layout_desc,
      unsigned output_dims);

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod);

/**
 * input_overlay_set_eightway_diagonal_sensitivity:
 *
 * Gets the slope limits defining each eightway type's diagonal zones.
 */
void input_overlay_set_eightway_diagonal_sensitivity(void);

RETRO_END_DECLS

#endif
