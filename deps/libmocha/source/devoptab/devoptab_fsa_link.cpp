#include "devoptab_fsa.h"

int __fsa_link(struct _reent *r,
               const char *existing,
               const char *newLink) {
    r->_errno = ENOSYS;
    return -1;
}
