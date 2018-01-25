#ifndef TESS_GET_TEXT
#define TESS_GET_TEXT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned width;
	unsigned height;
	unsigned bytes_per_pixel;
	void*    data;
}tess_image;

extern char tess_last_error[];

//if running in RetroArch language should be "eng" or "jpn"
bool  tess_init(const char* lang_data_dir, const char* language);
void  tess_deinit();
char* tess_get_text(tess_image image);

#ifdef __cplusplus
}
#endif


#endif