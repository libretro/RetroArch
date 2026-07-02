#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(_WIN32)
#include <malloc.h> /* alloca */
#elif !defined(__DJGPP__)
#include <alloca.h> /* DJGPP declares alloca in <stdlib.h> (already included) */
#endif
#include <formats/rvorbis.h>
