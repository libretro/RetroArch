/* Logging sink shared by the linked driver and engine: the driver
 * calls the verbosity.h functions, the standalone engine expands the
 * macros. Both land in live_log(). */
#ifndef LIVE_LOG_H
#define LIVE_LOG_H
void live_log(const char *fmt, ...);
#define RARCH_LOG(...)  live_log(__VA_ARGS__)
#define RARCH_DBG(...)  live_log(__VA_ARGS__)
#define RARCH_ERR(...)  live_log("[ERROR] " __VA_ARGS__)
#define RARCH_WARN(...) live_log("[WARN] " __VA_ARGS__)
#endif
