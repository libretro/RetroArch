#!/bin/sh
rm -rf RetroArch.app
mkdir -p RetroArch.app/Contents/MacOS
cp -r pkg/apple/OSX/* RetroArch.app/Contents
cp retroarch RetroArch.app/Contents/MacOS
 
gsed -i -e 's/\${EXECUTABLE_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${PRODUCT_BUNDLE_IDENTIFIER}/com.libretro.RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${PRODUCT_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${MACOSX_DEPLOYMENT_TARGET}/10.13/' RetroArch.app/Contents/Info.plist
 
cp media/retroarch.icns RetroArch.app/Contents/Resources
