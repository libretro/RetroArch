#include "rglgen.h"
#include "glsym.h"
#include <string.h>

void rglgen_resolve_symbols_custom(rglgen_proc_address_t proc,
      const struct rglgen_sym_map *map)
{
   for (; map->sym; map++)
   {
      rglgen_func_t func = proc(map->sym);
      memcpy(map->ptr, &func, sizeof(func));
   }
}

void rglgen_resolve_symbols(rglgen_proc_address_t proc)
{
   rglgen_resolve_symbols_custom(proc, rglgen_symbol_map);
}

