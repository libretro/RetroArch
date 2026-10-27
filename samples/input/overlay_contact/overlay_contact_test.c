/* A finger's contact on an overlay D-pad or face-button area
 * (input/common/overlay_contact.h): the ways the ellipse a touchscreen
 * reports covers, from the area's middle. */

#include <stdio.h>

#include "common/overlay_contact.h"

static unsigned failures;

#define CHECK(cond, msg) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)

#define U OVERLAY_CONTACT_UP
#define D OVERLAY_CONTACT_DOWN
#define L OVERLAY_CONTACT_LEFT
#define R OVERLAY_CONTACT_RIGHT
#define HALF_PI 1.5707963f

int main(void)
{
   /* an area 200 pixels across on a square screen: a pixel is 1/100
    * of a half-width */
   const float px = 0.01f;

   /* no contact size - a mouse, a pen: the way the centre lies */
   CHECK(overlay_contact_dirs(0.6f, 0.1f, 0, 0, 0.0f, px, px, 1.0f) == R,
         "with no size, right of the middle is not right");
   CHECK(overlay_contact_dirs(0.1f, -0.6f, 0, 0, 0.0f, px, px, 1.0f) == U,
         "with no size, above the middle is not up");
   CHECK(overlay_contact_dirs(0.0f, 0.0f, 0, 0, 0.0f, px, px, 1.0f) == 0,
         "the middle itself pressed a way");
   CHECK(overlay_contact_dirs(0.03f, -0.02f, 0, 0, 0.0f, px, px, 1.0f) == 0,
         "a hair from the middle pressed a way");

   /* a fingertip well inside the right: right alone */
   CHECK(overlay_contact_dirs(0.7f, 0.0f, 20, 16, 0.0f, px, px, 1.0f) == R,
         "a fingertip on the right pressed more than right");

   /* a thumb laid across up and right */
   CHECK(overlay_contact_dirs(0.4f, -0.4f, 60, 40, 0.785f, px, px, 1.0f) == (U | R),
         "a thumb across up and right is not up and right");

   /* a thumb between the right and the bottom face buttons */
   CHECK(overlay_contact_dirs(0.45f, 0.45f, 70, 50, -0.785f, px, px, 1.0f) == (D | R),
         "a thumb between A and B is not both");

   /* upright and long, on the middle: it reaches up and down; its
    * minor axis, short, reaches neither side */
   CHECK(overlay_contact_dirs(0.0f, 0.0f, 120, 2, 0.0f, px, px, 1.0f) == (U | D),
         "an upright contact on the middle is not up and down");
   /* turned a quarter, it reaches the sides */
   CHECK(overlay_contact_dirs(0.0f, 0.0f, 120, 2, HALF_PI, px, px, 1.0f) == (L | R),
         "a level contact on the middle is not left and right");

   /* how large it counts: too small to cross into up, then doubled */
   CHECK(overlay_contact_dirs(0.5f, -0.2f, 20, 20, 0.0f, px, px, 1.0f) == R,
         "a small contact right of the middle crossed into up");
   CHECK(overlay_contact_dirs(0.5f, -0.2f, 20, 20, 0.0f, px, px, 4.0f) == (U | R),
         "magnified, it did not reach up");

   /* a wide screen: the same pixels are less of the area across than
    * up and down - it reaches up but not the side */
   CHECK(overlay_contact_dirs(0.0f, 0.0f, 60, 60, 0.0f, 0.001f, 0.02f, 1.0f) == (U | D),
         "on a wide screen, the contact's width counted as its height");

   /* the packing the frontend keeps contacts in */
   CHECK(OVERLAY_CONTACT_MAJOR(OVERLAY_CONTACT_PACK(300, 120)) == 300
         && OVERLAY_CONTACT_MINOR(OVERLAY_CONTACT_PACK(300, 120)) == 120,
         "the axes do not come back as packed");
   CHECK(OVERLAY_CONTACT_MAJOR(OVERLAY_CONTACT_PACK(70000, 1)) == 0xffff,
         "an axis past 16 bits was not held to the most");
   CHECK(OVERLAY_CONTACT_ANGLE(OVERLAY_CONTACT_ANGLE_Q(-1.2345f)) > -1.2346f
         && OVERLAY_CONTACT_ANGLE(OVERLAY_CONTACT_ANGLE_Q(-1.2345f)) < -1.2344f,
         "the angle does not come back to a ten-thousandth");

   if (failures)
   {
      fprintf(stderr, "FAIL overlay_contact_test: %u failures\n", failures);
      return 1;
   }
   printf("[pass] overlay_contact_test: a contact presses the ways it covers"
         ", with no size the way its centre lies, sized per axis\n");
   return 0;
}
