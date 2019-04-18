#!/bin/bash

#
# This script will upload the packaged RetroArch cores to a WiiU running
# FTPiiU or FTPiiU Anywhere
#
# IMPORTANT: This script assumes the following structur
#
# WARNING: I experienced corrupt uploads when using Dimok's FTPiiU. You
# probably want to use FIX94's FTPiiU Anywhere.
#
# After uploading everything, the script will re-download the RPX files and
# compare their hash and print an error if the file was corrupted.
#
# The WiiU's IP address can be specified by either setting the WIIU_IP_ADDRESS
# environment variable, or by configuring the wiiu-devel.properties file
# (see the file wiiu-devel.properties.template for instructions).
#

# The path to the parent directory of your retroarch/ and wiiu/ folders, as
# visible in FTPiiU.

RETRO_ROOT=sd

here=$(pwd)
cd $(dirname $(readlink -f $0))
if [ -e ../wiiu-devel.properties ]; then
  . ../wiiu-devel.properties
fi

if [ -z "$WIIU_IP_ADDRESS" ]; then
  echo "WIIU_IP_ADDRESS not set. Set up ../wiiu-devel.properties or set the"
  echo "environment variable."
  cd $here
  exit 1
fi

filesToUpload()
{
  find . -type f \( -name "*.rpx" -o -name "*.xml" -o -name "*.png" -o -name "*.info" \)
}

cd ../pkg/wiiu/rpx

# First, delete any previous *.remote files from previous uploads.
find . -name '*.remote' | xargs rm -f {}

# Now generate the FTP command list
rm -f .ftpcommands

# Now create the directory structure
for dir in $(find . -type "d"); do
  if [ "$dir" == "." ]; then
    continue
  fi
  echo "mkdir $dir" >> .ftpcommands
done

# Delete and re-upload the files we just built
for cmd in rm put; do
  filesToUpload | xargs -L 1 echo "$cmd" >> .ftpcommands
done

# Lastly, download the RPX files as *.rpx.remote files
for rpx in $(find . -name "*.rpx"); do
  echo "get $rpx ${rpx}.remote" >> .ftpcommands
done

# The command list is done. Time to execute it.
ftp -n $WIIU_IP_ADDRESS <<END_SCRIPT
quote USER wiiu
quote PASS wiiu
passive
bin
cd $RETRO_ROOT

$(cat .ftpcommands)
END_SCRIPT

rm -f .ftpcommands

errors=0
# Now, we compare the hashes of the original file and the file we got back,
# and print an error if the hashes don't match.
for remote in $(find . -name "*.remote"); do
  originalFile=$(echo $remote |sed 's/\.remote//')
  originalHash=$(md5sum -b $originalFile |awk '{print $1}')
  remoteHash=$(md5sum -b $remote |awk '{print $1}')

  if [ "$originalHash" != "$remoteHash" ]; then
    echo "ERROR: $remote was corrupted during upload."
    errors=$((errors+1))
  fi
done

cd $here

if [ $errors -ne 0 ]; then
  echo "Upload failed. $errors files failed to upload correctly."
  exit 1
fi

echo "RetroArch build uploaded and validated successfully."
exit 0
