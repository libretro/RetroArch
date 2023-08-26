#include "webhooks_macro_parser.h"

int wmp_parse_macro(const char* macro, rc_runtime_t* runtime)
{
  //  For now, the parser acts as a Facade to the rc_runtime_activate_richpresence
  //  that actually does the parse of the macro.
  //  This gives an opportunity to branch out in the future in case the
  //  need to Rich Presence diverge from the need of the webhooks.

  int result = rc_runtime_activate_richpresence(runtime, macro, NULL, 0);

  return result;
}
