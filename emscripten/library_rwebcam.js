//"use strict";

var LibraryRWebCam = {
   $RWC: {
      /*
      run modes:
      0: not running
      1: waiting for user to confirm webcam
      2: running
      */
      runMode: 0,
      videoElement: null
   },

   RWebCamInit: function() {
      RWC.runMode = 0;

      if (!navigator) return 0;

      navigator.getMedia = navigator.getUserMedia ||
                           navigator.webkitGetUserMedia ||
                           navigator.mozGetUserMedia ||
                           navigator.msGetUserMedia;

      if (!navigator.getMedia) return 0;

      RWC.videoElement = document.createElement("video");

      RWC.runMode = 1;

      navigator.getMedia({video: true, audio: false}, function(stream) {
         RWC.videoElement.autoplay = true;
         RWC.videoElement.src = URL.createObjectURL(stream);
         RWC.runMode = 2;
      }, function (err) {
         console.log("webcam request failed", err);
         RWC.runMode = 0;
      });

      return 1;
   },

   RWebCamTexImage2D__deps: ['RWebCamReady'],
   RWebCamTexImage2D: function(width, height) {
      if (!_RWebCamReady()) return 0;

      Module.ctx.texImage2D(Module.ctx.TEXTURE_2D, 0, Module.ctx.RGB, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, RWC.videoElement);

      if (width)  {{{ makeSetValue('width',  '0', 'RWC.videoElement.videoWidth',  'i32') }}};
      if (height) {{{ makeSetValue('height', '0', 'RWC.videoElement.videoHeight', 'i32') }}};
   },

   RWebCamTexSubImage2D__deps: ['RWebCamReady'],
   RWebCamTexSubImage2D: function(x, y) {
      if (!_RWebCamReady()) return 0;

      Module.ctx.texSubImage2D(Module.ctx.TEXTURE_2D, 0, x, y, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, RWC.videoElement);
   },

   RWebCamReady: function() {
      return (RWC.runMode == 2 && !RWC.videoElement.paused && RWC.videoElement.videoWidth != 0 && RWC.videoElement.videoHeight != 0) ? 1 : 0;
   },

   RWebCamFree: function() {
      RWC.videoElement.pause();
      RWC.videoElement = null;
      RWC.runMode = 0;
   }
};

autoAddDeps(LibraryRWebCam, '$RWC');
mergeInto(LibraryManager.library, LibraryRWebCam);
