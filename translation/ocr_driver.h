#ifndef __OCR_DRIVER__H
#define __OCR_DRIVER__H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct ocr_image_info
{
	unsigned width;
	unsigned height;
	unsigned pixel_format;
	void*    data;
};

typedef struct ocr_driver
{
	void* (*init)(int game_character_set);
	void  (*free)(void* data);

	/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to get_text */
	char* (*get_text)(void* data, struct ocr_image_info image);

	const char *ident;
} ocr_driver_t;

#ifdef HAVE_TESSERACT
extern const ocr_driver_t ocr_tesseract;
#endif
extern const ocr_driver_t ocr_null;

bool  ocr_driver_init(void);
void  ocr_driver_free(void);

/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to ocr_driver_get_text */
char* ocr_driver_get_text(struct ocr_image_info image);

RETRO_END_DECLS

#endif