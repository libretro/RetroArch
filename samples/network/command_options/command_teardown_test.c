#include <stdio.h>
#include "../../../input/input_driver.h"
#include "../../../command.h"

static unsigned destroyed;

static void command_test_destroy(command_t *cmd)
{
   (void)cmd;
   destroyed++;
}

int main(void)
{
   input_driver_state_t *st = input_state_get_ptr();
   command_t cmd = {0};
   int16_t analog[4];
   unsigned generation = input_driver_command_generation();
   unsigned p, a;

   cmd.destroy = command_test_destroy;
   st->command[0] = &cmd;
   for (p = 0; p < MAX_USERS; p++)
   {
      input_driver_command_hold(p, 8, 1);
      for (a = 0; a < 4; a++)
         input_driver_command_hold(p, 16 + a, -1234);
   }
   if (!st->cmd_input)
      return 1;
   input_driver_deinit_command(st);
   if (destroyed != 1 || st->command[0] || st->cmd_input
         || input_driver_command_generation() == generation)
      return 1;
   for (p = 0; p < MAX_USERS; p++)
   {
      if (input_driver_command_held(p, analog))
         return 1;
      for (a = 0; a < 4; a++)
         if (analog[a])
            return 1;
   }
   input_driver_deinit_command(st);
   if (destroyed != 1 || st->cmd_input)
      return 1;
   puts("command_teardown: PASS");
   return 0;
}
