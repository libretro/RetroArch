#include "../../deps/zahnrad/zahnrad.h"
#include "../menu_display.h"

enum
{
   ZR_TEXTURE_POINTER = 0,
   ZR_TEXTURE_BACK,
   ZR_TEXTURE_SWITCH_ON,
   ZR_TEXTURE_SWITCH_OFF,
   ZR_TEXTURE_TAB_MAIN_ACTIVE,
   ZR_TEXTURE_TAB_PLAYLISTS_ACTIVE,
   ZR_TEXTURE_TAB_SETTINGS_ACTIVE,
   ZR_TEXTURE_TAB_MAIN_PASSIVE,
   ZR_TEXTURE_TAB_PLAYLISTS_PASSIVE,
   ZR_TEXTURE_TAB_SETTINGS_PASSIVE,
   ZR_TEXTURE_LAST
};

enum
{
   ZRMENU_WND_MAIN = 0,
   ZRMENU_WND_CONTROL,
   ZRMENU_WND_SHADER_PARAMETERS,
   ZRMENU_WND_TEST
};

enum zrmenu_theme
{
   THEME_DARK = 0,
   THEME_LIGHT
};

typedef struct zrmenu_handle
{
   char box_message[PATH_MAX_LENGTH];
   bool window_enabled[4];
   bool resize;
   unsigned width;
   unsigned height;
   void *memory;
   struct zr_context ctx;
   struct zr_memory_status status;
   enum zrmenu_theme theme;

   struct
   {
      menu_texture_item bg;
      menu_texture_item list[ZR_TEXTURE_LAST];
   } textures;

   gfx_font_raster_block_t list_block;
} zrmenu_handle_t;

void zrmenu_set_style(struct zr_context *ctx, enum zrmenu_theme theme);
void zrmenu_wnd_shader_parameters(struct zr_context *ctx, zrmenu_handle_t *zr);
void zrmenu_wnd_control(struct zr_context *ctx, zrmenu_handle_t *zr);
void zrmenu_wnd_test(struct zr_context *ctx, zrmenu_handle_t *zr);
void zrmenu_wnd_main(struct zr_context *ctx, zrmenu_handle_t *zr);
