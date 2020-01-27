#!/bin/bash

mkdir -p ~/.ssh

chmod 700 ~/.ssh

echo "Decrypting SSH key..."

openssl aes-256-cbc -K $encrypted_e9bb4da59666_key -iv $encrypted_e9bb4da59666_iv -in travis-deploy-key.enc -out ~/.ssh/id_rsa -d

chmod 600 ~/.ssh/id_rsa

mkdir ~/dist
cd ~/dist

echo "Copying binary into dist folder..."

cp -rv  ${TRAVIS_BUILD_DIR}/pkg/apple/build/Release/RetroArch.app .

echo "Downloading assets..."

cd RetroArch.app/Contents/Resources/
curl -O http://bot.libretro.com/assets/frontend/bundle.zip
unzip -q -o bundle.zip
rm -rf bundle.zip

echo "Creating DMG image..."

cd ~/dist

FILENAME=$(date +%F)_RetroArch_Metal.dmg

hdiutil create -volname RetroArch -srcfolder ./ -ov -format UDZO ~/${FILENAME}
cp -f ~/${FILENAME} ~/RetroArch_Metal.dmg

echo "Notarizing DMG..."

codesign --force --verbose --timestamp --sign "7069CC8A4AE9AFF0493CC539BBA4FA345F0A668B" ~/RetroArch_Metal.dmg
REQUESTUUID=$(xcrun altool --notarize-app -t osx -f ~/RetroArch_Metal.dmg --primary-bundle-id libretro.RetroArch -u $APPLE_ID -p $APPLE_ID_PASS -itc_provider ZE9XE938Z2 | awk '/RequestUUID/ { print $NF; }')
sleep 200
xcrun altool --notarization-info $REQUESTUUID -u $APPLE_ID -p $APPLE_ID_PASS -ascprovider ZE9XE938Z2
xcrun stapler staple ~/RetroArch_Metal.dmg
xcrun stapler validate ~/RetroArch_Metal.dmg

echo "Uploading to server..."

rsync -avhP -e 'ssh -p 12346 -o StrictHostKeyChecking=no' ~/${FILENAME} travis@bot.libretro.com:~/nightly/apple/osx/x86_64/
rsync -avhP -e 'ssh -p 12346 -o StrictHostKeyChecking=no' ~/RetroArch_Metal.dmg travis@bot.libretro.com:~/nightly/apple/osx/x86_64/

rm -f ~/RetroArch_Metal.dmg
