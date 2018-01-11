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

extern const ocr_driver_t ocr_tesseract;
extern const ocr_driver_t ocr_null;

char* ocr_get_text(struct ocr_image_info image);

#endif