1. download the build from https://buildbot.libretro.com/nightly/emscripten/
2. unzip that somewhere in your web server tree
3. download the asset bundle from https://buildbot.libretro.com/assets/frontend/bundle.zip
4. extract the bundle in  /assets/frontend/bundle
5. create an assets/cores dir, you can put game data in that dir
6. chmod +x indexer
7. run the indexer script (you need coffeescript) like this: ./indexer ./assets/frontend > ./assets/frontend/.index-xhr
8. run the indexer script (you need coffeescript) like this: ./indexer ./assets/cores > ./assets/cores/.index-xhr

That should be it, you can add more cores to the list by editing index.html

