/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */
var client = new Dropbox.Client({ key: "--your-api-key--" }); /* setup key*/
var BrowserFS = BrowserFS;
var afs;

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

function cleanupStorage()
{
   localStorage.clear();
   if (BrowserFS.FileSystem.IndexedDB.isAvailable()) 
   {
      var req = indexedDB.deleteDatabase("RetroArch");
      req.onsuccess = function () {
         console.log("Deleted database successfully");
      };
      req.onerror = function () {
         console.log("Couldn't delete database");
      };
      req.onblocked = function () {
         console.log("Couldn't delete database due to the operation being blocked");
      };
   }

   document.getElementById("btnClean").disabled = true;
}


function dropboxInit()
{
  //document.getElementById("btnDrop").disabled = true;
  //$('#icnDrop').removeClass('fa-dropbox');
  //$('#icnDrop').addClass('fa-spinner fa-spin');
  

  client.authDriver(new Dropbox.AuthDriver.Redirect());
  client.authenticate({ rememberUser: true }, function(error, client)
  {
     if (error) 
     {
        return showError(error);
     }
     dropboxSync(client, dropboxSyncComplete);
  });
}

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

function dropboxSyncComplete()
{
  //$('#icnDrop').removeClass('fa-spinner').removeClass('fa-spin');
  //$('#icnDrop').addClass('fa-check');
  console.log("WEBPLAYER: Dropbox sync successful");

  setupFileSystem("dropbox");
  preLoadingComplete();
}

function idbfsInit()
{
   document.getElementById("btnLocal").disabled = true;
   $('#icnLocal').removeClass('fa-globe');
   $('#icnLocal').addClass('fa-spinner fa-spin');
   var imfs = new BrowserFS.FileSystem.InMemory();
   if (BrowserFS.FileSystem.IndexedDB.isAvailable()) 
   {
      afs = new BrowserFS.FileSystem.AsyncMirror(imfs, 
         new BrowserFS.FileSystem.IndexedDB(function(e, fs) 
      {
         if (e)
         {
            //fallback to imfs
            afs = new BrowserFS.FileSystem.InMemory();
            console.log("WEBPLAYER: error: " + e + " falling back to in-memory filesystem");
            setupFileSystem("browser");
            preLoadingComplete();
         } 
         else 
         {
            // initialize afs by copying files from async storage to sync storage.
            afs.initialize(function (e) 
            {
               if (e) 
               {
                  afs = new BrowserFS.FileSystem.InMemory();
                  console.log("WEBPLAYER: error: " + e + " falling back to in-memory filesystem");
                  setupFileSystem("browser");
                  preLoadingComplete();
               }
               else 
               {
                  idbfsSyncComplete();
               }
            });
         }
      },
      "RetroArch"));
   }
}

function idbfsSyncComplete()
{
   $('#icnLocal').removeClass('fa-spinner').removeClass('fa-spin');
   $('#icnLocal').addClass('fa-check');
   console.log("WEBPLAYER: idbfs setup successful");

   setupFileSystem("browser");
   preLoadingComplete();
}

function preLoadingComplete()
{
   /* Make the Preview image clickable to start RetroArch. */
   $('.webplayer-preview').addClass('loaded').click(function () {
      startRetroArch();
      return false;
  });
  document.getElementById("btnRun").disabled = false;
  $('#btnRun').removeClass('disabled');
}

