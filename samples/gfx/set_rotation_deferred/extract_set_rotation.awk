# Pulls video_driver_set_rotation() out of gfx/video_driver.c, so
# set_rotation_deferred_test.c runs the driver's own code.
{ sub(/\r$/, "") }
/^bool video_driver_set_rotation\(unsigned rotation\)$/ { fn = 1 }
fn { body = body $0 "\n"; if ($0 ~ /^}/) fn = 0; next }
END {
   if (body == "")
   {
      print "extract_set_rotation.awk: the anchor in video_driver.c has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from gfx/video_driver.c by extract_set_rotation.awk. */"
   printf "%s", body
}
