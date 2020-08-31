#!/bin/bash

# This script generates Gradle modules for each Android core,
# so that they can be served by Google Play as Dynamic Feature Modules.
# Run "./init_modules.sh" to generate modules, or "./init_modules.sh clean" to remove them

# These paths assume that this script is running inside libretro-super,
# and that the compiled Android cores are available while this script is run
RECIPES_PATH="../../../../recipes/android"
INFO_PATH="../../../../dist/info"
CORES_PATH="../../../../dist/android"

# Get the list of Android cores to generate modules for
CORES_LIST=$(cat module_list.txt)

# The below command would generate a module for every single Android core,
# but Dynamic Feature Modules enforces a 50-module limit
#CORES_LIST=$(find $RECIPES_PATH -type f ! -name '*.*' -exec cat {} + | awk '{ split($1, test, " "); print test[1] }' | grep "\S")

# Delete any leftover files from previous script runs
rm -rf modules
rm -f res/values/core_names.xml
rm -f res/values/module_names_*.xml
rm -f dynamic_features.gradle
rm -f settings.gradle

if [[ $1 = clean ]] ; then
  exit 1
fi

# Make directory for modules to be stored in
mkdir -p modules
mkdir -p res/values

# Begin generating files with necessary metadata
# for compiling Dynamic Feature Modules
echo "<resources>" >> res/values/core_names.xml
echo "android {" >> dynamic_features.gradle
echo "dynamicFeatures = [" >> dynamic_features.gradle

for arch in armeabi-v7a arm64-v8a x86 x86_64
do
  SANITIZED_ARCH_NAME=$(echo $arch | sed "s/-/_/g")
  echo "<resources>" >> res/values/module_names_$arch.xml
  echo "<string-array name=\"module_names_$SANITIZED_ARCH_NAME\">" >> res/values/module_names_$arch.xml
done

# Time to generate a module for each core!
while IFS= read -r core; do
  SANITIZED_CORE_NAME="core_$(echo $core | sed "s/-/_/g")"
  DISPLAY_NAME=$(cat $INFO_PATH/${core}_libretro.info | grep "display_name" | cut -d'"' -f 2)

  echo "Generating module for $core..."

  # Make a copy of the template
  cp -r module_template modules/$SANITIZED_CORE_NAME

  # Write the name of the core into AndroidManifest.xml
  if [[ "$OSTYPE" == "darwin"* ]]
  then
    sed -i '' "s/%CORE_NAME%/$SANITIZED_CORE_NAME/g" modules/$SANITIZED_CORE_NAME/AndroidManifest.xml
  else
    sed -i "s/%CORE_NAME%/$SANITIZED_CORE_NAME/g" modules/$SANITIZED_CORE_NAME/AndroidManifest.xml
  fi

  # Create a libs directory for each architecture,
  # and copy the libretro core into each directory
  for arch in armeabi-v7a arm64-v8a x86 x86_64
  do
    mkdir -p modules/$SANITIZED_CORE_NAME/libs/$arch

    if [[ -e $CORES_PATH/$arch/${core}_libretro_android.so ]]
    then
      ln -s  ../../../../$CORES_PATH/$arch/${core}_libretro_android.so modules/$SANITIZED_CORE_NAME/libs/$arch/lib$core.so
    else
      touch modules/$SANITIZED_CORE_NAME/libs/$arch/lib$core.so
    fi

    if [[ -s "modules/$SANITIZED_CORE_NAME/libs/$arch/lib$core.so" ]]
    then
      echo "<item>$core</item>" >> res/values/module_names_$arch.xml
    fi
  done

  # Write metadata about the module into the corresponding files
  echo "<string name=\"$SANITIZED_CORE_NAME\">$DISPLAY_NAME</string>" >> res/values/core_names.xml
  echo "':modules:$SANITIZED_CORE_NAME'," >> dynamic_features.gradle
  echo "include ':modules:$SANITIZED_CORE_NAME'" >> settings.gradle
done <<< "$CORES_LIST"

# Finish generating the metadata files
echo "</resources>" >> res/values/core_names.xml
echo "]" >> dynamic_features.gradle
echo "}" >> dynamic_features.gradle

for arch in armeabi-v7a arm64-v8a x86 x86_64
do
  echo "</string-array>" >> res/values/module_names_$arch.xml
  echo "</resources>" >> res/values/module_names_$arch.xml
done
