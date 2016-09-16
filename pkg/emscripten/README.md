# RetroArch Web Player

# Requirements
Most of the magic happens on the browser so nothing really on that regard

I you want a self hosted version you need
- A web server, nginx/apache will do, unzip a nightly here: 
  https://buildbot.libretro.com/nightly/emscripten/
- Move the template files from the embed folder to the top level dir,
  the other templates are for specific sites
- A SSL certificate if you want to integrate with dropbox
- A dropbox application, you can't use ours on a self hosted version

If you want assets for XMB, shaders, overlays you need to do the following:
- Grab the asset bundle:
  https://bot.libretro.com/assets/frontend/bundle.zip
- Unzip it somewhere in your web server
- Generate an index of the folder with this script, save it inside the folder you want to serve 
  https://github.com/jvilk/BrowserFS/blob/master/tools/XHRIndexer.coffee
- Add the mount in the setupFileSystem function, look for the xfs blocks and 
  unconmment them and tweak them accordingly

If you want to add your game directory so it serves content to your users you 
need to do the following:
- Add the dir to your web server
- Generate an index with this script, save it inside the folder you want to serve 
  https://github.com/jvilk/BrowserFS/blob/master/tools/XHRIndexer.coffee
- Add a new mount in the setupFileSystem function, look for the xfs blocks and 
  unconmment them and tweak them accordingly

```javascript
   /* create an XmlHttpRequest filesystem for the bundled data */
   /* --> uncomment this if you want builtin assets for XMB, overlays, etc.*/
   var xfs1 =  new BrowserFS.FileSystem.XmlHttpRequest
   ("--your-assets-index-file-name--", "--your-index-url--");
   
   /* create an XmlHttpRequest filesystem for content */
   /* --> uncomment this if you want to serve content.*/
   var xfs2 =  new BrowserFS.FileSystem.XmlHttpRequest
   ("--your-content-index-file-name--", "--your-index-url--");


   // lots and lots of code
   // and then
   
   /*
   mfs.mount('/home/web_user/retroarch/bundle', xfs1);
   mfs.mount('/home/web_user/retroarch/userdata/content/', xfs2);
   */
}
```