/* gfx/drivers/d3d9hlsl.c's menu vertex ring (d3d9_menu_ring_cases.h has
 * the cases). */

#include "../../../gfx/drivers/d3d9hlsl.c"
#include "d3d9_menu_ring_cases.h"

int main(void)
{
   return run_cases(gfx_display_d3d9_hlsl_draw, sizeof(hlsl_renderchain_t));
}
