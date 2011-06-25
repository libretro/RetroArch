COMMAND_LINE_OPTS_ENABLE=""

add_command_line_enable()
{
   COMMAND_LINE_OPTS_ENABLE="$COMMAND_LINE_OPTS_ENABLE:\"$1\" \"$2\" \"$3\":"
   eval HAVE_$1=$3
}

add_command_line_string()
{
   COMMAND_LINE_OPTS_STRINGS="$COMMAND_LINE_OPTS_STRINGS:\"$1\" \"$2\" \"$3\":"
   eval $1=$3
}

## lvl. 43 regex dragon awaits thee.
print_help()
{
   echo "===================="
   echo " Quickbuild script"
   echo "===================="
   echo "Package: $PACKAGE_NAME"
   echo "Version: $PACKAGE_VERSION"
   echo ""
   echo "General environment variables:"
   echo "CC:         C compiler"
   echo "CFLAGS:     C compiler flags"
   echo "CXX:        C++ compiler"
   echo "CXXFLAGS:   C++ compiler flags"
   echo "LDFLAGS:    Linker flags"
   echo ""
   echo "General options:"
   echo "--prefix=\$path: Install path prefix"
   echo "--help: Show this help"
   echo ""
   echo "Custom options:"

   tmpopts="$COMMAND_LINE_OPTS_ENABLE"
   while [ ! -z "$tmpopts" ]
   do
      subopts="`echo $tmpopts | sed 's|^:"\([^"]*\)"."\([^"]*\)"."\([^"]*\)":.*$|"\1":"\2":"\3"|'`"
      tmpopts="`echo $tmpopts | sed 's|^\W*$||'`"
      tmpopts="`echo $tmpopts | sed 's|^:"[^"]*"."[^"]*"."[^"]*":||'`"
      print_sub_opt "$subopts"
   done

   echo ""

   tmpopts="$COMMAND_LINE_OPTS_STRINGS"
   while [ ! -z "$tmpopts" ]
   do
      subopts="`echo $tmpopts | sed 's|^:"\([^"]*\)"."\([^"]*\)"."\([^"]*\)":.*$|"\1":"\2":"\3"|'`"
      tmpopts="`echo $tmpopts | sed 's|^\W*$||'`"
      tmpopts="`echo $tmpopts | sed 's|^:"[^"]*"."[^"]*"."[^"]*":||'`"
      print_sub_str_opt "$subopts"
   done
}

print_sub_opt()
{
   arg1="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\1|'`"
   arg2="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\2|'`"
   arg3="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\3|'`"

   lowertext="`echo $arg1 | tr '[A-Z]' '[a-z]'`"

   if [ "$arg3" = "auto" ]; then
      echo "--enable-$lowertext: $arg2"
      echo "--disable-$lowertext"
   elif [ "$arg3" = "yes" ]; then
      echo "--disable-$lowertext: $arg2"
   elif [ "$arg3" = "no" ]; then
      echo "--enable-$lowertext: $arg2"
   fi
}

print_sub_str_opt()
{
   arg1="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\1|'`"
   arg2="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\2|'`"
   arg3="`echo $1 | sed 's|^"\([^"]*\)":"\([^"]*\)":"\([^"]*\)"$|\3|'`"

   lowertext="`echo $arg1 | tr '[A-Z]' '[a-z]'`"

   echo "--with-$lowertext: $arg2 (Defaults: $arg3)"
}

parse_input()
{
   ### Parse stuff :V

   while [ ! -z "$1" ]
   do
      
      case "$1" in

         --prefix=*)
            prefix="`echo $1 | sed -e 's|^--prefix=||' -e 's|^\(.*\)/$|\1|'`"

            if [ "$prefix" != "$1" ]; then
               PREFIX="$prefix"
            fi
            ;;

         --enable-*)
            tmp="$1"
            enable="${tmp#--enable-}"
            if [ -z "`echo $COMMAND_LINE_OPTS_ENABLE | grep -i -- $enable`" ]; then
               print_help
               exit 1
            fi
            eval HAVE_`echo $enable | tr '[a-z]' '[A-Z]'`=yes
            ;;

         --disable-*)
            tmp="$1"
            disable="${tmp#--disable-}"
            if [ -z "`echo $COMMAND_LINE_OPTS_ENABLE | grep -i -- $disable`" ]; then
               print_help
               exit 1
            fi
            eval HAVE_`echo $disable | tr '[a-z]' '[A-Z]'`=no
            ;;

         --with-*)
            tmp="$1"
            arg="${tmp#--with-*=}"
            tmp="${tmp#--with-}"
            with="${tmp%%=*}"
            if [ -z "`echo $COMMAND_LINE_OPTS_STRINGS | grep -i -- $with`" ]; then
               print_help
               exit 1
            fi
            eval "`echo $with | tr '[a-z]' '[A-Z]'`=\"$arg\""
            ;;


         -h|--help)
            print_help
            exit 0
            ;;
         *)
            print_help
            exit 1
            ;;

      esac

      shift

   done
}


