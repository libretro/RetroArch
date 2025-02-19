/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */
var BrowserFS = BrowserFS;
var afs;
var initializationCount = 0;

var Module = {
   noInitialRun: true,
   arguments: ["-v", "--menu", "-c", "/home/web_user/retroarch/userdata/retroarch.cfg"],

   encoder: new TextEncoder(),
   message_queue: [],
   message_out: [],
   message_accum: "",

   retroArchSend: function(msg) {
      let bytes = this.encoder.encode(msg + "\n");
      this.message_queue.push([bytes, 0]);
   },
   retroArchRecv: function() {
      let out = this.message_out.shift();
      if (out == null && this.message_accum != "") {
         out = this.message_accum;
         this.message_accum = "";
      }
      return out;
   },
   preRun: [
      function(module) {
         function stdin() {
            // Return ASCII code of character, or null if no input
            while (module.message_queue.length > 0) {
               var msg = module.message_queue[0][0];
               var index = module.message_queue[0][1];
               if (index >= msg.length) {
                  module.message_queue.shift();
               } else {
                  module.message_queue[0][1] = index + 1;
                  // assumption: msg is a uint8array
                  return msg[index];
               }
            }
            return null;
         }

         function stdout(c) {
            if (c == null) {
               // flush
               if (module.message_accum != "") {
                  module.message_out.push(module.message_accum);
                  module.message_accum = "";
               }
            } else {
               let s = String.fromCharCode(c);
               if (s == "\n") {
                  if (module.message_accum != "") {
                     module.message_out.push(module.message_accum);
                     module.message_accum = "";
                  }
               } else {
                  module.message_accum = module.message_accum + s;
               }
            }
         }
         module.FS.init(stdin, stdout);
      }
   ],
   postRun: [],
   onRuntimeInitialized: function() {
      appInitialized();
   },
   print: function(text) {
      console.log("stdout:", text);
   },
   printErr: function(text) {
      console.log("stderr:", text);
   },
   canvas: document.getElementById("canvas"),
   totalDependencies: 0,
   monitorRunDependencies: function(left) {
      this.totalDependencies = Math.max(this.totalDependencies, left);
   }
};


function cleanupStorage() {
   localStorage.clear();
   if (BrowserFS.FileSystem.IndexedDB.isAvailable()) {
      var req = indexedDB.deleteDatabase("RetroArch");
      req.onsuccess = function() {
         console.log("Deleted database successfully");
      };
      req.onerror = function() {
         console.error("Couldn't delete database");
      };
      req.onblocked = function() {
         console.error("Couldn't delete database due to the operation being blocked");
      };
   }

   document.getElementById("btnClean").disabled = true;
}

function idbfsInit() {
   $('#icnLocal').removeClass('fa-globe');
   $('#icnLocal').addClass('fa-spinner fa-spin');
   var imfs = new BrowserFS.FileSystem.InMemory();
   if (BrowserFS.FileSystem.IndexedDB.isAvailable()) {
      afs = new BrowserFS.FileSystem.AsyncMirror(imfs,
         new BrowserFS.FileSystem.IndexedDB(function(e, fs) {
               if (e) {
                  // fallback to imfs
                  afs = new BrowserFS.FileSystem.InMemory();
                  console.error("WEBPLAYER: error: " + e + " falling back to in-memory filesystem");
                  appInitialized();
               } else {
                  // initialize afs by copying files from async storage to sync storage.
                  afs.initialize(function(e) {
                     if (e) {
                        afs = new BrowserFS.FileSystem.InMemory();
                        console.error("WEBPLAYER: error: " + e + " falling back to in-memory filesystem");
                        appInitialized();
                     } else {
                        idbfsSyncComplete();
                     }
                  });
               }
            },
            "RetroArch"));
   }
}

function idbfsSyncComplete() {
   $('#icnLocal').removeClass('fa-spinner').removeClass('fa-spin');
   $('#icnLocal').addClass('fa-check');
   console.log("WEBPLAYER: idbfs setup successful");

   appInitialized();
}

function appInitialized() {
   /* Need to wait for the file system, the wasm runtime, and the zip download
      to complete before enabling the Run button. */
   initializationCount++;
   if (initializationCount == 3) {
      setupFileSystem("browser");
      preLoadingComplete();
   }
}

function preLoadingComplete() {
   // Make the Preview image clickable to start RetroArch.
   $('.webplayer-preview').addClass('loaded').click(function() {
      startRetroArch();
      return false;
   });
   $('#btnRun').removeClass('disabled').removeAttr("disabled").click(function() {
      startRetroArch();
      return false;
   });
}

var zipTOC;

function zipfsInit() {
   // 256 MB max bundle size
   let buffer = new ArrayBuffer(256 * 1024 * 1024);
   let bufferView = new Uint8Array(buffer);
   let idx = 0;
   // bundle should be in five parts (this can be changed later)
   Promise.all([fetch("assets/frontend/bundle.zip.aa"),
      fetch("assets/frontend/bundle.zip.ab"),
      fetch("assets/frontend/bundle.zip.ac"),
      fetch("assets/frontend/bundle.zip.ad"),
      fetch("assets/frontend/bundle.zip.ae")
   ]).then(function(resps) {
      Promise.all(resps.map((r) => r.arrayBuffer())).then(function(buffers) {
         for (let buf of buffers) {
            if (idx + buf.byteLength > buffer.maxByteLength) {
               console.error("WEBPLAYER: error: bundle.zip is too large");
            }
            bufferView.set(new Uint8Array(buf), idx, buf.byteLength);
            idx += buf.byteLength;
         }
         BrowserFS.FileSystem.ZipFS.computeIndex(BrowserFS.BFSRequire('buffer').Buffer(new Uint8Array(buffer, 0, idx)), function(toc) {
            zipTOC = toc;
            appInitialized();
         });
      })
   });
}

