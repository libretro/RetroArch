#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_fchmod(struct _reent *r,
                 void *fd,
                 mode_t mode) {
    // FSChangeMode on open files is not possible on Cafe OS
    r->_errno = ENOSYS;
    return -1;
}
