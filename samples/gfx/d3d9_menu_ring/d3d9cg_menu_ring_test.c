/* gfx/drivers/d3d9cg.c's menu vertex ring (d3d9_menu_ring_cases.h has
 * the cases). */

#include "../../../gfx/drivers/d3d9cg.c"
#include "d3d9_menu_ring_cases.h"

int main(void)
{
   return run_cases(gfx_display_d3d9_cg_draw, sizeof(cg_renderchain_t));
}
