#!/bin/sh
mkdir -p RetroArch.app/Contents/MacOS
cp -r pkg/apple/OSX/* RetroArch.app/Contents
cp retroarch RetroArch.app/Contents/MacOS
 
gsed -i -e 's/\${EXECUTABLE_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${PRODUCT_BUNDLE_IDENTIFIER}/com.libretro.RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${PRODUCT_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
gsed -i -e 's/\${MACOSX_DEPLOYMENT_TARGET}/10.13/' RetroArch.app/Contents/Info.plist
 
mkdir -p RetroArch.app/Contents/Resources/retroarch.iconset/
 
sips -z 16 16   pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_16x16.png
sips -z 32 32   pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_16x16@2x.png
sips -z 32 32   pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_32x32.png
sips -z 64 64   pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_32x32@2x.png
sips -z 128 128 pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_128x128.png
sips -z 256 256 pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_128x128@2x.png
sips -z 256 256 pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_256x256.png
sips -z 512 512 pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_256x256@2x.png
sips -z 512 512 pkg/apple/Resources/retroarch_logo.png --out RetroArch.app/Contents/Resources/retroarch.iconset/icon_512x512.png
 
iconutil -c icns -o RetroArch.app/Contents/Resources/retroarch.icns RetroArch.app/Contents/Resources/retroarch.iconset
