var count = 0;
var filename = "";
var firstRun = true;


function setupFolders()
{
   console.log("setupFolders");

   if(localStorage.getItem("folders_inited")!="true")
   {
        FS.createFolder('/home','web_user',true,true);
        FS.createFolder('/home/web_user','retroarch',true,true);
        FS.createFolder('/home/web_user/retroarch','saves',true,true);
        FS.createFolder('/home/web_user/retroarch','states',true,true);
        FS.createFolder('/home/web_user/retroarch','system',true,true);
        FS.createFolder('/home/web_user/retroarch','cheat',true,true);
        FS.createFolder('/home/web_user/retroarch','remap',true,true);
        FS.createFolder('/home/web_user','.config',true,true);
        FS.createFolder('/home/web_user/.config','retroarch',true,true);

        localStorage.setItem("folders_inited","true");
        console.log('Folders initialized: ' + localStorage.getItem("folders_inited"));
   }
}

function createConfig()
{
   console.log("createConfig");

   if(localStorage.getItem("cfg_inited")!="true")
   {
      var config = 'input_player1_select = shift\n';
      config += 'audio_latency = 96\n'
      config += 'video_font_size = 16\n'
      config += 'rgui_browser_directory = /content\n';
      config += 'savefile_directory = /home/web_user/retroarch/saves/\n';
      config += 'savestate_directory = /home/web_user/retroarch/states/\n';
      config += 'system_directory = /home/web_user/retroarch/system/\n';
      config += 'rgui_config_directory = /home/web_user/.config/retroarch/\n';
      config += 'input_remapping_directory = /home/web_user/retroarch/remap/\n';
      config += 'cheat_database_path = /home/web_user/retroarch/cheat/\n';
      FS.writeFile('/home/web_user/.config/retroarch/retroarch.cfg',config);

      localStorage.setItem("cfg_inited","true");
      console.log('Config initialized: ' + localStorage.getItem("cfg_inited"));
   }
}

function setupFileSystem()
{
   console.log("setupFileSystem");

   if(localStorage.getItem("fs_inited")!="true")
   {
      var lsfs = new BrowserFS.FileSystem.LocalStorage();
      BrowserFS.initialize(lsfs);
      var BFS = new BrowserFS.EmscriptenFS();

      FS.mount(BFS, {root: '/'}, '/home');

      console.log('Filesystem initialized');
   }
   else
   {
      console.log('Filesystem already initialized');
   }
}

function runEmulator(files)
{

   if (Modernizr.localstorage)
   {
      if(firstRun)
         setupFileSystem();
   }

   setupFolders();
   createConfig();

   if(firstRun)
      FS.createFolder('/','content',true,true);

   count = files.length;
   for (var i = 0; i < files.length; i++)
   {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.readAsArrayBuffer(files[i]);
      if(firstRun)
      {
         filereader.onload = function(){uploadContent(this.result, '/content/' + this.file_name)};
         firstRun = false;
         initFromData();
      }
      else
         filereader.onload = function(){uploadContent(this.result, '/content/' + this.file_name)};

    }
}

function uploadContent(data, name)
{
   var dataView = new Uint8Array(data);
   FS.createDataFile('/', name, dataView, true, false);
}

function copyFile(src,dest)
{
   console.log('copying: ' + src + ' to: ' + dest);
   var contents = FS.readFile(src,{ encoding: 'binary' });
   console.log(contents);
   FS.writeFile(dest,contents,{ encoding: 'binary' });
}

function uploadData(files, path)
{
   count = files.length;
   for (var i = 0; i < files.length; i++)
   {
      filereader = new FileReader();
      filereader.file_name = files[i].name;
      filereader.readAsArrayBuffer(files[i]);
      filereader.onload = function(){uploadContent(this.result, '/tmp/' + this.file_name)};
      filereader.onloadend = function(){copyFile('/tmp/' + this.file_name,'/home/web_user/retroarch/' + path + '/' + this.file_name)};

    }
}

function initFromData()
{

   count--;
   if (count === 0)
   {
         document.getElementById('canvas_div').style.display = 'block';
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
