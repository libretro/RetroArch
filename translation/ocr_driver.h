#ifndef __OCR_DRIVER__H
#define __OCR_DRIVER__H

struct ocr_image_info
{
	int 	 game_character_set;
	unsigned image_width;
	unsigned image_height;
	unsigned pixel_format;
	void*    image_data;
};

typedef struct ocr_driver
{
	void* (*init)();
	void  (*free)(void* data);
	
	char* (*get_text)(struct ocr_image_info image);
	
	const char *ident;
} ocr_driver_t;

#ifdef HAVE_TESSERACT
extern const ocr_driver_t ocr_tesseract;
#endif
extern const ocr_driver_t ocr_null;

bool  ocr_driver_init(void);
void  ocr_driver_free(void);
char* ocr_driver_get_text(struct ocr_image_info image);

#endif