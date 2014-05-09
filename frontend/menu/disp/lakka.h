void lakka_render(void *data);

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
