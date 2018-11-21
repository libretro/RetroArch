#ifndef MATH_H
#define MATH_H

#include <floatlib.h>
#define roundf(in) (in >= 0.0f ? floorf(in + 0.5f) : ceilf(in - 0.5f))

#define cos(x) ((double)cosf((float)x))
#define pow(x, y) ((double)powf((float)x, (float)y))
#define sin(x) ((double)sinf((float)x))
#define ceil(x) ((double)ceilf((float)x))
#define floor(x) ((double)floorf((float)x))
#define sqrt(x) ((double)sqrtf((float)x))
#define fabs(x) ((double)fabsf((float)(x)))

#define fmaxf(a, b) (((a) > (b)) ? (a) : (b))
#define fminf(a, b) (((a) < (b)) ? (a) : (b))

#define exp(a) ((double)expf((float)a))
#define log(a) ((double)logf((float)a))

#endif //MATH_H
