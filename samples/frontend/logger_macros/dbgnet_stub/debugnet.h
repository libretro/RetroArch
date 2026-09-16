#ifndef DEBUGNET_H
#define DEBUGNET_H
#define DEBUGNET_NONE 0
#define DEBUGNET_INFO 1
#define DEBUGNET_ERROR 2
#define DEBUGNET_DEBUG 3
void debugNetPrintf(int level, const char *fmt, ...);
#endif
