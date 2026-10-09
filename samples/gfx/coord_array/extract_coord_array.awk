# Pulls the coordinate array (realloc_checked() through
# video_coord_array_free()) out of gfx/video_driver.c, so
# coord_array_test.c runs the driver's own code rather than a copy.
{ sub(/\r$/, "") }
/^static INLINE bool realloc_checked\(/ { on = 1 }
on { body = body $0 "\n" }
on && /^void video_coord_array_free\(/ { last = 1 }
on && last && /^}/ { on = 0; done = 1 }
END {
   if (!done)
   {
      print "extract_coord_array.awk: an anchor in video_driver.c has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from gfx/video_driver.c by extract_coord_array.awk. */"
   printf "%s", body
}
