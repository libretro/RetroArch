/* gfx/drivers/d3d9hlsl.c's preset setup (d3d9_last_pass_cases.h has the
 * cases): a stock pass is appended after the last pass only when drawing
 * that pass straight to the backbuffer would give a different image. */

#include "../../../gfx/drivers/d3d9hlsl.c"
#include "d3d9_last_pass_cases.h"

int main(void)
{
   return run_cases(d3d9_init_multipass);
}
