/* Very simple grayscale filter.
 * Author: Hans-Kristian Arntzen
 * License: Public domain
 */

#include "grayscale.h"

// Input format 0RRRRRGGGGGBBBBB. Colors themselves could be switched around.
void grayscale_filter(uint16_t *data, int width, int height)
{
   for (int i = 0; i < width * height; i++ )
   {
      int r, g, b, color;
      r = (data[i] >> 10) & 0x1F;
      g = (data[i] >> 5)  & 0x1F;
      b = (data[i] >> 0)  & 0x1F;

      color = (int)(r * 0.3 + g * 0.59 + b * 0.11);
      data[i] = (color << 10) | (color << 5) | color;
   }
}
