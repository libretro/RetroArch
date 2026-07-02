#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(_WIN32)
#include <malloc.h> /* alloca */
#else
#include <alloca.h>
#endif
#define RVORBIS_NO_PUSHDATA_API
#define RVORBIS_NO_STDIO
#define RVORBIS_NO_CRT
#include <formats/rvorbis.h>
