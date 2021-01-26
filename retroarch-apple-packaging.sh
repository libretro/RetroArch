#!/bin/sh
# app stuff

rm -rf RetroArch.app

mkdir -p RetroArch.app/Contents/MacOS
cp -r pkg/apple/OSX/* RetroArch.app/Contents
cp retroarch RetroArch.app/Contents/MacOS

mv RetroArch.app/Contents/Info_Metal.plist RetroArch.app/Contents/Info.plist

sed -i'.bak' 's/\${EXECUTABLE_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
sed -i'.bak' 's/\$(PRODUCT_BUNDLE_IDENTIFIER)/com.libretro.RetroArch/' RetroArch.app/Contents/Info.plist
sed -i'.bak' 's/\${PRODUCT_NAME}/RetroArch/' RetroArch.app/Contents/Info.plist
sed -i'.bak' 's/\${MACOSX_DEPLOYMENT_TARGET}/10.13/' RetroArch.app/Contents/Info.plist

cp media/retroarch.icns RetroArch.app/Contents/Resources/

# dmg stuff

umount wc
rm -rf RetroArch.dmg wc empty.dmg

mkdir -p template
hdiutil create -fs HFSX -layout SPUD -size 200m empty.dmg -srcfolder template -format UDRW -volname RetroArch -quiet
rmdir template

mkdir -p wc
hdiutil attach empty.dmg -noautoopen -quiet -mountpoint wc
rm -rf wc/RetroArch.app
ditto -rsrc RetroArch.app wc/RetroArch.app
ln -s /Applications wc/Applications
WC_DEV=`hdiutil info | grep wc | grep "Apple_HFS" | awk '{print $1}'` && hdiutil detach $WC_DEV -quiet -force
hdiutil convert empty.dmg -quiet -format UDZO -imagekey zlib-level=9 -o RetroArch.dmg

umount wc
rm -rf wc empty.dmg
