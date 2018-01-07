#!/bin/sh

#### options ####

slang_dir=slang-shaders
gshcompiler=$(pwd)/compiler.exe
vshflag=--vsh
pshflag=--psh
outputflag=--out
alignflag=--align

#################


currentdir=$(pwd)

die ()
{
   echo error while converting $name
   #mv -f tmp.vsh $currentdir
   #mv -f tmp.psh $currentdir
   #cp $1 $currentdir 2> /dev/null
   #exit 1
}


make

cd $slang_dir
slang_dir=$(pwd)
slang_files=`find $slang_dir -name "*.slang"`

for name in $slang_files ; do
echo $name
echo cd $(dirname $name)
cd $(dirname $name)
echo $currentdir/slang-convert --slang $name --vsh tmp.vsh --psh tmp.psh
$currentdir/slang-convert --slang $(basename $name) --vsh tmp.vsh --psh tmp.psh
echo $gshcompiler $alignflag $vshflag tmp.vsh $pshflag tmp.psh $outputflag `echo "$name" | sed "s/\.slang//"`.gsh
$gshcompiler $alignflag $vshflag tmp.vsh $pshflag tmp.psh $outputflag `echo "$name" | sed "s/\.slang//"`.gsh || die $name
rm -rf tmp.vsh tmp.psh
done
