# Pulls display pacing's wait, and what it tells "now" by, out of
# gfx/video_thread_wrapper.c, so pace_hold_start_test.c runs the
# wrapper's own code rather than a copy of it.
{ sub(/\r$/, "") }
/^#define VIDEO_THREAD_NOW\(info\)/ { mc = 1 }
mc { macro = macro $0 "\n"; if ($0 !~ /\\$/) mc = 0; next }
/^static retro_time_t video_thread_frame_now\(const video_frame_info_t \*info\)$/ { fn = 1 }
/^static void video_thread_hold_until\(thread_video_t \*thr,$/ { fn = 1 }
fn { body = body $0 "\n"; if ($0 ~ /^}/) { fn = 0; got++ } next }
END {
   if (macro == "" || got != 2)
   {
      print "extract_hold.awk: an anchor in video_thread_wrapper.c has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from gfx/video_thread_wrapper.c by extract_hold.awk. */"
   printf "%s\n%s", macro, body
}
