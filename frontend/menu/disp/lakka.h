#ifndef _MENU_DISP_LAKKA_H
#define _MENU_DISP_LAKKA_H

#include "../../../gfx/gl_common.h"
#include "../../../gfx/fonts/fonts.h"

extern int depth;
extern int num_categories;
extern int menu_active_category;

typedef struct
{
   char*  name;
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   struct font_output_list out;
} menu_subitem;

typedef struct
{
   char*  name;
   char*  rom;
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   int    active_subitem;
   int num_subitems;
   menu_subitem *subitems;
   struct font_output_list out;
} menu_item;

typedef struct
{
   char*  name;
   char*  libretro;
   GLuint icon;
   float  alpha;
   float  zoom;
   int    active_item;
   int    num_items;
   menu_item *items;
   struct font_output_list out;
} menu_category;

extern menu_category *categories;

void lakka_render(void *data);
void lakka_switch_items(void);
void lakka_switch_subitems(void);
void lakka_open_submenu(void);
void lakka_close_submenu(void);
void lakka_switch_categories(void);

#endif /* MENU_DISP_LAKKA_H */
