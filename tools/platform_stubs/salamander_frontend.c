/* Host frontend driver and main for the salamander object-set lane:
 * enough of frontend_driver.c for frontend_salamander.c, so each
 * launcher's libretro-common list is linked as that launcher links it,
 * and a unit in the list that needs one outside it fails here. */
#include <stddef.h>
#include <string.h>
#include <boolean.h>
#include <compat/strl.h>
#include "../../frontend/frontend_driver.h"

int salamander_main(int argc, char *argv[]);

static frontend_ctx_driver_t salamander_frontend_stub;

frontend_ctx_driver_t *frontend_ctx_init_first(void)
{
   salamander_frontend_stub.ident = "stub";
   return &salamander_frontend_stub;
}

size_t frontend_driver_get_core_extension(char *s, size_t len)
{
   return strlcpy(s, "elf", len);
}

bool frontend_driver_get_salamander_basename(char *s, size_t len)
{
   strlcpy(s, "retroarch.elf", len);
   return true;
}

void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }

int main(int argc, char *argv[])
{
   return salamander_main(argc, argv) ? 0 : 1;
}
