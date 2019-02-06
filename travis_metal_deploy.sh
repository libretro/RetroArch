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

echo "Uploading to server..."

rsync -avhP -e 'ssh -p 12346 -o StrictHostKeyChecking=no' ~/${FILENAME} travis@bot.libretro.com:~/nightly/apple/osx/x86_64/
