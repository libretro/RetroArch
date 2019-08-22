#ifndef __TRANSLATION_DRIVER__H
#define __TRANSLATION_DRIVER__H

#include <boolean.h>
#include <retro_common_api.h>

#include "ocr_driver.h"

RETRO_BEGIN_DECLS

enum translation_init_errors
{
   TRANSLATION_INIT_SUCCESS = 0,
   TRANSLATION_INIT_UNSUPPORTED_DEVICE_LANGUAGE,
   TRANSLATION_INIT_UNSUPPORTED_GAME_LANGUAGE,
   TRANSLATION_INIT_UNKNOWN_DEVICE_LANGUAGE,
   TRANSLATION_INIT_UNKNOWN_GAME_LANGUAGE
};

enum translation_lang
{
   TRANSLATION_LANG_DONT_CARE = 0,
   TRANSLATION_LANG_EN,    /* English  */
   TRANSLATION_LANG_ES,    /* Spanish  */
   TRANSLATION_LANG_FR,    /* French   */
   TRANSLATION_LANG_IT,    /* Italian */
   TRANSLATION_LANG_DE,    /* German   */
   TRANSLATION_LANG_JP,    /* Japanese */
   TRANSLATION_LANG_NL,    /* Dutch    */
   TRANSLATION_LANG_CS,    /* Czech    */
   TRANSLATION_LANG_DA,    /* Danish   */
   TRANSLATION_LANG_SV,    /* Swedish */
   TRANSLATION_LANG_HR,    /* Croatian */
   TRANSLATION_LANG_KO,    /* Korean */
   TRANSLATION_LANG_ZH_CN, /* Chinese Simplified */
   TRANSLATION_LANG_ZH_TW, /* Chinese Traditional */
   TRANSLATION_LANG_CA,    /* Catalan */
   TRANSLATION_LANG_BG,    /* Bulgarian */
   TRANSLATION_LANG_BN,    /* Bengali */
   TRANSLATION_LANG_EU,    /* Basque */
   TRANSLATION_LANG_AZ,    /* Azerbaijani */
   TRANSLATION_LANG_AR,    /* Arabic */
   TRANSLATION_LANG_SQ,    /* Albanian */
   TRANSLATION_LANG_AF,    /* Afrikaans */
   TRANSLATION_LANG_EO,    /* Esperanto */
   TRANSLATION_LANG_ET,    /* Estonian */
   TRANSLATION_LANG_TL,    /* Filipino */
   TRANSLATION_LANG_FI,    /* Finnish */
   TRANSLATION_LANG_GL,    /* Galician */
   TRANSLATION_LANG_KA,    /* Georgian */
   TRANSLATION_LANG_EL,    /* Greek */
   TRANSLATION_LANG_GU,    /* Gujarati */
   TRANSLATION_LANG_HT,    /* Haitian Creole */
   TRANSLATION_LANG_IW,    /* Hebrew */
   TRANSLATION_LANG_HI,    /* Hindi */
   TRANSLATION_LANG_HU,    /* Hungarian */
   TRANSLATION_LANG_IS,    /* Icelandic */
   TRANSLATION_LANG_ID,    /* Indonesian */
   TRANSLATION_LANG_GA,    /* Irish */
   TRANSLATION_LANG_KN,    /* Kannada */
   TRANSLATION_LANG_LA,    /* Latin */
   TRANSLATION_LANG_LV,    /* Latvian */
   TRANSLATION_LANG_LT,    /* Lithuanian */
   TRANSLATION_LANG_MK,    /* Macedonian */
   TRANSLATION_LANG_MS,    /* Malay */
   TRANSLATION_LANG_MT,    /* Maltese */
   TRANSLATION_LANG_NO,    /* Norwegian */
   TRANSLATION_LANG_FA,    /* Persian */
   TRANSLATION_LANG_PL,    /* Polish */
   TRANSLATION_LANG_PT,    /* Portuguese */
   TRANSLATION_LANG_RO,    /* Romanian */
   TRANSLATION_LANG_RU,    /* Russian */
   TRANSLATION_LANG_SR,    /* Serbian */
   TRANSLATION_LANG_SK,    /* Slovak */
   TRANSLATION_LANG_SL,    /* Slovenian */
   TRANSLATION_LANG_SW,    /* Swahili */
   TRANSLATION_LANG_TA,    /* Tamil */
   TRANSLATION_LANG_TE,    /* Telugu */
   TRANSLATION_LANG_TH,    /* Thai */
   TRANSLATION_LANG_TR,    /* Turkish */
   TRANSLATION_LANG_UK,    /* Ukrainian */
   TRANSLATION_LANG_UR,    /* Urdu */
   TRANSLATION_LANG_VI,    /* Vietnamese */
   TRANSLATION_LANG_CY,    /* Welsh */
   TRANSLATION_LANG_YI,    /* Yiddish */
   TRANSLATION_LANG_LAST
};

struct translation_driver_info
{
	int device_language;
	int game_language;
};

typedef struct translation_driver
{
	void* (*init)(const struct translation_driver_info *params);
	void  (*free)(void* data);

	/* use translate_image if non NULL else run image through ocr driver then run translate_text */
	/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to translate_image/text */
	/* NOTE: translate_image is allowed to call the ocr driver itself if it wants */
	char* (*translate_text)(void* data, const char* game_text);
	char* (*translate_image)(void* data, struct ocr_image_info image);

	const char *ident;
} translation_driver_t;

extern const translation_driver_t translation_cached_google;
extern const translation_driver_t translation_null;

bool  translation_driver_init(void);
void  translation_driver_free(void);

/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to translation_driver_translate_image */
char* translation_driver_translate_image(struct ocr_image_info image);

RETRO_END_DECLS

#endif
