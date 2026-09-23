# Pulls the device-clock word's three uses out of audio/drivers/asio.c -
# the publish in the callback, the reset on a new session, and the
# reader the statistics overlay calls - so asio_clock_test.c runs the
# driver's own code rather than a copy of it.
{ sub(/\r$/, "") }
/^static bool ra_asio_device_clock_ppm\(/ { rd = 1 }
rd { reader = reader $0 "\n"; if ($0 ~ /^}/) rd = 0; next }

!pubdone && /if \(ppm > -100000\.0 && ppm < 100000\.0\)/ {
   pub = 1; pubdone = 1; match($0, /^ */); ind = substr($0, 1, RLENGTH)
}
pub {
   publish = publish $0 "\n"
   if ($0 == ind "}") pub = 0
   next
}

/The clock measurement starts again with this session/ { rs = 1 }
rs { if ($0 ~ /^[ \t\r]*$/) rs = 0; else reset = reset $0 "\n"; next }

END {
   if (reader == "" || publish == "" || reset == "")
   {
      print "extract_clock.awk: an anchor in asio.c has moved" > "/dev/stderr"
      exit 1
   }
   print "/* Generated from audio/drivers/asio.c by extract_clock.awk. */"
   print "static void asio_drv_publish(ra_asio_t *ad, double ppm)\n{"
   printf "%s}\n\n", publish
   print "static void asio_drv_reset(ra_asio_t *ad)\n{"
   printf "%s}\n\n", reset
   printf "%s", reader
}
