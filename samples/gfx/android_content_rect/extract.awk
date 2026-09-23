# Prints the named static functions (FUNCS, comma-separated) from the
# source it is given, whole: from the line that opens the definition to
# the closing brace in column zero.
BEGIN { n = split(FUNCS, want, ","); for (i = 1; i <= n; i++) need[want[i]] = 1 }
{ sub(/\r$/, "") }
!in_fn && /^static / {
   for (f in need)
      if (index($0, " " f "(")) { in_fn = 1; found[f] = 1 }
}
in_fn { print; if ($0 ~ /^}/) { in_fn = 0; print "" } }
END {
   for (f in need)
      if (!(f in found))
      {
         print "extract.awk: " f " not found" > "/dev/stderr"
         exit 1
      }
}
