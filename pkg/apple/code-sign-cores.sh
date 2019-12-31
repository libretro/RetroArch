#!/bin/sh

# WARNING: You may have to run Clean in Xcode after changing CODE_SIGN_IDENTITY! 

# Verify that $CODE_SIGN_IDENTITY is set
if [ -z "${CODE_SIGN_IDENTITY}" ] ; then
    echo "CODE_SIGN_IDENTITY needs to be set for code-signing!"

    if [ "${CONFIGURATION}" = "Release" ] ; then
        exit 1
    else
        # Code-signing is optional for non-release builds.
        exit 0
    fi
fi

ITEMS=""

if [ "$1" = "tvos" ]; then
    CORES_DIR="${PROJECT_DIR}/tvOS/modules"
else
    CORES_DIR="${PROJECT_DIR}/iOS/modules"
fi

echo "Cores dir: ${CORES_DIR}"
if [ -d "$CORES_DIR" ] ; then
    CORES=$(find "${CORES_DIR}" -depth -type d -name "*.framework" -or -name "*.dylib" -or -name "*.bundle" | sed -e "s/\(.*framework\)/\1\/Versions\/A\//")
    RESULT=$?
    if [ "$RESULT" != 0 ] ; then
        exit 1
    fi

    ITEMS="${CORES}"
fi

# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY_NAME}"
if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" = "" ] ; then
    # Fall back to old behavior.
    CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
fi

echo "Identity:"
echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"

echo "Found:"
echo "${ITEMS}"

# Change the Internal Field Separator (IFS) so that spaces in paths will not cause problems below.
SAVED_IFS=$IFS
# Doing IFS=$(echo -en "\n") does not work on Xcode 10 for some reason
IFS="
"

# Loop through all items.
for ITEM in $ITEMS;
do
    echo "Signing '${ITEM}'"
    codesign --force --verbose --sign "${CODE_SIGN_IDENTITY_FOR_ITEMS}" "${ITEM}"
    RESULT=$?
    if [ "$RESULT" != 0 ] ; then
        echo "Failed to sign '${ITEM}'."
        IFS=$SAVED_IFS
        exit 1
    fi
done

# Restore $IFS.
IFS=$SAVED_IFS