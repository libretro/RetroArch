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
OVERLAY_DIR=${RETROARCH_DIR}/media/overlays
SHADERS_DIR=${RETROARCH_DIR}/media/shaders_glsl

if [ -d "${OVERLAY_DIR}" ]; then
   mv  ${OVERLAY_DIR}/.git ${OVERLAY_DIR}/../.git
   cp -r ${OVERLAY_DIR} ${BASE_DIR}/obj/${APP_BUNDLE_NAME}/
   mv  ${OVERLAY_DIR}/../.git ${OVERLAY_DIR}/.git
fi
if [ -d "${SHADERS_DIR}" ]; then
   cp -r ${SHADERS_DIR} ${BASE_DIR}/obj/${APP_BUNDLE_NAME}/
fi
