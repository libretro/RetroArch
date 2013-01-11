#include <stdio.h>

#define LOG(stream, level, msg, ...) fprintf(stream, "%s::%s+%d::", level, __FILE__, __LINE__); fprintf(stream, msg, ##__VA_ARGS__); fprintf(stream, "\n")
#define LOG_DEBUG(msg, ...) LOG(stderr, "DEBUG", msg, ##__VA_ARGS__)
#define LOG_WARN(msg, ...) LOG(stderr, "WARNING", msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) LOG(stdout, "INFO", msg, ##__VA_ARGS__)
