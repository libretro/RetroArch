#!/bin/sh

cat AndroidManifest.xml.base | sed -e 's/\[\[\[PAK\]\]\]/'$1'/' > AndroidManifest.xml
ndk-build
ant debug install
