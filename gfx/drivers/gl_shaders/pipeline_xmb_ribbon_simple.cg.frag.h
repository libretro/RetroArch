#include "shaders_common.h"

static const char *stock_fragment_xmb_ribbon_simple = CG(
   void main(
      float4 out oColor : COLOR)
   {
      oColor = float4(0.05, 0.05, 0.05, 1.0);
   }
);
