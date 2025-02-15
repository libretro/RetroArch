/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */

var retroarch_ready = false;
var setImmediate;

var Module = {
   noInitialRun: true,
   arguments: ["-v", "--menu"],
   noImageDecoding: true,
   noAudioDecoding: true,

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
      retroarch_ready = true;
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


async function cleanupStorage()
{
  localStorage.clear();
  let storage = await navigator.storage.getDirectory();
  await storage.remove({recursive: true});
  document.getElementById("btnClean").disabled = true;
}

function appInitialized()
{
  /* Need to wait for the wasm runtime to load before enabling the Run button. */
  if (retroarch_ready)
  {
    setupFileSystem().then(() => { preLoadingComplete(); });
  }
 }

function preLoadingComplete() {
   $('#icnRun').removeClass('fa-spinner').removeClass('fa-spin');
   $('#icnRun').addClass('fa-play');
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

async function setupZipFS(zipBuf) {
  async function writeFile(path, data) {
    const dir_end = path.lastIndexOf("/");
    const parent = path.substr(0, dir_end);
    const child = path.substr(dir_end+1);
    const parent_dir = await mkdirTree(parent);
    //console.log("about to create", parent, "/", child);
    const file = await parent_dir.getFileHandle(child,{create:true});
    const stream = await file.createWritable();
    await stream.write(data);
    await stream.close();
  }
  async function mkdirTree(path) {
    const parts = path.split("/");
    let here = root;
    for (const part of parts) {
      if (part == "") { continue; }
      here = await here.getDirectoryHandle(part, {create:true});
    }
    return here;
  }
  const root = await navigator.storage.getDirectory();
  const mount = "assets";
  const zipReader = new zip.ZipReader(new zip.Uint8ArrayReader(zipBuf), {useWebWorkers:false});
  const entries = await zipReader.getEntries();
  for(const file of entries) {
    if (file.getData && !file.directory) {
      const writer = new zip.Uint8ArrayWriter();
      const data = await file.getData(writer);
      await writeFile(mount+"/"+file.filename, data);
    } else if (file.directory) {
      await mkdirTree(mount+"/"+file.filename);
    }
  }
  await zipReader.close();
}


async function setupFileSystem()
{
  let old_timestamp = localStorage.getItem("asset_time") ?? "";
  let resp = await fetch("assets-minimal.zip", {
    headers: {
      "If-Modified-Since": old_timestamp
    }
  });
  if (resp.status == 200) {
    await setupZipFS(await resp.bytes());
    localStorage.setItem("asset_time", resp.headers.get("last-modified"));
  } else {
    await resp.text();
  }
  Module.FS.mkdirTree("/home/web_user/retroarch/");
  Module.FS.mount(Module.OPFS, {}, "/home/web_user/retroarch");
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
   Module.FS.mkdirTree("/home/web_user/retroarch/userdata/content");
   Module.FS.writeFile('/home/web_user/retroarch/userdata/content/'+name, data);
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

async function downloadScript(src) {
  let resp = await fetch(src);
  let blob = await resp.blob();
  return blob;
}

function loadCore(core) {
   // Make the core the selected core in the UI.
   var coreTitle = $('#core-selector a[data-core="' + core + '"]').addClass('active').text();
   $('#dropdownMenu1').text(coreTitle);
   downloadScript("./"+core+"_libretro.js").then(scriptBlob => {
      Module.mainScriptUrlOrBlob = scriptBlob;
      import(URL.createObjectURL(scriptBlob)).then(script => {
         script.default(Module).then(mod => {
            Module = mod;
         }).catch(err => { console.error("Couldn't instantiate module",err); throw err; });
      }).catch(err => { console.error("Couldn't load script",err); throw err; });
   });
}
