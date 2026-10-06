#ifndef WIN32_WINDOW_PROC_TRACE_H
#define WIN32_WINDOW_PROC_TRACE_H
#include <stddef.h>
#include <boolean.h>
extern char   trace_buf[1 << 20];
extern size_t trace_len;
extern bool   stub_input_takes;
/* off while the window is created, destroyed or left to settle: what
 * arrives then is the desktop's doing and differs from one to the next */
extern bool   trace_on;
/* which of the video families' creation steps ran: 1 GL, 2 Vulkan */
extern unsigned stub_create_steps;
/* WM_SETFOCUS and WM_KILLFOCUS offered to the input driver */
extern unsigned stub_input_focus_msgs;
void trace(const char *fmt, ...);
void stubs_init(void);
void stub_set_geometry(unsigned width, unsigned height);
#endif
