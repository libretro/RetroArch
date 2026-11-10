/* runahead_copy_test with the load check: a copy loads only once it
 * matches the running core, and a copy that doesn't is deleted */
#define HAVE_CORE_LOAD_CHECK 1
#include "runahead_copy_test.c"