function setupFileSystem(backend) {
   // create a mountable filesystem that will server as a root mountpoint for browserfs
   var mfs = new BrowserFS.FileSystem.MountableFileSystem();

   // create an XmlHttpRequest filesystem for the bundled data
   var xfs1 = new BrowserFS.FileSystem.ZipFS(zipTOC);
   // create an XmlHttpRequest filesystem for core assets
   var xfs2 = new BrowserFS.FileSystem.XmlHttpRequest(".index-xhr", "assets/cores/");

   console.log("WEBPLAYER: initializing filesystem: " + backend);
   mfs.mount('/home/web_user/retroarch/bundle', xfs1);
   mfs.mount('/home/web_user/retroarch/userdata', afs);
   mfs.mount('/home/web_user/retroarch/userdata/content/downloads', xfs2);
   BrowserFS.initialize(mfs);
   var BFS = new BrowserFS.EmscriptenFS(Module.FS, Module.PATH, Module.ERRNO_CODES);
   Module.FS.mount(BFS, {
      root: '/home'
   }, '/home');
   console.log("WEBPLAYER: " + backend + " filesystem initialization successful");
}

// Retrieve the value of the given GET parameter.
function getParam(name) {
   var results = new RegExp('[?&]' + name + '=([^&#]*)').exec(window.location.href);
   if (results) {
      return results[1] || null;
   }
}

function startRetroArch() {
   $('.webplayer').show();
   $('.webplayer-preview').hide();
   document.getElementById("btnRun").disabled = true;

   $('#btnAdd').removeClass("disabled").removeAttr("disabled").click(function() {
      $('#btnRom').click();
   });
   $('#btnRom').removeAttr("disabled").change(function(e) {
      selectFiles(e.target.files);
   });
   $('#btnMenu').removeClass("disabled").removeAttr("disabled").click(function() {
      Module._cmd_toggle_menu();
      Module.canvas.focus();
   });
   $('#btnFullscreen').removeClass("disabled").removeAttr("disabled").click(function() {
      Module.requestFullscreen(false);
      Module.canvas.focus();
   });

   Module.canvas.focus();
   Module.canvas.addEventListener("pointerdown", function() {
      Module.canvas.focus();
   }, false);
   Module.callMain(Module.arguments);
}

function selectFiles(files) {
   $('#btnAdd').addClass('disabled');
   $('#icnAdd').removeClass('fa-plus');
   $('#icnAdd').addClass('fa-spinner spinning');
   var count = files.length;

   for (var i = 0; i < count; i++) {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.readAsArrayBuffer(files[i]);
      filereader.onload = function() {
         uploadData(this.result, this.file_name)
      };
      filereader.onloadend = function(evt) {
         console.log("WEBPLAYER: file: " + this.file_name + " upload complete");
         if (evt.target.readyState == FileReader.DONE) {
            $('#btnAdd').removeClass('disabled');
            $('#icnAdd').removeClass('fa-spinner spinning');
            $('#icnAdd').addClass('fa-plus');
         }
      }
   }
}

function uploadData(data, name) {
   var dataView = new Uint8Array(data);
   Module.FS.createDataFile('/', name, dataView, true, false);

   var data = Module.FS.readFile(name, {
      encoding: 'binary'
   });
   Module.FS.writeFile('/home/web_user/retroarch/userdata/content/' + name, data, {
      encoding: 'binary'
   });
   Module.FS.unlink(name);
}

function switchCore(corename) {
   localStorage.setItem("core", corename);
}

function switchStorage(backend) {
   if (backend != localStorage.getItem("backend")) {
      localStorage.setItem("backend", backend);
      location.reload();
   }
}

// When the browser has loaded everything.
$(function() {
   // Enable data clear
   $('#btnClean').click(function() {
      cleanupStorage();
   });

   // Enable all available ToolTips.
   $('.tooltip-enable').tooltip({
      placement: 'right'
   });

   // Allow hiding the top menu.
   $('.showMenu').hide();
   $('#btnHideMenu, .showMenu').click(function() {
      $('nav').slideToggle('slow');
      $('.showMenu').toggle('slow');
   });

   // Attempt to disable some default browser keys.
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
   window.addEventListener('keydown', function(e) {
      if (keys[e.which]) {
         e.preventDefault();
      }
   });

   // Switch the core when selecting one.
   $('#core-selector a').click(function() {
      var coreChoice = $(this).data('core');
      switchCore(coreChoice);
   });
   // Find which core to load.
   var core = localStorage.getItem("core", core);
   if (!core) {
      core = 'gambatte';
   }
   loadCore(core);
});

function loadCore(core) {
   // Make the core the selected core in the UI.
   var coreTitle = $('#core-selector a[data-core="' + core + '"]').addClass('active').text();
   $('#dropdownMenu1').text(coreTitle);
   // Load the Core's related JavaScript.
   import("./" + core + "_libretro.js").then(script => {
      script.default(Module).then(mod => {
         Module = mod;
         $('#icnRun').removeClass('fa-spinner').removeClass('fa-spin');
         $('#icnRun').addClass('fa-play');
         $('#lblDrop').removeClass('active');
         $('#lblLocal').addClass('active');
         idbfsInit();
         zipfsInit();
      }).catch(err => {
         console.error("Couldn't instantiate module", err);
         throw err;
      });
   }).catch(err => {
      console.error("Couldn't load script", err);
      throw err;
   });
}
