/* Stands in for OpenDingux GCW0's uClibc, whose <fcntl.h> declares
 * O_CLOEXEC only under _GNU_SOURCE. Used by uclibc-profile-check. */
#include_next <fcntl.h>
#undef O_CLOEXEC
