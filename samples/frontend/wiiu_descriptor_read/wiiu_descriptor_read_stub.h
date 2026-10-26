/* The stand-in vfs_implementation.c reads through, declared the way the
 * host declares read(), whose prototype the redirect renames as well. */
#include <stddef.h>
#ifdef _WIN32
int test_read(int fd, void *buf, unsigned int len);
#else
#include <sys/types.h>
ssize_t test_read(int fd, void *buf, size_t len);
#endif
