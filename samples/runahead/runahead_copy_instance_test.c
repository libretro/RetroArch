/* runahead_copy_check_test with Android 6+'s second instance: loaded
 * from the running core's own file, the copy made only when it can't */
#define HAVE_CORE_LOAD_CHECK 1
#define RUNAHEAD_INSTANCE_LOAD 1
#include "runahead_copy_test.c"
