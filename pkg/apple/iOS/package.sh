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
RETROARCH_DIR=${BASE_DIR}/../../..
IOSDIR=${BASE_DIR}/iOS
APP_BUNDLE_DIR=${BASE_DIR}/obj/${APP_BUNDLE_NAME}
APP_BUNDLE_DIR_MEDIA=${APP_BUNDLE_DIR}/Media.xcassets
APP_BUNDLE_DIR_APPICONSET=${APP_BUNDLE_DIR_MEDIA}/AppIcon.appiconset
APP_BUNDLE_DIR_LAUNCHIMAGE=${APP_BUNDLE_DIR_MEDIA}/LaunchImage.launchimage
OVERLAY_DIR=${RETROARCH_DIR}/media/overlays
SHADERS_DIR=${RETROARCH_DIR}/media/shaders_glsl

if [ -d "${OVERLAY_DIR}" ]; then
   mv -v ${OVERLAY_DIR}/.git ${OVERLAY_DIR}/../.git
   cp -vr ${OVERLAY_DIR} ${APP_BUNDLE_DIR}/
   mv -v ${OVERLAY_DIR}/../.git ${OVERLAY_DIR}/.git
fi
if [ -d "${SHADERS_DIR}" ]; then
   cp -rv ${SHADERS_DIR} ${APP_BUNDLE_DIR}/
fi

cp -r ${IOSDIR}/en.lproj ${APP_BUNDLE_DIR}

plistutil -i ${IOSDIR}/Info.plist -o ${APP_BUNDLE_DIR}/Info.plist

if [ -d "${APP_BUNDLE_DIR_MEDIA}" ]; then
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-1.png ${APP_BUNDLE_DIR}/Icon-Small.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-2.png ${APP_BUNDLE_DIR}/Icon-Small@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-29-3.png ${APP_BUNDLE_DIR}/Icon-Small@3x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-1.png ${APP_BUNDLE_DIR}/Icon-Small-40.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-2.png ${APP_BUNDLE_DIR}/Icon-Small-40@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-40-3.png ${APP_BUNDLE_DIR}/Icon-Small-40@3x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-50-1.png ${APP_BUNDLE_DIR}/Icon-Small-50.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-50-2.png ${APP_BUNDLE_DIR}/Icon-Small-50@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-57-1.png ${APP_BUNDLE_DIR}/Icon.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-57-2.png ${APP_BUNDLE_DIR}/Icon@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-60-2.png ${APP_BUNDLE_DIR}/Icon-60@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-60-3.png ${APP_BUNDLE_DIR}/Icon-60@3x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-72-1.png ${APP_BUNDLE_DIR}/Icon-72.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-72-2.png ${APP_BUNDLE_DIR}/Icon-72@2x.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-76-1.png ${APP_BUNDLE_DIR}/Icon-76.png
   cp -v ${APP_BUNDLE_DIR_APPICONSET}/Icon-76-2.png ${APP_BUNDLE_DIR}/Icon-76@2x.png
   
   
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-320x480.png ${APP_BUNDLE_DIR}/Default.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x960.png ${APP_BUNDLE_DIR}/Default@2x.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-640x1136.png ${APP_BUNDLE_DIR}/Default-568h@2x.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1242x2208.png ${APP_BUNDLE_DIR}/Default-414w-736h@3x.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-750x1334.png ${APP_BUNDLE_DIR}/Default-375w-667h@2x.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/landscape-1024x748.png ${APP_BUNDLE_DIR}/Default-Landscape.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1536x2008.png ${APP_BUNDLE_DIR}/Default-Landscape@2x.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-768x1004.png ${APP_BUNDLE_DIR}/Default-Portrait.png
   cp -v ${APP_BUNDLE_DIR_LAUNCHIMAGE}/portrait-1536x2008.png ${APP_BUNDLE_DIR}/Default-Portrait@2x.png

   rm -rfv ${APP_BUNDLE_DIR_MEDIA}
fi
