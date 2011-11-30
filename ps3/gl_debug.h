#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdint.h>
#include <sys/timer.h>
#ifdef __cplusplus
extern "C" {
#endif

// Inits debug lib
void init_debug(uint64_t width, uint64_t height); 

void dprintf_console(const char *fmt, ...);
void dprintf(float x, float y, float scale, const char *fmt, ...);

void uninit_debug(void);
void write_fps(void);
void test_performance(void);

#ifdef SSNES_DEBUG
#define SSNES_LOG(msg, args...) do { \
   dprintf(0.1f, 0.1f, 1.0f, "SSNES: " msg, ##args); \
   sys_timer_usleep(300000); \
   } while(0)

#define SSNES_ERR(msg, args...) do { \
   dprintf(0.1f, 0.1f, 1.0f, "SSNES [ERROR] :: " msg, ##args); \
   sys_timer_usleep(300000); \
   } while(0)
#else
#define SSNES_LOG(msg, args...) ((void)0)
#define SSNES_ERR(msg, args...) ((void)0)
#endif // SSNES_DEBUG


#ifdef __cplusplus
}
#endif

#endif // __DEBUG_H
