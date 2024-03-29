/* Copyright  (C) 2023 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (filtered_poll.h).
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

#ifndef FILTERED_POLL_H__
#define FILTERED_POLL_H__

#include <libretro.h>

RETRO_BEGIN_DECLS

enum    /* type */
{
  FP_ANALOG_INC = 1, FP_ANALOG_DEC
};

#define MAX_OPERATIONS 1

        /* operation */
#define FP_TIMELOCKOUT  0x0001
/* Additional operations that could be defined are below. Up to 3 more could be defined
 * along with code being added in filtered_poll.c to define these added operations */
/* #define FP_NAME_OF_OPERATION2  0x0010 */
/* #define FP_NAME_OF_OPERATION3  0x0100 */
/* #define FP_NAME_OF_OPERATION4  0x1000 */

#define NO_CODE 0x9000

struct fp_retro_code
{
  unsigned port;
  unsigned device;
  unsigned idx;
  unsigned id;    /* key or joy code */
};

/* Row of operations definition table */
struct fp_codes_for_filter
{
  int player;
  int type;                      /* action type, example FP_ANALOG_INC and FP_ANALOG_DEC definied above */
  uint16_t operations;           /* filter operation, FP_TIMELOCKOUT is the only one currently defined */
  int modifier[MAX_OPERATIONS];  /* reference data that can be used for this player, type, and operation */
  struct fp_retro_code *codes;   /* all key and / or joy codes assigned to this action to check */
  int codes_array_length;        /* length of array assigned to *codes */
  int next_active_position;      /* assigned to next active row for filtering */
};

/* Row of operations data table */
struct fp_filter_state
{
  int player;
  int type;
  int64_t state[MAX_OPERATIONS]; /* saved variable that can be used for this player, type, and operation */
};

int16_t core_input_state_filtered_poll_return_cb_override(unsigned port,
      unsigned device, unsigned idx, unsigned id);

/* typedefs and #if statements with matching #else and #end if statment are commented out starting with /***
 * when in the RetroArch project, but are uncommented when used in a core module. */

/*** #if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) */
/* Setup typedef as function pointer template to get access to function in main executables
 * in Windows environments. */
/*** typedef retro_input_state_t (*retro_get_core_input_state_filtered_poll_return_cb_FUNC)(void); */
/*** typedef void (*retro_set_filtered_poll_run_filter_FUNC)(int runFilter); */
/*** typedef void (*retro_set_filtered_poll_original_cb_FUNC)(retro_input_state_t defaultStateCb); */
/*** typedef void (*retro_set_filtered_poll_variables_FUNC)( int fpActionsArraySize, */
                                                        /*** struct fp_codes_for_filter *codesFilter, */
                                                        /*** struct fp_filter_state *filterState, */
                                                        /*** void *fpCode, */
                                                        /*** int fpCodesArrayLength); */

/*** RETRO_END_DECLS */
/*** #else */
RETRO_END_DECLS
/* Just set up header which is supported on other OSs */
RETRO_API retro_input_state_t retro_get_core_input_state_filtered_poll_return_cb(void);
RETRO_API void retro_set_filtered_poll_run_filter(int runFilter);
RETRO_API void retro_set_filtered_poll_original_cb(retro_input_state_t defaultStateCb);
RETRO_API void retro_set_filtered_poll_variables(       int fpActionsArraySize,
                                                        struct fp_codes_for_filter *codesFilter,
                                                        struct fp_filter_state *filterState,
                                                        void *fpCode,
                                                        int fpCodesArrayLength);
/*** #endif */

#endif
