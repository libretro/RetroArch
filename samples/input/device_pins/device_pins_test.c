/* A port's keyboard, kept by what the keyboard is and not by its place
 * in a list: input/common/input_device_pins.h, on its own.
 *
 * Checked here:
 *
 * - identities: one of a kind keeps what the driver knows it by; the
 *   second and third of a model are told apart;
 * - two ports, each pinned to a keyboard: each reads its own, in
 *   whatever order the keyboards are listed;
 * - the first keyboard unplugged: the second, now first in the list,
 *   is still the second port's, and the first port reads none - not
 *   the other port's;
 * - a single port pinned to a keyboard that is away reads every
 *   keyboard, so that the player still has keys;
 * - a port with no pin reads every keyboard; one with a number and no
 *   pin (a setting from before pins) reads that number's keyboard, or
 *   every keyboard when the number is past the list;
 * - two keyboards of one model are two pins;
 * - mice: a port with no pin reads the mouse its number names, as it
 *   always did; a pinned port reads its mouse wherever it is listed;
 *   with that mouse away it reads none while another port has its
 *   own, and the mouse its number names otherwise. */
#include <stdio.h>
#include <string.h>

#include "../../../input/common/input_device_pins.h"

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define PORTS 4

int main(void)
{
   char base[4][INPUT_PIN_LEN], ident[4][INPUT_PIN_LEN];
   char pin[PORTS][INPUT_PIN_LEN];
   unsigned index[PORTS];
   int8_t choice[PORTS];

   /* ---- identities ------------------------------------------------ */
   memset(base, 0, sizeof(base));
   strcpy(base[0], "03f0:098f");
   strcpy(base[1], "046d:c31c");
   strcpy(base[2], "046d:c31c");
   strcpy(base[3], "046d:c31c");
   input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 4);
   CHECK(!strcmp(ident[0], "03f0:098f") && !strcmp(ident[1], "046d:c31c")
         && !strcmp(ident[2], "046d:c31c#2") && !strcmp(ident[3], "046d:c31c#3"),
         "identities: \"%s\" \"%s\" \"%s\" \"%s\"", ident[0], ident[1], ident[2], ident[3]);
   {
      /* a long name still has room for its number */
      char lb[2][INPUT_PIN_LEN], li[2][INPUT_PIN_LEN];
      memset(lb[0], 'k', INPUT_PIN_LEN - 1); lb[0][INPUT_PIN_LEN - 1] = '\0';
      memcpy(lb[1], lb[0], INPUT_PIN_LEN);
      input_pins_identities(li, (const char (*)[INPUT_PIN_LEN])lb, 2);
      CHECK(strcmp(li[0], li[1]) && strstr(li[1], "#2"),
            "two keyboards with one long name have one identity");
   }
   printf("   ok   identities: one of a kind keeps its own; the second and third of a model are told apart\n");

   /* ---- two ports, two keyboards ---------------------------------- */
   memset(pin, 0, sizeof(pin)); memset(index, 0, sizeof(index));
   memset(base, 0, sizeof(base));
   strcpy(base[0], "03f0:098f"); strcpy(base[1], "046d:c31c");
   input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 2);
   strcpy(pin[0], "03f0:098f"); strcpy(pin[1], "046d:c31c");
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 2);
   CHECK(choice[0] == 1 && choice[1] == 2 && choice[2] == INPUT_PIN_ALL,
         "two pinned ports: %d %d, and an unpinned one %d", choice[0], choice[1], choice[2]);
   /* the list in the other order */
   strcpy(base[0], "046d:c31c"); strcpy(base[1], "03f0:098f");
   input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 2);
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 2);
   CHECK(choice[0] == 2 && choice[1] == 1,
         "the keyboards listed the other way round: %d %d, want 2 1", choice[0], choice[1]);
   printf("   ok   each of two pinned ports reads its own keyboard, in whatever order they are listed\n");

   /* ---- the first keyboard unplugged ------------------------------ */
   memset(base, 0, sizeof(base));
   strcpy(base[0], "046d:c31c");
   input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 1);
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 1);
   CHECK(choice[1] == 1, "the second port's keyboard, now first in the list, is read as %d", choice[1]);
   CHECK(choice[0] == INPUT_PIN_NONE,
         "the first port, its keyboard away, reads %d: it must not read the other port's", choice[0]);
   printf("   ok   one keyboard unplugged: the other is still its own port's, and the port left without reads none\n");

   /* ---- one port, its keyboard away ------------------------------- */
   memset(pin, 0, sizeof(pin));
   strcpy(pin[0], "03f0:098f");
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 1);
   CHECK(choice[0] == INPUT_PIN_ALL,
         "a single pinned port whose keyboard is away reads %d: the player has no keys", choice[0]);
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 0);
   CHECK(choice[0] == INPUT_PIN_ALL, "with no keyboard listed at all a pinned port reads %d", choice[0]);
   printf("   ok   a single port whose pinned keyboard is away reads every keyboard\n");

   /* ---- a number and no pin --------------------------------------- */
   memset(pin, 0, sizeof(pin)); memset(index, 0, sizeof(index));
   index[0] = 1; index[1] = 5;
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 1);
   CHECK(choice[0] == 1 && choice[1] == INPUT_PIN_ALL && choice[2] == INPUT_PIN_ALL,
         "numbers without pins: %d %d %d, want 1, all, all", choice[0], choice[1], choice[2]);
   printf("   ok   no pin: every keyboard; a number from before pins: that keyboard, or every one when it is past the list\n");

   /* ---- two of one model ------------------------------------------ */
   memset(base, 0, sizeof(base)); memset(pin, 0, sizeof(pin)); memset(index, 0, sizeof(index));
   strcpy(base[0], "046d:c31c"); strcpy(base[1], "046d:c31c");
   input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 2);
   strcpy(pin[0], "046d:c31c#2"); strcpy(pin[1], "046d:c31c");
   input_pins_resolve(choice, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
         (const char (*)[INPUT_PIN_LEN])ident, 2);
   CHECK(choice[0] == 2 && choice[1] == 1, "two keyboards of one model: %d %d, want 2 1", choice[0], choice[1]);
   printf("   ok   two keyboards of one model are two pins\n");

   /* ---- mice ------------------------------------------------------ */
   {
      int16_t mouse[PORTS];
      memset(base, 0, sizeof(base)); memset(pin, 0, sizeof(pin));
      strcpy(base[0], "1b1c:1b5a"); strcpy(base[1], "046d:c08b"); strcpy(base[2], "03f0:098f");
      input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 3);
      /* every port has its own number from the start, and no pin */
      index[0] = 0; index[1] = 1; index[2] = 2; index[3] = 3;
      input_pins_resolve_mice(mouse, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
            (const char (*)[INPUT_PIN_LEN])ident, 3);
      CHECK(mouse[0] == 0 && mouse[1] == 1 && mouse[2] == 2 && mouse[3] == 3,
            "no pins: ports read mice %d %d %d %d, want their numbers", mouse[0], mouse[1], mouse[2], mouse[3]);
      /* two ports pinned, the other way round from their numbers */
      strcpy(pin[0], "046d:c08b"); strcpy(pin[1], "1b1c:1b5a");
      input_pins_resolve_mice(mouse, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
            (const char (*)[INPUT_PIN_LEN])ident, 3);
      CHECK(mouse[0] == 1 && mouse[1] == 0 && mouse[2] == 2,
            "two pinned ports: %d %d, and an unpinned one %d; want 1 0 2", mouse[0], mouse[1], mouse[2]);
      /* the first port's mouse unplugged: the list is one shorter */
      memset(base, 0, sizeof(base));
      strcpy(base[0], "1b1c:1b5a"); strcpy(base[1], "03f0:098f");
      input_pins_identities(ident, (const char (*)[INPUT_PIN_LEN])base, 2);
      input_pins_resolve_mice(mouse, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
            (const char (*)[INPUT_PIN_LEN])ident, 2);
      CHECK(mouse[1] == 0, "the second port's mouse is read as %d, want 0", mouse[1]);
      CHECK(mouse[0] == INPUT_PIN_NO_MOUSE,
            "the first port, its mouse away, reads %d: it must not read the other port's", mouse[0]);
      /* a single pinned port, its mouse away: the mouse its number names */
      memset(pin, 0, sizeof(pin));
      strcpy(pin[0], "046d:c08b");
      input_pins_resolve_mice(mouse, (const char (*)[INPUT_PIN_LEN])pin, index, PORTS,
            (const char (*)[INPUT_PIN_LEN])ident, 2);
      CHECK(mouse[0] == 0, "a single pinned port whose mouse is away reads %d, want the mouse its number names (0)", mouse[0]);
      printf("   ok   mice: no pin reads by number; a pinned port reads its mouse wherever it is listed; with it away, none while another port has its own, and by number otherwise\n");
   }

   if (failures)
   {
      printf("FAIL device_pins_test: %u\n", failures);
      return 1;
   }
   printf("PASS device_pins_test\n");
   return 0;
}
