#!/bin/bash

# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY_NAME}"
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

mkdir -p "$BASE_DIR"/Frameworks

for dylib in $(find "$BASE_DIR"/modules -maxdepth 1 -type f -regex '.*libretro.*\.dylib$') ; do
    intermediate=$(basename "$dylib")
    intermediate="${intermediate/%.dylib/}"
    identifier="${intermediate/%$SUFFIX/}"
    intermediate="${identifier/%_libretro/}"
    fwName="${intermediate}_libretro"
    echo Making framework $fwName from $dylib

    fwDir="$BASE_DIR/Frameworks/${fwName}.framework"
    mkdir -p "$fwDir"
    lipo -create "$dylib" -output "$fwDir/$fwName"
    echo "signing $fwName"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "$fwDir"
    sed -e "s,%CORE%,$fwName," -e "s,%IDENTIFIER%,$identifier," iOS/fw.tmpl > "$fwDir/Info.plist"
done