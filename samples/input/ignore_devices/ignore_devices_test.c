/* ignore_devices_test.c -- SDL_GAMECONTROLLER_IGNORE_DEVICES as the
 * joypad drivers read it (input/common/input_ignore_devices.h): the
 * controllers Steam Input stands in for, left to it.
 *
 *   1. The list as Steam writes it: hex ids with 0x, a slash, commas.
 *   2. Spaces, upper case, no 0x, an empty entry: read as SDL reads.
 *   3. A malformed entry is passed over and the next still read.
 *   4. Nothing set, or ids of 0: nothing is ignored.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/input_ignore_devices.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

int main(void)
{
   const char *steam = "0x054c/0x09cc,0x057e/0x2009";
   CHECK(input_ignore_devices_names(steam, 0x054c, 0x09cc), "the first entry is not read");
   CHECK(input_ignore_devices_names(steam, 0x057e, 0x2009), "the second entry is not read");
   CHECK(!input_ignore_devices_names(steam, 0x054c, 0x0ce6), "another product of the maker was taken");
   CHECK(!input_ignore_devices_names(steam, 0x09cc, 0x054c), "the ids were read the wrong way round");

   CHECK(input_ignore_devices_names(" 0X054C/0X0CE6 , ,28de/11ff", 0x28de, 0x11ff),
         "spaces, upper case, no 0x or an empty entry are not read as SDL reads them");
   CHECK(input_ignore_devices_names("0X054C/0X0CE6", 0x054c, 0x0ce6), "upper case is not read");
   CHECK(input_ignore_devices_names("junk,0x045e/0x02ff", 0x045e, 0x02ff),
         "an entry after a malformed one is not read");
   CHECK(!input_ignore_devices_names("0x045e/", 0x045e, 0), "an entry with no product matched");
   CHECK(!input_ignore_devices_names(NULL, 0x054c, 0x09cc), "a list that is not there matched");
   CHECK(!input_ignore_devices_names("", 0x054c, 0x09cc), "an empty list matched");

#if defined(_WIN32)
   _putenv("SDL_GAMECONTROLLER_IGNORE_DEVICES=0x054c/0x09cc");
#else
   setenv("SDL_GAMECONTROLLER_IGNORE_DEVICES", "0x054c/0x09cc", 1);
#endif
   CHECK(input_device_ignored(0x054c, 0x09cc), "the variable is not read");
   CHECK(!input_device_ignored(0, 0), "a controller with no ids was ignored");

   if (failures)
   {
      fprintf(stderr, "FAIL ignore_devices_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS ignore_devices_test: the list as Steam writes it and as SDL reads it\n");
   return 0;
}
