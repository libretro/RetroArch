#!/bin/bash

# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY}"
if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" = "" ] ; then
    # Fall back to old behavior.
    CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
fi

echo "Identity:"
echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"

if [ "$1" = "tvos" ] ; then
    BASE_DIR="tvOS"
    SUFFIX="_tvos"
else
    BASE_DIR="iOS"
    SUFFIX="_ios"
fi

if [ -n "$BUILT_PRODUCTS_DIR" -a -n "$FRAMEWORKS_FOLDER_PATH" ] ; then
    OUTDIR="$BUILT_PRODUCTS_DIR"/"$FRAMEWORKS_FOLDER_PATH"
else
    OUTDIR="$BASE_DIR"/Frameworks
fi

mkdir -p "$OUTDIR"

for dylib in $(find "$BASE_DIR"/modules -maxdepth 1 -type f -regex '.*libretro.*\.dylib$') ; do
    intermediate=$(basename "$dylib")
    intermediate="${intermediate/%.dylib/}"
    intermediate="${intermediate/%$SUFFIX/}"
    fwName="${intermediate//_/.}"
    echo Making framework $fwName from $dylib

    fwDir="${OUTDIR}/${fwName}.framework"
    mkdir -p "$fwDir"
    lipo -create "$dylib" -output "$fwDir/$fwName"
    sed -e "s,%CORE%,$fwName," -e "s,%BUNDLE%,$fwName," -e "s,%IDENTIFIER%,$fwName," iOS/fw.tmpl > "$fwDir/Info.plist"
    echo "signing $fwName"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "$fwDir"
done