function setupFileSystem(backend)
{
   /* create a mountable filesystem that will server as a root
      mountpoint for browserfs */
   var mfs =  new BrowserFS.FileSystem.MountableFileSystem();

   /* setup this if you setup your server to serve assets or core assets, 
      you can find more information in the included README */

   /* create an XmlHttpRequest filesystem for the bundled data 
      uncomment this section if you want XMB assets, Overlays, Shaders, etc.
   var xfs1 =  new BrowserFS.FileSystem.XmlHttpRequest
      ("--your-assets-index-file-name--", "--your-index-url--");*/
   /* create an XmlHttpRequest filesystem for content
      uncomment this section if you want to serve content
   var xfs2 =  new BrowserFS.FileSystem.XmlHttpRequest
      ("--your-content-index-file-name--", "--your-index-url--");*/

   console.log("WEBPLAYER: initializing filesystem: " + backend);
   mfs.mount('/home/web_user/retroarch/userdata', afs);

   /* setup this if you setup your server to serve assets or core assets, 
      you can find more information in the included README */
   /*
   mfs.mount('/home/web_user/retroarch/bundle', xfs1);
   mfs.mount('/home/web_user/retroarch/userdata/content/', xfs2);
   */
   BrowserFS.initialize(mfs);
   var BFS = new BrowserFS.EmscriptenFS();
   FS.mount(BFS, {root: '/home'}, '/home');
   console.log("WEBPLAYER: " + backend + " filesystem initialization successful");
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

function startRetroArch()
{
   $('.webplayer').show();
   $('.webplayer-preview').hide();
   //document.getElementById("btnDrop").disabled = true;
   document.getElementById("btnRun").disabled = true;
  
   $('#btnFullscreen').removeClass('disabled');
   $('#btnMenu').removeClass('disabled');
   $('#btnAdd').removeClass('disabled');
   $('#btnRom').removeClass('disabled');

   document.getElementById("btnAdd").disabled = false;
   document.getElementById("btnRom").disabled = false;
   document.getElementById("btnMenu").disabled = false;
   document.getElementById("btnFullscreen").disabled = false;

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
         console.log("WEBPLAYER: file: " + this.file_name + " upload complete");
         if (evt.target.readyState == FileReader.DONE)
         {
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
   FS.writeFile('/home/web_user/retroarch/userdata/content/' + name, data ,{ encoding: 'binary' });
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
  /**
   * Attempt to disable some default browser keys.
   */
	var keys = {
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
  };
	window.addEventListener('keydown', function (e) {
    if (keys[e.which]) {
      e.preventDefault();
    }
  });

   // Switch the core when selecting one.
   $('#core-selector a').click(function () {
      var coreChoice = $(this).data('core');
      switchCore(coreChoice);
   });

   // Find which core to load.
   var core = localStorage.getItem("core", core);
   if (!core) {
      core = 'gambatte';
   }
   // Make the core the selected core in the UI.
   var coreTitle = $('#core-selector a[data-core="' + core + '"]').addClass('active').text();
   $('#dropdownMenu1').text(coreTitle);

   // Load the Core's related JavaScript.
   $.getScript(core + '_libretro.js', function () 
   {
      $('#icnRun').removeClass('fa-spinner').removeClass('fa-spin');
      $('#icnRun').addClass('fa-play');

      if (localStorage.getItem("backend") == "dropbox")
      {
         $('#lblDrop').addClass('active');
         $('#lblLocal').removeClass('active');
         dropboxInit();
      }
      else
      {
         $('#lblDrop').removeClass('active');
         $('#lblLocal').addClass('active');
         idbfsInit();
      }
   });
 });

function keyPress(k)
{
   kp(k, "keydown");
   setTimeout(function(){kp(k, "keyup")}, 50);
}

kp = function(k, event) {
   var oEvent = document.createEvent('KeyboardEvent');
 
   // Chromium Hack
   Object.defineProperty(oEvent, 'keyCode', {
      get : function() {
         return this.keyCodeVal;
      }
   });
   Object.defineProperty(oEvent, 'which', {
      get : function() {
         return this.keyCodeVal;
      }
   });
 
   if (oEvent.initKeyboardEvent) {
      oEvent.initKeyboardEvent(event, true, true, document.defaultView, false, false, false, false, k, k);
   } else {
      oEvent.initKeyEvent(event, true, true, document.defaultView, false, false, false, false, k, 0);
   }
 
   oEvent.keyCodeVal = k;
 
   if (oEvent.keyCode !== k) {
      alert("keyCode mismatch " + oEvent.keyCode + "(" + oEvent.which + ")");
   }
 
   document.dispatchEvent(oEvent);
   document.getElementById('canvas').focus();
}
