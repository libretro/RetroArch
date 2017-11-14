// 6 april 2015
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include "controlsigs.h"

extern uiInitOptions options;

extern void *uiAlloc(size_t, const char *);
#define uiNew(T) ((T *) uiAlloc(sizeof (T), #T))
extern void *uiRealloc(void *, size_t, const char *);
extern void uiFree(void *);

// ugh, this was only introduced in MSVC 2015...
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif
extern void realbug(const char *file, const char *line, const char *func, const char *prefix, const char *format, va_list ap);
#define _ns2(s) #s
#define _ns(s) _ns2(s)
extern void _implbug(const char *file, const char *line, const char *func, const char *format, ...);
#define implbug(...) _implbug(__FILE__, _ns(__LINE__), __func__, __VA_ARGS__)
extern void _userbug(const char *file, const char *line, const char *func, const char *format, ...);
#define userbug(...) _userbug(__FILE__, _ns(__LINE__), __func__, __VA_ARGS__)

// control.c
extern uiControl *newControl(size_t size, uint32_t OSsig, uint32_t typesig, const char *typenamestr);

// shouldquit.c
extern int shouldQuit(void);

// areaevents.c
typedef struct clickCounter clickCounter;
// you should call Reset() to zero-initialize a new instance
// it doesn't matter that all the non-count fields are zero: the first click will fail the curButton test straightaway, so it'll return 1 and set the rest of the structure accordingly
struct clickCounter {
	int curButton;
	int rectX0;
	int rectY0;
	int rectX1;
	int rectY1;
	uintptr_t prevTime;
	int count;
};
int clickCounterClick(clickCounter *c, int button, int x, int y, uintptr_t time, uintptr_t maxTime, int32_t xdist, int32_t ydist);
extern void clickCounterReset(clickCounter *);
extern int fromScancode(uintptr_t, uiAreaKeyEvent *);

// matrix.c
extern void fallbackSkew(uiDrawMatrix *, double, double, double, double);
extern void scaleCenter(double, double, double *, double *);
extern void fallbackTransformSize(uiDrawMatrix *, double *, double *);

#ifdef __cplusplus
}
#endif
