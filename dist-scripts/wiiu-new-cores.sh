#!/bin/bash

. ../version.all
platform=wiiu
EXT=a
scriptDir=
pngDir=
infoDir=

original_pwd=$(pwd)

setScriptDir()
{
  scriptDir=$(dirname $(readlink -f $1))
}

setInfoDir()
{
  if [ -d ../../dist/info ]; then
    infoDir=$(readlink -f ../../dist/info)
  elif [ $(ls -1 *.info |wc -l) > 0 ]; then
    infoDir=$(pwd)
  fi

  if [ -z "$infoDir" ]; then
    echo "WARNING: Could not find your *.info files. meta.xml files will not be generated."
  fi
}

setPngDir()
{
  pwd
  if [ -d ../media/assets/pkg/wiiu ]; then
    pngDir=$(readlink -f ../media/assets/pkg/wiiu)
  elif [ $(ls -1 *.png |wc -l) > 0 ]; then
    pngDir=$(pwd)
  fi

  if [ -z "$pngDir" ]; then
    echo "WARNING: Could not find your *.png files. icon.png files will not be generated."
  fi
}

getCores()
{
  if [ -d ../../dist/wiiu ]; then
    ls -1 ../../dist/wiiu/*.a
  elif [ $(ls -1 *.a |wc -l) > 0 ]; then
    ls -1 *.a
  fi
}

clean()
{
  local here=$(pwd)

  cd $scriptDir/..
  make -f Makefile.wiiu clean || exit 1

  for trash in libretro_wiiu.a libretro_wiiu.elf libretro_wiiu.rpx \
               objs/wiiu pkg/wiiu/wiiu pkg/wiiu/retroarch pkg/wiiu/rpx
  do
    rm -rf $trash
  done

  cd $here
}

# $1 = core filename (e.g. ../../dist/wiiu/somecore_libretro_wiiu.a
# $2 = desired package type, e.g. rpx or elf
coreNameToPackageName()
{
  local packageName=$(basename $1 |awk -F'\.a' '{print $1}' |sed 's/_wiiu//')
  echo "$packageName"
}

lookup()
{
  cat | grep "$1 = " | sed "s/$1 = \"//" | sed s/\"//
}

generateMetaXml()
{
  local infoFile=$1
  local xmlDir=$2
  local outFile=$xmlDir/meta.xml

  if [ ! -e $infoFile ]; then
    return 1
  fi

  local display_name=$(cat $infoFile |lookup "display_name")
  local corename=$(cat $infoFile |lookup "corename")
  local authors=$(cat $infoFile |lookup "authors" |sed s/\|/\ -\ /g)
  local systemname=$(cat $infoFile |lookup "systemname")
  local license=$(cat $infoFile |lookup "license")
  local build_date=$(date +%Y%m%d%H%M%S)
  local build_hash=$(git rev-parse --short HEAD 2>/dev/null)

  mkdir -p $xmlDir
  echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' > $outFile
  echo '<app version="1">' >> $outFile
  echo '  <name>'$corename'</name>' >> $outFile
  echo '  <coder>'$authors'</coder>' >> $outFile
  echo '  <version>'$RARCH_VERSION' r'$build_hash'</version>' >> $outFile
  echo '  <release_date>'$build_date'</release_date>' >> $outFile
  echo '  <short_description>RetroArch</short_description>' >> $outFile
  echo -e '  <long_description>'$display_name'\n\nSystem: '$systemname'\nLicense: '$license'</long_description>' >> $outFile
  echo '  <category>emu</category>' >> $outFile
  echo '  <url>https://github.com/libretro</url>' >> $outFile
  echo '</app>' >> $outFile
}

copyPng()
{
  local pngFilename=$(echo $1 |sed 's/_libretro//').png
  local destFilename=$2/icon.png

  if [ -e $pngDir/$pngFilename ]; then
    cp $pngDir/$pngFilename $destFilename
  fi
}

buildCore()
{
  local core=$1
  local distDir=$(pwd)
  local buildDir=$(dirname $(pwd))
  local packageName=$(coreNameToPackageName $core)
  local rpxResult=$packageName.rpx
  local elfResult=$packageName.elf

  cd $buildDir

  if [ -f Makefile.wiiu ]; then
    echo "--- building core: $packageName ---"
    rm -f libretro_wiiu.a
    cp $distDir/$core libretro_wiiu.a
    make -f Makefile.wiiu \
      PC_DEVELOPMENT_TCP_PORT=$PC_DEVELOPMENT_TCP_PORT \
      -j3 || exit 1

    if [ ! -z "$infoDir" ]; then
      for i in 'pkg/wiiu/retroarch/cores' 'pkg/wiiu/rpx/retroarch/cores'; do
        mkdir -p $i/info
        cp $infoDir/$packageName.info $i/info
        generateMetaXml $i/info/$packageName.info $i/../../wiiu/apps/$packageName
      done
    fi

    if [ ! -z "$pngDir" ]; then
      for i in 'pkg/wiiu/wiiu/apps' 'pkg/wiiu/rpx/wiiu/apps'; do
        copyPng $packageName $i/$packageName
      done
    fi

    for i in "pkg/wiiu/wiiu/apps/$packageName" 'pkg/wiiu/retroarch/cores'; do
      mkdir -p $i
      cp retroarch_wiiu.elf $i/$elfResult
    done
    for i in "pkg/wiiu/rpx/wiiu/apps/$packageName" 'pkg/wiiu/rpx/retroarch/cores'; do
      mkdir -p $i
      cp retroarch_wiiu.rpx $i/$rpxResult
    done
  else
    echo "ERROR: Something went wrong. Makefile.wiiu not found."
    exit 1
  fi

  cd $distDir
}

setScriptDir $0

clean

cd $scriptDir
if [ -e ../wiiu-devel.properties ]; then
  . ../wiiu-devel.properties
fi

setInfoDir
setPngDir

cores=$(getCores)

if [ -z "$cores" ]; then
  echo "ERROR: No cores found. Nothing to do."
  exit 1
fi

for core in $cores; do
  buildCore $core
done
