#ifndef _MPNG_FORMAT_H
#define _MPNG_FORMAT_H

enum video_format
{
   /* these three are same values and order as in libretro - do not change */
   FMT_XRGB1555,
   FMT_XRGB8888,
   FMT_RGB565,

   FMT_NONE,

   FMT_RGB888,
   FMT_ARGB1555,
   FMT_ARGB8888,
};

struct mpng_image
{
	unsigned int width;
	unsigned int height;
	void * pixels;
	
	/* Small, or even large, 
    * amounts of padding between each scanline is fine. 
    * However, each scanline is packed.
    * The pitch is in bytes.
    */
	unsigned int pitch;
	
	enum video_format format;
};

static inline uint8_t videofmt_byte_per_pixel(enum video_format fmt)
{
	static const uint8_t table[]={2, 4, 2, 0, 3, 2, 4};
	return table[fmt];
}

#ifdef __cplusplus
extern "C" {
#endif

/* Valid formats: 888, (8)888, 8888
 * If there is transparency, 8888 is mandatory. */
bool png_decode(const void * pngdata,
      size_t pnglen,
      struct mpng_image *img,
      enum video_format format);

#ifdef __cplusplus
}
#endif

#endif
