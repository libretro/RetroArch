# Prints the named functions (FUNCS, comma-separated) from the source it
# is given, whole: from the line that opens the definition to the
# closing brace in column zero. A prototype is skipped.
BEGIN { n = split(FUNCS, want, ","); for (i = 1; i <= n; i++) need[want[i]] = 1 }
{ sub(/\r$/, "") }
!in_fn && /^[A-Za-z]/ {
   for (f in need)
      if (index($0, " " f "(") || index($0, "*" f "("))
      {
         in_fn = 1; body = 0; buf = ""; cur = f
      }
}
in_fn {
   buf = buf $0 "\n"
   if (!body && $0 ~ /\);[ \t]*$/) { in_fn = 0; next }
   if ($0 ~ /^{/) body = 1
   if (body && $0 ~ /^}/) { printf "%s\n", buf; found[cur] = 1; in_fn = 0 }
}
END {
   for (f in need)
      if (!(f in found))
      {
         print "extract.awk: " f " not found" > "/dev/stderr"
         exit 1
      }
}
