/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */
var dropbox = false;
var client = new Dropbox.Client({ key: "il6e10mfd7pgf8r" });
var XFS;

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
  document.getElementById('btnRun').disabled = true;
  document.getElementById('btnDrop').disabled = true;
  $('#icnDrop').removeClass('fa-dropbox');
  $('#icnDrop').addClass('fa-spinner spinning');
  

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
  document.getElementById('btnRun').disabled = false;
  $('#icnDrop').removeClass('fa-spinner spinning');
  $('#icnDrop').addClass('fa-check');
  console.log("WEBPLAYER: Sync successful");
  setupFileSystem("dropbox");
  setupFolderStructure();
}

var afs;

function dropboxSync(dropboxClient, cb)
{
  var dbfs = new BrowserFS.FileSystem.Dropbox(dropboxClient);
  // Wrap in afsFS.
  afs = new BrowserFS.FileSystem.AsyncMirror(
     new BrowserFS.FileSystem.InMemory(), dbfs);

  afs.initialize(function(err)
  {
      // Initialize it as the root file system.
      //BrowserFS.initialize(afs);
      cb();
  });
}

function setupFileSystem(backend)
{
   /* create a mountable filesystem that will server as a root 
      mountpoint for browserfs */
   var mfs =  new BrowserFS.FileSystem.MountableFileSystem();

   /* create an XmlHttpRequest filesystem for the bundled data */
   var xfs1 =  new BrowserFS.FileSystem.XmlHttpRequest
      (".index-xhr", "/web/assets/");
   /* create an XmlHttpRequest filesystem for core assets */
   var xfs2 =  new BrowserFS.FileSystem.XmlHttpRequest
      (".index-xhr", "/assets/cores/");

   console.log("WEBPLAYER: Initializing Filesystem");
   if(backend == "browser")
   {
      console.log("WEBPLAYER: Initializing LocalStorage");

      /* create a local filesystem */
      var lsfs = new BrowserFS.FileSystem.LocalStorage();

      /* mount the filesystems onto mfs */
      mfs.mount('/home/web_user/retroarch/userdata', lsfs);
   }
   else
   {
      /* mount the filesystems onto mfs */
      mfs.mount('/home/web_user/retroarch/userdata', afs);
   }

   mfs.mount('/home/web_user/retroarch/bundle', xfs1);
   mfs.mount('/home/web_user/retroarch/downloads', xfs2);
   BrowserFS.initialize(mfs);
   var BFS = new BrowserFS.EmscriptenFS();
   FS.mount(BFS, {root: '/home'}, '/home');
   console.log("WEBPLAYER: " + backened + " filesystem initialized");
}

/**
 * Retrieve the value of the given GET parameter.
 */
function getParam(name) {
  var results = new RegExp('[\?&]' + name + '=([^&#]*)').exec(window.location.href);
  if (results) {
    return results[1] || null;
  }
}

function setupFolderStructure()
{
  FS.createPath('/', '/home/web_user', true, true);
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
   document.getElementById('btnDrop').disabled = true;
   document.getElementById('btnRun').disabled = true;
  
   $('#btnFullscreen').removeClass('disabled');
   $('#btnAdd').removeClass('disabled');
   $('#btnRom').removeClass('disabled');

   Module['callMain'](Module['arguments']);
   document.getElementById('canvas').focus();
}

function selectFiles(files)
{
   $('#btnAdd').addClass('disabled');
   $('#icnAdd').removeClass('fa-plus');
   $('#icnAdd').addClass('fa-spinner spinning');
   var count = files.length;

   for (var i = 0; i < files.length; i++) 
   {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.readAsArrayBuffer(files[i]);
      filereader.onload = function(){uploadData(this.result, this.file_name)};
      filereader.onloadend = function(evt) 
      {
         if (evt.target.readyState == FileReader.DONE)
         {
            console.log("WEBPLAYER: File: " + this.file_name + " Upload Complete");
            $('#btnAdd').removeClass('disabled');
            $('#icnAdd').removeClass('fa-spinner spinning');
            $('#icnAdd').addClass('fa-plus');
         }
       }
   }
}

function uploadData(data,name)
{
   var dataView = new Uint8Array(data);
   FS.createDataFile('/', name, dataView, true, false);

   var data = FS.readFile(name,{ encoding: 'binary' });
   FS.writeFile('/home/web_user/userdata/content/' + name, data ,{ encoding: 'binary' });
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

function switchCore(corename) {
   localStorage.setItem("core", corename);
}

function switchStorage(backend) {
   if (backend != localStorage.getItem("backend"))
   {
      localStorage.setItem("backend", backend);
      location.reload();
   }
}

// When the browser has loaded everything.
$(function() {
   // Find which core to load.
   var core = localStorage.getItem("core", core);
   if (!core) {
     core = 'gambatte';
   }
   // Show the current core as the active core.
   $('.nav-item.' + core).addClass('active');

   // Load the Core's related JavaScript.
   $.getScript(core + '_libretro.js', function () 
   {
    // Activate the Start RetroArch button.
    $('#btnRun').removeClass('disabled');
    $('#icnRun').removeClass('fa-spinner spinning');
    $('#icnRun').addClass('fa-play');

    if (localStorage.getItem("backend") == "dropbox")
    {
      $('#lblDrop').addClass('active');
      $('#lblLocal').removeClass('active');
      dropboxInit();
    }
    else {
      $('#lblDrop').removeClass('active');
      $('#lblLocal').addClass('active');
      setupFileSystem("browser");
      setupFolderStructure();
    }
    //$('#dropdownMenu1').text(localStorage.getItem("core"));
    /**
     * Attempt to disable some default browser keys.
     */

     var util = { };

   document.addEventListener('keydown', function(e)
   {
      var key = util.key[e.which];
      if( key )
      {
         e.preventDefault();
      }
   })

   util.key = { 
      9: "tab",
      13: "enter",
      16: "shift",
      18: "alt",
      27: "esc",
      33: "rePag",
      34: "avPag",
      35: "end",
      36: "home",
      37: "left",
      38: "up",
      39: "right",
      40: "down",
      112: "F1",
      113: "F2",
      114: "F3",
      115: "F4",
      116: "F5",
      117: "F6",
      118: "F7",
      119: "F8",
      120: "F9",
      121: "F10",
      122: "F11",
      123: "F12"
   }

   });
 });