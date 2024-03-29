/* Copyright  (C) 2023 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (filtered_poll.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <features/features_cpu.h>
#include <filtered_poll.h>

static struct fp_codes_for_filter *fp_codes_filters   = NULL;
static int fp_actions_array_size                      = 0;
static struct fp_retro_code *fp_code                  = NULL;
static struct fp_filter_state *fp_filter_states       = NULL;
static retro_input_state_t default_state_cb           = NULL;
static int run_filter                                 = 0;
static int initialized                                = 0;

/* Intercept keyboard and joypad button saved poll data being returned by RetroArch
 * to core (input callback) and filter the value if depending on filter settings coming from the core */
int16_t core_input_state_filtered_poll_return_cb_override(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
  int16_t pressed;
  int i, j;

  int action;
  int run_loop;
  int64_t time_since_press;

  int time_lockout_updated;

  int a;
  int row_moved;

  int s;

  /* Get saved polled value of keyboard or joypad button currently being checked with original input
   * callback function */
  pressed = default_state_cb(port, device, idx, id);

  /* Only filter if a keyboard or joypad button */
  if (    run_filter
      &&  (device == RETRO_DEVICE_KEYBOARD || device == RETRO_DEVICE_JOYPAD))
  {
    /* Don't do anything if no filters are set */
    if (fp_codes_filters != NULL)
    {
      /* Initialize opeation data table if this has not been done yet */
      if ((!initialized) && (fp_filter_states != NULL))
      {
        for (i = 0; i < fp_actions_array_size; i++)
        {
          fp_filter_states[i].player = fp_codes_filters[i].player;
          fp_filter_states[i].type = fp_codes_filters[i].type;
          for (j = 0; j < MAX_OPERATIONS; j++)
          {
            fp_filter_states[i].state[j] = 0;
          }
        }
        initialized = 1;

      }

      action = 0;
      run_loop = 1;

      /* Find first filter to check if there are any */
      if ((!fp_codes_filters[action].player) && (fp_codes_filters[action].next_active_position))
        action = fp_codes_filters[action].next_active_position;
      else run_loop = 0;

      /* Check all active filters if there are any */
      if (run_loop)
      {
        do
        {
          /* If enabled, check filter that can lockout a button for a certain amount of time */
          if (fp_codes_filters[action].operations & FP_TIMELOCKOUT)
          {
            time_lockout_updated = 0;
            /* Check through all button codes for an action to see if one of them match
             * the requested code / button state */
            for (i = 0; i < fp_codes_filters[action].codes_array_length; i++)
            {
              if (fp_codes_filters[action].codes[i].id == NO_CODE)
                break;
              /* If requested port, device, idx and code (id) equals
               * currently port, device, idx and checked code (id), perform operation */
              if (      port   == fp_codes_filters[action].codes[i].port
                    &&  device == fp_codes_filters[action].codes[i].device
                    &&  idx    == fp_codes_filters[action].codes[i].idx
                    &&  id     == fp_codes_filters[action].codes[i].id)
              {
                /* If row from operation definition table is not equal to row
                 * from operation data table (due to port numbers changing),
                 * find operation data row and move to correct position */
                if (( fp_codes_filters[action].player != fp_filter_states[action].player) ||
                     (fp_codes_filters[action].type   != fp_filter_states[action].type))
                {
                  a = 0;
                  row_moved = 0;
                  while ((a < fp_actions_array_size) && !row_moved)
                  {
                    if (( fp_codes_filters[action].player == fp_filter_states[a].player) &&
                         (fp_codes_filters[action].type   == fp_filter_states[a].type))
                    {
                      fp_filter_states[action].player = fp_filter_states[a].player;
                      fp_filter_states[a].player = 0;
                      fp_filter_states[action].type   = fp_filter_states[a].type;
                      fp_filter_states[a].type = 0;
                      for (s = 0; s < MAX_OPERATIONS; s++)
                      {
                        fp_filter_states[action].state[s] = fp_filter_states[a].state[s];
                        fp_filter_states[a].state[s] = 0;
                      }
                      row_moved = 1;
                    }
                    a++;
                  }
                }

                /* Only check to see if filter should be applied if button is pressed
                 * since this particular filter will not "press" an unpressed button */
                if (pressed)
                {
                  /* Set to current time to initialize if 0 */
                  if (!fp_filter_states[action].state[0])
                    fp_filter_states[action].state[0] = (int64_t) cpu_features_get_time_usec();
                  else
                  {
                    /* Check time since last button press */
                    time_since_press = (int64_t) cpu_features_get_time_usec()
                                              - fp_filter_states[action].state[0];
                    /* Return button not pressed if time is less than lockout time */
                    if (time_since_press < fp_codes_filters[action].modifier[0] * 1000)
                      return 0;
                    else
                      /* Reset to current time so time since last button press is "0" */
                      fp_filter_states[action].state[0] = (int64_t) cpu_features_get_time_usec();
                  }
                }
              }
            }
          }
          /* example of 'if statement' to define a second operation / filter if needed in the future */
/*        if (fp_codes_filters[action].operations & FP_NAME_OF_OPERATION2)
          {
          } */

          /* example of 'if statement' to define a third operation / filter if needed in the future */
/*        if (fp_codes_filters[action].operations & FP_NAME_OF_OPERATION3)
          {
          } */

          /* example of 'if statement' to define a fourth operation / filter if needed in the future */
/*        if (fp_codes_filters[action].operations & FP_NAME_OF_OPERATION4)
          {
          } */

          /* Goto next active operation definition row if there is one */
          if (fp_codes_filters[action].next_active_position)
            action = fp_codes_filters[action].next_active_position;
          else
            run_loop = 0;
        } while (run_loop);
      }
    }
  }

  return pressed;
}

/* Per thoe standard RetroArch saved poll code state callback function, this
 * also calls the actual function that does the work.  It has to be done
 * exactly this way it appears because the callbock function in the core
 * is called with 4 function arguments where this function has none,
 * but the function it calls has 4 */
static retro_input_state_t retro_core_input_state_filtered_poll_return_cb(void)
{
  return core_input_state_filtered_poll_return_cb_override;
}

/* Get saved poll code state callback function, this function called static function
 * defined above in this file */
retro_input_state_t retro_get_core_input_state_filtered_poll_return_cb(void)
{
  return retro_core_input_state_filtered_poll_return_cb();
}

/* Set to 0 to not run filter at all, or 1 to run filter */
void retro_set_filtered_poll_run_filter(int runFilter)
{
  run_filter = runFilter;
}

/* Set to point to original callback function that was in place
 * before the custom one above */
void retro_set_filtered_poll_original_cb(retro_input_state_t defaultStateCb)
{
  default_state_cb = defaultStateCb;
}

void retro_set_filtered_poll_variables(   int fpActionsArraySize,
                                          struct fp_codes_for_filter *codesFilter,
                                          struct fp_filter_state *filterState,
                                          void *fpCode,
                                          int fpCodesArrayLength)
{
  int i;

  fp_actions_array_size = fpActionsArraySize;   /* Total number of actions or ports to check */
  fp_codes_filters = codesFilter;               /* Declared operation definition table */
  fp_filter_states = filterState;               /* Declared operation data table */
  fp_code = (struct fp_retro_code *) fpCode;    /* All codes used in all actions with their associated player, device, idx */

  for (i=0; i < fp_actions_array_size; i++) /* Save these codes to operations definition table */
  {
    fp_codes_filters[i].codes = (struct fp_retro_code *) fp_code + (i * fpCodesArrayLength);
    fp_codes_filters[i].codes_array_length = fpCodesArrayLength;
  }
}
