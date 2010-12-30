
PKG_CONF_PATH=""
PKG_CONF_USED=""
CONFIG_DEFINES=""
MAKEFILE_DEFINES=""
INCLUDE_DIRS=""
LIBRARY_DIRS=""
[ -z "$PREFIX" ] && PREFIX="/usr/local"

add_define_header()
{
   CONFIG_DEFINES="$CONFIG_DEFINES:@$1@$2@:"
}

add_define_make()
{
   MAKEFILE_DEFINES="$MAKEFILE_DEFINES:@$1@$2@:"
}

add_include_dirs()
{
   while [ ! -z "$1" ]
   do
      INCLUDE_DIRS="$INCLUDE_DIRS -I$1"
      shift
   done
}

add_library_dirs()
{
   while [ ! -z "$1" ]
   do
      LIBRARY_DIRS="$LIBRARY_DIRS -L$1"
      shift
   done
}

check_lib()
{
   tmpval="HAVE_$1"
   eval tmpval=\$$tmpval
   [ "$tmpval" = "no" ] && return 0

   echo -n "Checking function $3 in $2 ... "
   echo "void $3(void); int main(void) { $3(); return 0; }" > $TEMP_C


   eval HAVE_$1=no
   answer=no

   extralibs="$4"

   $CC -o $TEMP_EXE $TEMP_C $INCLUDE_DIRS $LIBRARY_DIRS $extralibs $2 2>/dev/null >/dev/null && answer=yes && eval HAVE_$1=yes

   echo $answer

   rm -rf $TEMP_C $TEMP_EXE
   if [ "$tmpval" = "yes" ] && [ "$answer" = "no" ]; then
      echo "Forced to build with library $2, but cannot locate. Exiting ..."
      exit 1
   fi
}

check_lib_cxx()
{
   tmpval="HAVE_$1"
   eval tmpval=\$$tmpval
   [ "$tmpval" = "no" ] && return 0

   echo -n "Checking function $3 in $2 ... "
   echo "extern \"C\" { void $3(void); } int main() { $3(); }" > $TEMP_CXX

   eval HAVE_$1=no
   answer=no

   extralibs="$4"

   $CXX -o $TEMP_EXE $TEMP_CXX $INCLUDE_DIRS $LIBRARY_DIRS $extralibs $2 2>/dev/null >/dev/null && answer=yes && eval HAVE_$1=yes

   echo $answer

   rm -rf $TEMP_CXX $TEMP_EXE
   if [ "$tmpval" = "yes" ] && [ "$answer" = "no" ]; then
      echo "Forced to build with library $2, but cannot locate. Exiting ..."
      exit 1
   fi
}

locate_pkg_conf()
{
   echo -n "Checking for pkg-config ... "
   PKG_CONF_PATH="`which pkg-config | grep ^/ | head -n1`"
   if [ -z $PKG_CONF_PATH ]; then
      echo "not found"
      echo "Cannot locate pkg-config. Exiting ..."
      exit 1
   fi
   echo "$PKG_CONF_PATH"
}

check_pkgconf()
{
   [ -z "$PKG_CONF_PATH" ] && locate_pkg_conf

   tmpval="HAVE_$1"
   eval tmpval=\$$tmpval
   [ "$tmpval" = "no" ] && return 0

   echo -n "Checking presence of package $2 ... "
   eval HAVE_$1=no
   eval $1_CFLAGS=""
   eval $1_LIBS=""
   answer=no
   minver=0.0
   [ ! -z $3 ] && minver=$3
   pkg-config --atleast-version=$minver --exists "$2" && eval HAVE_$1=yes && eval $1_CFLAGS='"`pkg-config $2 --cflags`"' && eval $1_LIBS='"`pkg-config $2 --libs`"' && answer=yes
   echo $answer

   PKG_CONF_USED="$PKG_CONF_USED $1"

   if [ "$tmpval" = "yes" ] && [ "$answer" = "no" ]; then
      echo "Forced to build with package $2, but cannot locate. Exiting ..."
      exit 1
   fi
}

check_header()
{
   tmpval="HAVE_$1"
   eval tmpval=\$$tmpval
   [ "$tmpval" = "no" ] && return 0

   echo -n "Checking presence of header file $2 ... "
   echo "#include<$2>" > $TEMP_C
   echo "int main(void) { return 0; }" >> $TEMP_C
   eval HAVE_$1=no
   answer=no

   $CC -o $TEMP_EXE $TEMP_C $INCLUDE_DIRS 2>/dev/null >/dev/null && answer=yes && eval HAVE_$1=yes

   echo $answer

   rm -rf $TEMP_C $TEMP_EXE
   if [ "$tmpval" = "yes" ] && [ "$answer" = "no" ]; then 
      echo "Build assumed that $2 exists, but cannot locate. Exiting ..."
      exit 1
   fi
}

check_switch_c()
{
   echo -n "Checking for availability of switch $2 in $CC ... "
   if [ -z "$CC" ]; then
      echo "No C compiler, cannot check ..."
      exit 1
   fi
   echo "int main(void) { return 0; }" > $TEMP_C
   eval HAVE_$1=no
   answer=no
   $CC -o $TEMP_EXE $TEMP_C $2 2>/dev/null >/dev/null && answer=yes && eval HAVE_$1=yes

   echo $answer

   rm -rf $TEMP_C $TEMP_EXE
}

