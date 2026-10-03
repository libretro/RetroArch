# Pulls the viewport parameter snapshot and the integer scaler out of
# gfx/video_driver.c, so integer_scale_axis_test.c runs the driver's
# own code rather than a copy of it.
{ sub(/\r$/, "") }
/^struct video_vp_param_snap$/ { sn = 1 }
sn { snap = snap $0 "\n"; if ($0 ~ /^};/) sn = 0; next }
/^static void video_viewport_get_scaled_integer\($/ { fn = 1 }
fn { body = body $0 "\n"; if ($0 ~ /^}/) fn = 0; next }
END {
   if (snap == "" || body == "")
   {
      print "extract_integer.awk: an anchor in video_driver.c has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from gfx/video_driver.c by extract_integer.awk. */"
   printf "%s\n%s", snap, body
}
