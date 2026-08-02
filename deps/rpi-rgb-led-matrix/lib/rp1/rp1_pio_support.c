/*
 * Pull the small vendor piolib implementation into one local translation unit.
 *
 * The project build files only need to know about rp1_pio_support.c; this
 * wrapper keeps the vendored RP1 C sources grouped together for both Make and
 * CMake and avoids sprinkling vendor-specific source lists through the tree.
 */
#include "rp1_pio_vendor/piolib/pio_rp1.c"
#include "rp1_pio_vendor/piolib/piolib.c"
