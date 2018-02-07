# RetroArch Web Player

# Requirements
Most of the magic happens on the browser so nothing really on that regard

I you want a self hosted version you need
- A web server, nginx/apache will do, unzip a nightly here: 
  https://buildbot.libretro.com/nightly/emscripten/
- Move the template files from the embed folder to the top level dir,
  the other templates are for specific sites
- Grab the asset bundle:
  https://bot.libretro.com/assets/frontend/bundle.zip
- Unzip it somewhere in your web server in ./assets/frontend/bundle
- Create an assets/cores dir, you can put game data in that dir so it's available under **downloads**
- chmod +x the indexer script
- run the indexer script (you need coffeescript) like this: ./indexer ./assets/frontend > ./assets/frontend/.index-xhr
- run the indexer script (you need coffeescript) like this: ./indexer ./assets/cores > ./assets/cores/.index-xhr

That should be it, you can add more cores to the list by editing index.html

