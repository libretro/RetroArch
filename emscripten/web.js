var count = 0;
var filename = "";
var firstRun = true;


function setupFolders()
{
   if (Modernizr.localstorage) {
     console.log("local storage available");
	 
	 if(localStorage.getItem("initialized")!="true")
		 
		 {
           FS.createFolder('/data','content',true,true);
           FS.createFolder('/data','saves',true,true);
           FS.createFolder('/data','states',true,true);
	       FS.createFolder('/data','system',true,true);		   
		   localStorage.setItem("initialized","true");
		   console.log(localStorage.getItem("initialized"));
		 }
   }
}

function setupBFS() {
  var lsfs = new BrowserFS.FileSystem.LocalStorage();
  BrowserFS.initialize(lsfs);
  var BFS = new BrowserFS.EmscriptenFS();
  FS.createFolder(FS.root, 'data', true, true);
  FS.mount(BFS, {root: '/'}, '/data');
}

function runEmulator(files)
{
   console.log("runEmulator");
   setupBFS();   

   if(firstRun)
   {
         setupFolders();
		 
		 Module.FS_createFolder('/', 'etc', true, true);
		 Module.FS_createFolder('/', 'retroarch', true, true);
         firstRun = false;
   }	
   
   count = files.length;
   for (var i = 0; i < files.length; i++) 
   {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.onload = function(){initFromData(this.result, '/retroarch/' + this.file_name)};
      filereader.readAsArrayBuffer(files[i]);



    }
}
	  
function uploadSaveFiles(files)
{
   console.log("uploadSaveFiles");

   count = files.length;
   for (var i = 0; i < files.length; i++) 
   {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.onload = function(){initFromData(this.result, '/data/saves/' + this.file_name)};
      filereader.readAsArrayBuffer(files[i]);
    }
}	  
	  
	  
function initFromData(data, name)
{

   console.log("initFromData");

   var dataView = new Uint8Array(data);
   Module.FS_createDataFile('/', name, dataView, true, false);
   count--;
   if (count === 0) 
   {
      var config = 'input_player1_select = shift\n';
      var latency = parseInt(document.getElementById('latency').value, 10);
      if (isNaN(latency)) 
         latency = 96;
      
      config += 'audio_latency = ' + latency + '\n'
      if (document.getElementById('vsync').checked)
         config += 'video_vsync = true\n';
      else
         config += 'video_vsync = false\n';
         config += 'rgui_browser_directory = /retroarch/\n';
	 config += 'savefile_directory = /data/saves\n';
	 config += 'savestate_directory = /data/states\n';
	 config += 'system_directory = /data/system/\n';
         Module.FS_createDataFile('/etc', 'retroarch.cfg', config, true, true);
         document.getElementById('canvas_div').style.display = 'block';
         document.getElementById('vsync').disabled = true;
         document.getElementById('vsync-label').style.color = 'gray';
         document.getElementById('latency').disabled = true;
         document.getElementById('latency-label').style.color = 'gray';
         Module['callMain'](Module['arguments']);
   }
}

var Module = 
{
   noInitialRun: true,
   arguments: ["--verbose", "--menu"],
   preRun: [],
   postRun: [],
   print: (function()
   {
      var element = document.getElementById('output');
      element.value = ''; // clear browser cache
      return function(text) 
      {
            text = Array.prototype.slice.call(arguments).join(' ');
            // These replacements are necessary if you render to raw HTML
            //text = text.replace(/&/g, "&amp;");
            //text = text.replace(/</g, "&lt;");
            //text = text.replace(/>/g, "&gt;");
            //text = text.replace('\n', '<br>', 'g');
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
   setStatus: function(text) 
   {
      if (Module.setStatus.interval) 
         clearInterval(Module.setStatus.interval);
      var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
      var statusElement = document.getElementById('status');
      var progressElement = document.getElementById('progress');
      if (m) 
      {
         text = m[1];
         progressElement.value = parseInt(m[2])*100;
         progressElement.max = parseInt(m[4])*100;
         progressElement.hidden = false;
      } 
      else 
      {
         progressElement.value = null;
         progressElement.max = null;
         progressElement.hidden = true;
       }
       statusElement.innerHTML = text;
   },

   totalDependencies: 0,
   monitorRunDependencies: function(left) 
   {
      this.totalDependencies = Math.max(this.totalDependencies, left);
      Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
   }
};

Module.setStatus('Downloading...');
