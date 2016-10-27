#ifndef _OS_TYPES_H_
#define _OS_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

typedef struct _OSCalendarTime {
  int sec;
  int min;
  int hour;
  int mday;
  int mon;
  int year;
  int wday;
  int yday;
  int msec;
  int usec;
} OSCalendarTime;

#ifdef __cplusplus
}
#endif

#endif
