#!/bin/bash

# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY}"
if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" = "" ] ; then
    # Fall back to old behavior.
    CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
fi

echo "Identity:"
echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"

if [ "$PLATFORM_FAMILY_NAME" = "tvOS" ] ; then
    BASE_DIR="tvOS"
    SUFFIX="_tvos"
elif [ "$PLATFORM_FAMILY_NAME" = "iOS" ] ; then
    BASE_DIR="iOS"
    SUFFIX="_ios"
elif [ "$PLATFORM_FAMILY_NAME" = "macOS" ] ; then
    BASE_DIR="OSX"
    SUFFIX=
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
    if [ -n "$SUFFIX" ] ; then
        intermediate="${intermediate/%$SUFFIX/}"
    fi
    fwName="${intermediate//_/.}"
    echo Making framework $fwName from $dylib

    fwDir="${OUTDIR}/${fwName}.framework"
    mkdir -p "$fwDir"
    if [ "$PLATFORM_FAMILY_NAME" = "iOS" ] ; then
        build_sdk=$(vtool -show-build "$dylib" | grep sdk | awk '{print $2}')
        vtool -set-build-version ios "${IPHONEOS_DEPLOYMENT_TARGET}" "${build_sdk}" -set-source-version 0.0 -replace -output "$dylib" "$dylib"
    fi
    lipo -create "$dylib" -output "$fwDir/$fwName"
    sed -e "s,%CORE%,$fwName," -e "s,%BUNDLE%,$fwName," -e "s,%IDENTIFIER%,$fwName," iOS/fw.tmpl > "$fwDir/Info.plist"
    echo "signing $fwName"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "$fwDir"
done

# Copy in MoltenVK as an embedded library manually instead of having
# Xcode do it. This makes it potentially easier to substitute out
# MoltenVK, have it provided outside the repo, or have different
# MoltenVK builds for different OS versions.

if [ -z "${MOLTENVK_XCFRAMEWORK}" ] ; then
    MOLTENVK_XCFRAMEWORK="${SRCROOT}/Frameworks/MoltenVK.xcframework"
fi
MVK_PLATFORM_SUBDIR="${SWIFT_PLATFORM_TARGET_PREFIX}-$(echo $ARCHS_STANDARD_64_BIT | sed -e 's/ /_/g')${LLVM_TARGET_TRIPLE_SUFFIX}"
if [ -d "${MOLTENVK_XCFRAMEWORK}/${MVK_PLATFORM_SUBDIR}/MoltenVK.framework" ] ; then
    echo copying moltenvk from "${MOLTENVK_XCFRAMEWORK}/${MVK_PLATFORM_SUBDIR}/MoltenVK.framework"
    cp -r "${MOLTENVK_XCFRAMEWORK}/${MVK_PLATFORM_SUBDIR}/MoltenVK.framework" "${OUTDIR}"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "${OUTDIR}/MoltenVK.framework"
fi

# iOS 12 needs an older version of MoltenVK
if [ -n "$MOLTENVK_LEGACY_XCFRAMEWORK_PATH" -a -d "${MOLTENVK_LEGACY_XCFRAMEWORK_PATH}/${MVK_PLATFORM_SUBDIR}/MoltenVK-${MOLTENVK_LEGACY_VERSION}.framework" ] ; then
    echo copying legacy moltenvk from "${MOLTENVK_LEGACY_XCFRAMEWORK_PATH}/${MVK_PLATFORM_SUBDIR}/MoltenVK-${MOLTENVK_LEGACY_VERSION}.framework"
    cp -r "${MOLTENVK_LEGACY_XCFRAMEWORK_PATH}/${MVK_PLATFORM_SUBDIR}/MoltenVK-${MOLTENVK_LEGACY_VERSION}.framework" "${OUTDIR}"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "${OUTDIR}/MoltenVK-${MOLTENVK_LEGACY_VERSION}.framework/MoltenVK-${MOLTENVK_LEGACY_VERSION}"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "${OUTDIR}/MoltenVK-${MOLTENVK_LEGACY_VERSION}.framework"
fi