check_switch_cxx()
{
   echo -n "Checking for availability of switch $2 in $CXX ... "
   if [ -z "$CXX" ]; then
      echo "No C++ compiler, cannot check ..."
      exit 1
   fi
   echo "int main() { return 0; }" > $TEMP_CXX
   eval HAVE_$1=no
   answer=no
   $CXX -o $TEMP_EXE $TEMP_CXX $2 2>/dev/null >/dev/null && answer=yes && eval HAVE_$1=yes

   echo $answer

   rm -rf $TEMP_CXX $TEMP_EXE
}

check_critical()
{
   val=HAVE_$1
   eval val=\$$val
   if [ "$val" != "yes" ]; then
      echo "$2"
      exit 1
   fi
}

output_define_header()
{
   arg1="`echo $2 | sed 's|^@\([^@]*\)@\([^@]*\)@$|\1|'`"
   arg2="`echo $2 | sed 's|^@\([^@]*\)@\([^@]*\)@$|\2|'`"

   echo "#define $arg1 $arg2" >> "$outfile"
}

create_config_header()
{
   outfile="$1"
   shift

   echo "Creating config header: $outfile"

   name="`echo __$outfile | sed 's|[\./]|_|g' | tr '[a-z]' '[A-Z]'`"
   echo "#ifndef $name" > "$outfile"
   echo "#define $name" >> "$outfile"
   echo "" >> "$outfile"
   echo "#define PACKAGE_NAME \"$PACKAGE_NAME\"" >> "$outfile"
   echo "#define PACKAGE_VERSION \"$PACKAGE_VERSION\"" >> "$outfile"

   while [ ! -z "$1" ]
   do
      tmpval="HAVE_$1"
      eval tmpval=\$$tmpval
      if [ "$tmpval" = "yes" ]; then 
         echo "#define HAVE_$1 1" >> "$outfile"
      elif [ "$tmpval" = "no" ]; then
         echo "/* #undef HAVE_$1 */" >> "$outfile"
      fi

      shift
   done

   echo "" >> "$outfile"

   tmpdefs="$CONFIG_DEFINES"
   while [ ! -z "$tmpdefs" ]
   do
      subdefs="`echo $tmpdefs | sed 's|^:\(@[^@]*@[^@]*@\):.*$|\1|'`"
      tmpdefs="`echo $tmpdefs | sed 's|^\W*$||'`"
      tmpdefs="`echo $tmpdefs | sed 's|^:\(@[^@]*@[^@]*@\):||'`"
      output_define_header "$outfile" "$subdefs"
   done

   echo "#endif" >> "$outfile"
}

output_define_make()
{
   arg1="`echo $2 | sed 's|^@\([^@]*\)@\([^@]*\)@$|\1|'`"
   arg2="`echo $2 | sed 's|^@\([^@]*\)@\([^@]*\)@$|\2|'`"

   echo "$arg1 = $arg2" >> "$outfile"
}

create_config_make()
{

   outfile="$1"
   shift

   echo "Creating make config: $outfile"

   rm -rf "$outfile"
   touch "$outfile"
   if [ "$USE_LANG_C" = "yes" ]; then
      echo "CC = $CC" >> "$outfile"
      echo "CFLAGS = $CFLAGS" >> "$outfile"
   fi
   if [ "$USE_LANG_CXX" = "yes" ]; then
      echo "CXX = $CXX" >> "$outfile"
      echo "CXXFLAGS = $CXXFLAGS" >> "$outfile"
   fi
   echo "LDFLAGS = $LDFLAGS" >> "$outfile"
   echo "INCLUDE_DIRS = $INCLUDE_DIRS" >> "$outfile"
   echo "LIBRARY_DIRS = $LIBRARY_DIRS" >> "$outfile"
   echo "PACKAGE_NAME = $PACKAGE_NAME" >> "$outfile"
   echo "PACKAGE_VERSION = $PACKAGE_VERSION" >> "$outfile"
   echo "PREFIX = $PREFIX" >> "$outfile"

   while [ ! -z "$1" ]
   do
      tmpval="HAVE_$1"
      eval tmpval=\$$tmpval
      if [ "$tmpval" = yes ]; then
         echo "HAVE_$1 = 1" >> "$outfile"
      elif [ "$tmpval" = no ]; then
         echo "HAVE_$1 = 0" >> "$outfile"
      fi

      if [ ! -z "`echo $PKG_CONF_USED | grep $1`" ]; then
         tmpval="$1_CFLAGS"
         eval tmpval=\$$tmpval
         echo "$1_CFLAGS = $tmpval" >> "$outfile"

         tmpval="$1_LIBS"
         eval tmpval=\$$tmpval
         echo "$1_LIBS = $tmpval" >> "$outfile"
      fi

     
      shift
   done

   echo "" >> "$outfile"

   tmpdefs="$MAKEFILE_DEFINES"
   while [ ! -z "$tmpdefs" ]
   do
      subdefs="`echo $tmpdefs | sed 's|^:\(@[^@]*@[^@]*@\):.*$|\1|'`"
      tmpdefs="`echo $tmpdefs | sed 's|^\W*$||'`"
      tmpdefs="`echo $tmpdefs | sed 's|^:\(@[^@]*@[^@]*@\):||'`"
      output_define_make "$outfile" "$subdefs"
   done

}


