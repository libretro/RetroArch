# Pulls the Scanline Sync wait out of gfx/video_driver.c and its state's
# indices out of gfx/video_driver.h, so scanline_wait_test.c runs the
# driver's own code rather than a copy of it.
{ sub(/\r$/, "") }
/^enum video_driver_scanline$/ { en = 1 }
en { idx = idx $0 "\n"; if ($0 ~ /^};/) en = 0; next }
/^VIDEO_NOINLINE static void video_driver_scanline_after_frame\(video_driver_state_t \*video_st,$/ { fn = 1 }
fn { body = body $0 "\n"; if ($0 ~ /^}/) fn = 0; next }
END {
   if (idx == "" || body == "")
   {
      print "extract_scanline.awk: an anchor in video_driver.[ch] has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from gfx/video_driver.[ch] by extract_scanline.awk. */"
   printf "%s\n%s", idx, body
}
