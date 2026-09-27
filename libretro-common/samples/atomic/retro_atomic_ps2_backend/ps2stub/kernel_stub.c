#include "kernel.h"

int ps2stub_eie      = 1;
int ps2stub_di_calls = 0;
int ps2stub_ei_calls = 0;
int ps2stub_nest     = 0;
int ps2stub_max_nest = 0;

int DIntr(void)
{
   int was = ps2stub_eie;

   ps2stub_di_calls++;
   ps2stub_eie = 0;

   if (was)
   {
      ps2stub_nest++;
      if (ps2stub_nest > ps2stub_max_nest)
         ps2stub_max_nest = ps2stub_nest;
   }

   return was;
}

int EIntr(void)
{
   int was = ps2stub_eie;

   ps2stub_ei_calls++;
   ps2stub_eie = 1;
   ps2stub_nest--;

   return was;
}
