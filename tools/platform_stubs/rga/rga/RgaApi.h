#ifndef RGA_API_STUB_H
#define RGA_API_STUB_H
/* Stand-in for Rockchip librga's RgaApi.h: the types, enumerants and
 * prototypes gfx/drivers/oga_gfx.c uses, with the values and layouts
 * librga gives them, so the driver is compiled - and its calls are
 * checked - where librga is not installed. */
enum
{
   HAL_TRANSFORM_FLIP_H  = 0x01,
   HAL_TRANSFORM_FLIP_V  = 0x02,
   HAL_TRANSFORM_ROT_90  = 0x04,
   HAL_TRANSFORM_ROT_180 = 0x03,
   HAL_TRANSFORM_ROT_270 = 0x07
};

typedef enum _Rga_SURF_FORMAT
{
   RK_FORMAT_RGBA_8888 = 0x0,
   RK_FORMAT_RGBX_8888 = 0x1,
   RK_FORMAT_RGB_888   = 0x2,
   RK_FORMAT_BGRA_8888 = 0x3,
   RK_FORMAT_RGB_565   = 0x4
} RgaSURF_FORMAT;

typedef struct rga_rect
{
   int xoffset;
   int yoffset;
   int width;
   int height;
   int wstride;
   int hstride;
   int format;
   int size;
} rga_rect_t;

typedef struct rga_info
{
   int fd;
   void *virAddr;
   void *phyAddr;
   unsigned hnd;
   int format;
   rga_rect_t rect;
   unsigned int blend;
   int bufferSize;
   int rotation;
   int color;
   int testLog;
   int mmuFlag;
   int colorkey_en;
   int colorkey_mode;
   int colorkey_max;
   int colorkey_min;
   int scale_mode;
   int color_space_mode;
   int sync_mode;
   char reserve[128];
} rga_info_t;

int c_RkRgaInit(void);
void c_RkRgaDeInit(void);
int c_RkRgaBlit(rga_info_t *src, rga_info_t *dst, rga_info_t *src1);
int c_RkRgaColorFill(rga_info_t *dst);

#endif
