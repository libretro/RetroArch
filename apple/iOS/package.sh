#!/bin/sh

# BSDs don't have readlink -f
read_link()
{
   TARGET_FILE="$1"
   cd $(dirname "$TARGET_FILE")
   TARGET_FILE=$(basename "$TARGET_FILE")

   while [ -L "$TARGET_FILE" ]
   do
      TARGET_FILE=$(readlink "$TARGET_FILE")
      cd $(dirname "$TARGET_FILE")
      TARGET_FILE=$(basename "$TARGET_FILE")
   done

   PHYS_DIR=$(pwd -P)
   RESULT="$PHYS_DIR/$TARGET_FILE"
   echo $RESULT
}

SCRIPT=$(read_link "$0")
echo "Script: $SCRIPT"
APP_BUNDLE_NAME=RetroArch.app
BASE_DIR=$(dirname "$SCRIPT")
RETROARCH_DIR=${BASE_DIR}/../..
IOSDIR=${BASE_DIR}/iOS
APP_BUNDLE_DIR=${BASE_DIR}/obj/${APP_BUNDLE_NAME}
APP_BUNDLE_DIR_MEDIA=${APP_BUNDLE_DIR}/Media.xcassets
APP_BUNDLE_DIR_APPICONSET=${APP_BUNDLE_DIR_MEDIA}/AppIcon.appiconset
APP_BUNDLE_DIR_LAUNCHIMAGE=${APP_BUNDLE_DIR_MEDIA}/LaunchImage.launchimage
OVERLAY_DIR=${RETROARCH_DIR}/media/overlays
SHADERS_DIR=${RETROARCH_DIR}/media/shaders_glsl

if [ -d "${OVERLAY_DIR}" ]; then
   mv  ${OVERLAY_DIR}/.git ${OVERLAY_DIR}/../.git
   cp -r ${OVERLAY_DIR} ${APP_BUNDLE_DIR}/
   mv  ${OVERLAY_DIR}/../.git ${OVERLAY_DIR}/.git
fi
if [ -d "${SHADERS_DIR}" ]; then
   cp -r ${SHADERS_DIR} ${APP_BUNDLE_DIR}/
fi

if [ -f "${APP_BUNDLE_DIR}/PauseIndicatorView.xib" ]; then
   rm ${APP_BUNDLE_DIR}/PauseIndicatorView.xib
fi

cp -r ${IOSDIR}/en.lproj ${APP_BUNDLE_DIR}

if [ -d "${APP_BUNDLE_DIR_MEDIA}" ]; then
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-1.png ${APP_BUNDLE_DIR}/AppIcon29x29.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-2.png ${APP_BUNDLE_DIR}/AppIcon29x29@2x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-2.png ${APP_BUNDLE_DIR}/AppIcon29x29@2x~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-3.png ${APP_BUNDLE_DIR}/AppIcon29x29@3x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-1.png ${APP_BUNDLE_DIR}/AppIcon29x29~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-2.png ${APP_BUNDLE_DIR}/AppIcon40x40@2x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-2.png ${APP_BUNDLE_DIR}/AppIcon40x40@2x~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-3.png ${APP_BUNDLE_DIR}/AppIcon40x40@3x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-1.png ${APP_BUNDLE_DIR}/AppIcon40x40~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-50-2.png ${APP_BUNDLE_DIR}/AppIcon50x50@2x~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-50-1.png ${APP_BUNDLE_DIR}/AppIcon50x50~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-57-1.png ${APP_BUNDLE_DIR}/AppIcon57x57.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-57-2.png ${APP_BUNDLE_DIR}/AppIcon57x57@2x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-60-2.png ${APP_BUNDLE_DIR}/AppIcon60x60@2x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-60-3.png ${APP_BUNDLE_DIR}/AppIcon60x60@3x.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-72-2.png ${APP_BUNDLE_DIR}/AppIcon72x72@2x~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-72-1.png ${APP_BUNDLE_DIR}/AppIcon72x72~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-76-2.png ${APP_BUNDLE_DIR}/AppIcon76x76@2x~ipad.png
   cp ${APP_BUNDLE_DIR_APPICONSET}/Icon-76-1.png ${APP_BUNDLE_DIR}/AppIcon76x76~ipad.png

   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x1136.png ${APP_BUNDLE_DIR}/Default-568h@2x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x1136.png ${APP_BUNDLE_DIR}/LaunchImage-568h@2x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x1136.png ${APP_BUNDLE_DIR}/LaunchImage-700-568h@2x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-2048x1536.png ${APP_BUNDLE_DIR}/LaunchImage-700-Landscape@2x~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-1024x768.png ${APP_BUNDLE_DIR}/LaunchImage-700-Landscape~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1536x2048.png ${APP_BUNDLE_DIR}/LaunchImage-700-Portrait@2x~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-768x1024.png ${APP_BUNDLE_DIR}/LaunchImage-700-Portrait~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x960.png ${APP_BUNDLE_DIR}/LaunchImage-700@2x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-750x1334.png ${APP_BUNDLE_DIR}/LaunchImage-800-667h@2x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-2208x1242.png ${APP_BUNDLE_DIR}/LaunchImage-800-Landscape-736h@3x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1242x2208.png ${APP_BUNDLE_DIR}/LaunchImage-800-Portrait-736h@3x.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-2048x1496.png ${APP_BUNDLE_DIR}/LaunchImage-Landscape@2x~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-1024x748.png ${APP_BUNDLE_DIR}/LaunchImage-Landscape~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1536x2008.png ${APP_BUNDLE_DIR}/LaunchImage-Portrait@2x~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-768x1004.png ${APP_BUNDLE_DIR}/LaunchImage-Portrait~ipad.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-320x480.png ${APP_BUNDLE_DIR}/LaunchImage.png
   cp ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x960.png ${APP_BUNDLE_DIR}/LaunchImage@2x.png

   rm -rf ${APP_BUNDLE_DIR_MEDIA}
fi
