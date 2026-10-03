/* Running another program in place of this one. */

#ifndef GEKKO_EXEC_H
#define GEKKO_EXEC_H

#include <gekko/gekko.h>

/* Runs a DOL or ELF image, after the exit hooks, passing argv the way
 * loaders do: in the argument block after the '_arg' tag at its entry
 * point.  Returns only on failure: -ENOEXEC for an image that is not a
 * program or has no room to load, -E2BIG for arguments too long. */
int gk_exec(const void *image, size_t len, int argc, const char *const *argv);

#endif
