#include "devoptab_fsa.h"

int __fsa_utimes(struct _reent *r,
                 const char *filename,
                 const struct timeval times[2]) {
    r->_errno = ENOSYS;
    return -1;
}
