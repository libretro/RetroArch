. qb/conf.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_EXE=.tmp

echo -n "Checking operating system ... "
OS="Win32" # whatever ;D
unamestr="`uname -o`"
if [ ! -z "`echo $unamestr | grep -i Linux`" ]; then
   OS="Linux"
elif [ ! -z "`echo $unamestr | grep -i Darwin`" ]; then
   OS="Darwin"
elif [ ! -z "`echo $unamestr | grep -i BSD`" ]; then
   OS="BSD"
elif [ ! -z "`echo $unamestr | grep -i NT`" ]; then
   OS="Cygwin"
fi

echo $OS


# Checking for working C compiler
if [ "$USE_LANG_C" = yes ]; then
   echo "Checking for working C compiler ..."
   if [ -z $CC ]; then
      CC=`which gcc cc 2> /dev/null | grep ^/ | head -n 1`
   fi
   if [ -z $CC ]; then
      echo "Could not find C compiler in path. Exiting ..."
      exit 1
   fi

   echo -n "Checking if $CC is a suitable compiler ... "
   answer=no
   echo "#include <stdio.h>" > $TEMP_C
   echo "int main(void) { puts(\"Hai world!\"); return 0; }" >> $TEMP_C
   $CC -o $TEMP_EXE $TEMP_C 2>/dev/null >/dev/null && answer=yes
   echo $answer

   rm -rf $TEMP_C $TEMP_EXE

   [ $answer = no ] && echo "Can't find suitable C compiler. Exiting ..." && exit 1
fi

# Checking for working C++ compiler
if [ "$USE_LANG_CXX" = "yes" ]; then
   echo "Checking for working C++ compiler ..."
   if [ -z $CXX ]; then
      CXX=`which g++ c++ 2> /dev/null | grep ^/ | head -n 1`
   fi
   if [ -z $CXX ]; then
      echo "Could not find C compiler in path. Exiting ..."
      exit 1
   fi

   echo -n "Checking if $CXX is a suitable compiler ... "
   answer=no
   echo "#include <iostream>" > $TEMP_CXX
   echo "int main() { std::cout << \"Hai guise\" << std::endl; return 0; }" >> $TEMP_CXX
   $CXX -o $TEMP_EXE $TEMP_CXX 2>/dev/null >/dev/null && answer=yes
   echo $answer

   rm -rf $TEMP_CXX $TEMP_EXE

   [ $answer = no ] && echo "Can't find suitable C++ compiler. Exiting ..." && exit 1
fi

