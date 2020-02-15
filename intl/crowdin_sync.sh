#!/bin/bash

# Update source code
git pull

# Convert source *.h to *.json
./h2json.py msg_hash_us.h

# Upload source file
crowdin upload sources

# Crowdin need some time to process data
sleep 1m

# Download translation files
crowdin download translations

# Convert translation *.json to *.h
for f in *.json
do
	./json2h.py $f
done

# Commit new translations
git add .
git commit -m "Synchronize translations"
git push
