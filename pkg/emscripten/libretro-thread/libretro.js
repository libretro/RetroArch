/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */

var filesystem_ready = false;
var retroarch_ready = false;
var setImmediate;

var Module = {
   noInitialRun: true,
   arguments: ["-v", "--menu"],
   noImageDecoding: true,
   noAudioDecoding: true,

  retroArchSend: function(msg) {
    this.EmscriptenSendCommand(msg);
  },
  retroArchRecv: function() {
    return this.EmscriptenReceiveCommandReply();
  },
   preRun: [
      function(module) {
         Module.ENV['OPFS'] = "/home/web_user/retroarch";
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
  const root = await navigator.storage.getDirectory();
  for await (const handle of root.values()) {
    await root.removeEntry(handle.name, {recursive: true});
  }
  document.getElementById("btnClean").disabled = true;
}

function appInitialized()
{
  /* Need to wait for the wasm runtime to load before enabling the Run button. */
  if (retroarch_ready && filesystem_ready)
  {
    preLoadingComplete();
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
  setupWorker.postMessage({command:"upload_file", name:name, data:data}, {transfer:[data]});
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

const setupWorker = new Worker("libretro.worker.js");
setupWorker.onmessage = (msg) => {
  if(msg.data.command == "loaded_bundle") {
    filesystem_ready = true;
    localStorage.setItem("asset_time", msg.data.time);
    appInitialized();
  } else if(msg.data.command == "uploaded_file") {
    // console.log("finished upload of",msg.data.name);
  }
}
setupWorker.postMessage({command:"load_bundle",time:localStorage.getItem("asset_time") ?? ""});
