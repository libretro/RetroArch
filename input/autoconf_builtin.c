#include "input_common.h"

// Some hardcoded autoconfig information. Will be used for pads with no autoconfig cfg files.
// All 4 almost-identical 360 pads are included, could be reduced with some fiddling.
const char* const input_builtin_autoconfs[] =
{
"\
input_device = \"XInput Controller (Player 1)\" \n\
input_driver = \"winxinput\"                    \n\
input_b_btn = \"0\"                             \n\
input_y_btn = \"2\"                             \n\
input_select_btn = \"7\"                        \n\
input_start_btn = \"6\"                         \n\
input_up_btn = \"h0up\"                         \n\
input_down_btn = \"h0down\"                     \n\
input_left_btn = \"h0left\"                     \n\
input_right_btn = \"h0right\"                   \n\
input_a_btn = \"1\"                             \n\
input_x_btn = \"3\"                             \n\
input_l_btn = \"4\"                             \n\
input_r_btn = \"5\"                             \n\
input_l2_axis = \"+4\"                          \n\
input_r2_axis = \"+5\"                          \n\
input_l3_btn = \"8\"                            \n\
input_r3_btn = \"9\"                            \n\
input_l_x_plus_axis = \"+0\"                    \n\
input_l_x_minus_axis = \"-0\"                   \n\
input_l_y_plus_axis = \"-1\"                    \n\
input_l_y_minus_axis = \"+1\"                   \n\
input_r_x_plus_axis = \"+2\"                    \n\
input_r_x_minus_axis = \"-2\"                   \n\
input_r_y_plus_axis = \"-3\"                    \n\
input_r_y_minus_axis = \"+3\"                   \n\
",

"\
input_device = \"XInput Controller (Player 2)\" \n\
input_driver = \"winxinput\"                    \n\
input_b_btn = \"0\"                             \n\
input_y_btn = \"2\"                             \n\
input_select_btn = \"7\"                        \n\
input_start_btn = \"6\"                         \n\
input_up_btn = \"h0up\"                         \n\
input_down_btn = \"h0down\"                     \n\
input_left_btn = \"h0left\"                     \n\
input_right_btn = \"h0right\"                   \n\
input_a_btn = \"1\"                             \n\
input_x_btn = \"3\"                             \n\
input_l_btn = \"4\"                             \n\
input_r_btn = \"5\"                             \n\
input_l2_axis = \"+4\"                          \n\
input_r2_axis = \"+5\"                          \n\
input_l3_btn = \"8\"                            \n\
input_r3_btn = \"9\"                            \n\
input_l_x_plus_axis = \"+0\"                    \n\
input_l_x_minus_axis = \"-0\"                   \n\
input_l_y_plus_axis = \"-1\"                    \n\
input_l_y_minus_axis = \"+1\"                   \n\
input_r_x_plus_axis = \"+2\"                    \n\
input_r_x_minus_axis = \"-2\"                   \n\
input_r_y_plus_axis = \"-3\"                    \n\
input_r_y_minus_axis = \"+3\"                   \n\
",

"\
input_device = \"XInput Controller (Player 3)\" \n\
input_driver = \"winxinput\"                    \n\
input_b_btn = \"0\"                             \n\
input_y_btn = \"2\"                             \n\
input_select_btn = \"7\"                        \n\
input_start_btn = \"6\"                         \n\
input_up_btn = \"h0up\"                         \n\
input_down_btn = \"h0down\"                     \n\
input_left_btn = \"h0left\"                     \n\
input_right_btn = \"h0right\"                   \n\
input_a_btn = \"1\"                             \n\
input_x_btn = \"3\"                             \n\
input_l_btn = \"4\"                             \n\
input_r_btn = \"5\"                             \n\
input_l2_axis = \"+4\"                          \n\
input_r2_axis = \"+5\"                          \n\
input_l3_btn = \"8\"                            \n\
input_r3_btn = \"9\"                            \n\
input_l_x_plus_axis = \"+0\"                    \n\
input_l_x_minus_axis = \"-0\"                   \n\
input_l_y_plus_axis = \"-1\"                    \n\
input_l_y_minus_axis = \"+1\"                   \n\
input_r_x_plus_axis = \"+2\"                    \n\
input_r_x_minus_axis = \"-2\"                   \n\
input_r_y_plus_axis = \"-3\"                    \n\
input_r_y_minus_axis = \"+3\"                   \n\
",

"\
input_device = \"XInput Controller (Player 4)\" \n\
input_driver = \"winxinput\"                    \n\
input_b_btn = \"0\"                             \n\
input_y_btn = \"2\"                             \n\
input_select_btn = \"7\"                        \n\
input_start_btn = \"6\"                         \n\
input_up_btn = \"h0up\"                         \n\
input_down_btn = \"h0down\"                     \n\
input_left_btn = \"h0left\"                     \n\
input_right_btn = \"h0right\"                   \n\
input_a_btn = \"1\"                             \n\
input_x_btn = \"3\"                             \n\
input_l_btn = \"4\"                             \n\
input_r_btn = \"5\"                             \n\
input_l2_axis = \"+4\"                          \n\
input_r2_axis = \"+5\"                          \n\
input_l3_btn = \"8\"                            \n\
input_r3_btn = \"9\"                            \n\
input_l_x_plus_axis = \"+0\"                    \n\
input_l_x_minus_axis = \"-0\"                   \n\
input_l_y_plus_axis = \"-1\"                    \n\
input_l_y_minus_axis = \"+1\"                   \n\
input_r_x_plus_axis = \"+2\"                    \n\
input_r_x_minus_axis = \"-2\"                   \n\
input_r_y_plus_axis = \"-3\"                    \n\
input_r_y_minus_axis = \"+3\"                   \n\
",

"\
input_device = \"Dual Trigger 3-in-1\" \n\
input_driver = \"dinput\"              \n\
input_b_btn = \"1\"                    \n\
input_y_btn = \"0\"                    \n\
input_select_btn = \"8\"               \n\
input_start_btn = \"9\"                \n\
input_up_btn = \"h0up\"                \n\
input_down_btn = \"h0down\"            \n\
input_left_btn = \"h0left\"            \n\
input_right_btn = \"h0right\"          \n\
input_a_btn = \"2\"                    \n\
input_x_btn = \"3\"                    \n\
input_l_btn = \"4\"                    \n\
input_r_btn = \"5\"                    \n\
input_l2_btn = \"6\"                   \n\
input_r2_btn = \"7\"                   \n\
input_l3_btn = \"10\"                  \n\
input_r3_btn = \"11\"                  \n\
input_l_x_plus_axis = \"+0\"           \n\
input_l_x_minus_axis = \"-0\"          \n\
input_l_y_plus_axis = \"+1\"           \n\
input_l_y_minus_axis = \"-1\"          \n\
input_r_x_plus_axis = \"+2\"           \n\
input_r_x_minus_axis = \"-2\"          \n\
input_r_y_plus_axis = \"+5\"           \n\
input_r_y_minus_axis = \"-5\"          \n\
",

NULL
};
