/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */
var dropbox = false;
var client = new Dropbox.Client({ key: "il6e10mfd7pgf8r" });

var showError = function(error) {
  switch (error.status) {
  case Dropbox.ApiError.INVALID_TOKEN:
  // If you're using dropbox.js, the only cause behind this error is that
  // the user token expired.
  // Get the user through the authentication flow again.
  break;

  case Dropbox.ApiError.NOT_FOUND:
  // The file or folder you tried to access is not in the user's Dropbox.
  // Handling this error is specific to your application.
  break;

  case Dropbox.ApiError.OVER_QUOTA:
  // The user is over their Dropbox quota.
  // Tell them their Dropbox is full. Refreshing the page won't help.
  break;

  case Dropbox.ApiError.RATE_LIMITED:
  // Too many API requests. Tell the user to try again later.
  // Long-term, optimize your code to use fewer API calls.
  break;

  case Dropbox.ApiError.NETWORK_ERROR:
  // An error occurred at the XMLHttpRequest layer.
  // Most likely, the user's network connection is down.
  // API calls will not succeed until the user gets back online.
  break;

  case Dropbox.ApiError.INVALID_PARAM:
  case Dropbox.ApiError.OAUTH_ERROR:
  case Dropbox.ApiError.INVALID_METHOD:
  default:
  // Caused by a bug in dropbox.js, in your application, or in Dropbox.
  // Tell the user an error occurred, ask them to refresh the page.
  }
};

function dropboxInit()
{
  document.getElementById('btnStart').disabled = true;
  document.getElementById('btnAuth').disabled = true;
  client.authDriver(new Dropbox.AuthDriver.Redirect());
  client.authenticate({ rememberUser: true }, function(error, client)
  {
     if (error) 
     {
        return showError(error);
     }
     dropboxSync(client, success);
  });
}
function success()
{
  document.getElementById('btnStart').disabled = false;
  console.log("WEBPLAYER: Sync successful");
}
function dropboxSync(dropboxClient, cb)
{
  var dbfs = new BrowserFS.FileSystem.Dropbox(dropboxClient);
  // Wrap in AsyncMirrorFS.
  var asyncMirror = new BrowserFS.FileSystem.AsyncMirror(
     new BrowserFS.FileSystem.InMemory(), dbfs);

  asyncMirror.initialize(function(err)
  {
      // Initialize it as the root file system.
     BrowserFS.initialize(asyncMirror);

     cb();
  });
}

var count = 0;
function setupFileSystem()
{
  console.log("WEBPLAYER: Initializing Filesystem");
  if(!client.isAuthenticated())
  {
     console.log("WEBPLAYER: Initializing LocalStorage");
     if(localStorage.getItem("fs_inited")!="true")
     {
        var lsfs = new BrowserFS.FileSystem.LocalStorage();

        BrowserFS.initialize(lsfs);
        var BFS = new BrowserFS.EmscriptenFS();
        FS.mount(BFS, {root: '/'}, '/home');
        console.log('WEBPLAYER: Filesystem initialized');
     }
     else
     {
        console.log('WEBPLAYER: Filesystem already initialized');
     }
  }
  else
  {
     console.log("WEBPLAYER: Initializing DropBoxFS");
     // Grab the BrowserFS Emscripten FS plugin.
     var BFS = new BrowserFS.EmscriptenFS();
     // Create the folder that we'll turn into a mount point.
     FS.createPath(FS.root, 'home', true, true);
     // Mount BFS's root folder into the '/data' folder.
     console.log('WEBPLAYER: Mounting');
     FS.mount(BFS, {root: '/'}, '/home');
     console.log('WEBPLAYER: DropBox initialized');
  }
}


function setupFolderStructure()
{
  FS.createPath('/', '/home/web_user', true, true);
  FS.createPath('/', '/home/web_user/.config', true, true);
  FS.createPath('/', '/home/web_user/.config/retroarch', true, true);
  FS.createPath('/', '/assets', true, true);
  FS.createPath('/', '/content', true, true);
}

function stat(path)
{
  try{
     FS.stat(path);
  }
  catch(err)
  {
     console.log("WEBPLAYER: file " + path + " doesn't exist");
     return false;
  }
  return true;
}

function startRetroArch()
{
  document.getElementById('canvas_div').style.display = 'block';
  document.getElementById('btnLoad').disabled = false;

  setupFileSystem();
  setupFolderStructure();

  Module['callMain'](Module['arguments']);
}

function selectFiles(files)
{
  count = files.length;

  for (var i = 0; i < files.length; i++) 
  {
     filereader = new FileReader();
     filereader.file_name = files[i].name;
     filereader.readAsArrayBuffer(files[i]);
     filereader.onload = function(){uploadData(this.result, this.file_name)};
  }
}

function uploadData(data,name)
{
  var dataView = new Uint8Array(data);
  FS.createDataFile('/', name, dataView, true, false);

  var data = FS.readFile(name,{ encoding: 'binary' });
  FS.writeFile('/content/' + name, data ,{ encoding: 'binary' });
  FS.unlink(name);

}

var Module = 
{
  noInitialRun: true,
  arguments: ["-v", "--menu"],
  preRun: [],
  postRun: [],
  print: (function() 
  {
     var element = document.getElementById('output');
     element.value = ''; // clear browser cache
     return function(text) 
     {
        text = Array.prototype.slice.call(arguments).join(' ');
        element.value += text + "\n";
        element.scrollTop = 99999; // focus on bottom
     };
  })(),

  printErr: function(text)
  {
     var text = Array.prototype.slice.call(arguments).join(' ');
     var element = document.getElementById('output');
     element.value += text + "\n";
     element.scrollTop = 99999; // focus on bottom
  },
  canvas: document.getElementById('canvas'),

  totalDependencies: 0,
  monitorRunDependencies: function(left) 
  {
     this.totalDependencies = Math.max(this.totalDependencies, left);
  }
};
