      var count = 0;
	  var filename = "";
	  var firstRun = true;
      function runEmulator(files){
		if(firstRun)
		{
		  Module.FS_createFolder('/', 'etc', true, true);
		  Module.FS_createFolder('/', 'retroarch', true, true);
		  Module.FS_createFolder('/retroarch', 'content', true, true);		  
		  Module.FS_createFolder('/retroarch', 'saves', true, true);
		  Module.FS_createFolder('/retroarch', 'states', true, true);
		  Module.FS_createFolder('/retroarch', 'system', true, true);
		  firstRun = false;
		}	
		
        count = files.length;
        for (var i = 0; i < files.length; i++) {
          filereader = new FileReader();
          filereader.file_name = files[i].name;
          filereader.onload = function(){initFromData(this.result, '/retroarch/content/' + this.file_name)};
          filereader.readAsArrayBuffer(files[i]);
        }
      }
	  
      function uploadSaveFiles(files){	
        count = files.length;
        for (var i = 0; i < files.length; i++) {
          filereader = new FileReader();
          filereader.file_name = files[i].name;
          filereader.onload = function(){initFromData(this.result, '/retroarch/saves/' + this.file_name)};
          filereader.readAsArrayBuffer(files[i]);
        }
      }	  
	  
	  
      function uploadSaveStateFiles(files){	
        count = files.length;
        for (var i = 0; i < files.length; i++) {
          filereader = new FileReader();
          filereader.file_name = files[i].name;
          filereader.onload = function(){initFromData(this.result, '/retroarch/states/' + this.file_name)};
          filereader.readAsArrayBuffer(files[i]);
        }
      }	  	  
	   
      function initFromData(data, name){
		var dataView = new Uint8Array(data);
		Module.FS_createDataFile('/', name, dataView, true, false);
        count--;
        if (count === 0) {
          var config = 'input_player1_select = shift\n';
          var latency = parseInt(document.getElementById('latency').value, 10);
          if (isNaN(latency)) latency = 96;
          config += 'audio_latency = ' + latency + '\n'
          if (document.getElementById('vsync').checked)
            config += 'video_vsync = true\n';
          else
            config += 'video_vsync = false\n';
		  config += 'rgui_browser_directory = /retroarch/content/\n';
		  config += 'savefile_directory = /retroarch/saves/\n';
		  config += 'savestate_directory = /retroarch/states/\n';
		  config += 'system_directory = /retroarch/system/\n';
          Module.FS_createDataFile('/etc', 'retroarch.cfg', config, true, true);
          document.getElementById('canvas_div').style.display = 'block';
          document.getElementById('vsync').disabled = true;
          document.getElementById('vsync-label').style.color = 'gray';
          document.getElementById('latency').disabled = true;
          document.getElementById('latency-label').style.color = 'gray';
          Module['callMain'](Module['arguments']);
        }
      }
      // connect to canvas
      var Module = {
	     noInitialRun: true,
		  arguments: ["--verbose", "--menu"],
        preRun: [],
        postRun: [],
        print: (function() {
          var element = document.getElementById('output');
          element.value = ''; // clear browser cache
          return function(text) {
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
        printErr: function(text) {
          var text = Array.prototype.slice.call(arguments).join(' ');
          var element = document.getElementById('output');
          element.value += text + "\n";
          element.scrollTop = 99999; // focus on bottom
        },
        canvas: document.getElementById('canvas'),
        setStatus: function(text) {
          if (Module.setStatus.interval) clearInterval(Module.setStatus.interval);
          var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
          var statusElement = document.getElementById('status');
          var progressElement = document.getElementById('progress');
          if (m) {
            text = m[1];
            progressElement.value = parseInt(m[2])*100;
            progressElement.max = parseInt(m[4])*100;
            progressElement.hidden = false;
          } else {
            progressElement.value = null;
            progressElement.max = null;
            progressElement.hidden = true;
          }
          statusElement.innerHTML = text;
        },
        totalDependencies: 0,
        monitorRunDependencies: function(left) {
          this.totalDependencies = Math.max(this.totalDependencies, left);
          Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
        }
      };
      Module.setStatus('Downloading...');
