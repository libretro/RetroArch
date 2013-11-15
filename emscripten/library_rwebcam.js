//"use strict";

var LibraryRWebCam = {
   $RWC: {
      RETRO_CAMERA_BUFFER_OPENGL_TEXTURE: 0,
      RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER: 1,
      tmp: null,

      contexts: [],
      counter: 0,

      ready: function(data) {
         return RWC.contexts[data].runMode == 2 && !RWC.contexts[data].videoElement.paused && RWC.contexts[data].videoElement.videoWidth != 0 && RWC.contexts[data].videoElement.videoHeight != 0;
      }
   },

   RWebCamInit__deps: ['malloc'],
   RWebCamInit: function(caps1, caps2, width, height) {
      if (!navigator) return 0;

      navigator.getMedia = navigator.getUserMedia ||
                           navigator.webkitGetUserMedia ||
                           navigator.mozGetUserMedia ||
                           navigator.msGetUserMedia;

      if (!navigator.getMedia) return 0;

      var c = ++RWC.counter;

      RWC.contexts[c] = [];
      RWC.contexts[c].videoElement = document.createElement("video");
      if (width !== 0 && height !== 0) {
         RWC.contexts[c].videoElement.width = width;
         RWC.contexts[c].videoElement.height = height;
      }
      RWC.contexts[c].runMode = 1;
      RWC.contexts[c].glTex = caps1 & (1 << RWC.RETRO_CAMERA_BUFFER_OPENGL_TEXTURE);
      RWC.contexts[c].rawFb = caps1 & (1 << RWC.RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER);

      navigator.getMedia({video: true, audio: false}, function(stream) {
         RWC.contexts[c].videoElement.autoplay = true;
         RWC.contexts[c].videoElement.src = URL.createObjectURL(stream);
         RWC.contexts[c].runMode = 2;
      }, function (err) {
         console.log("webcam request failed", err);
         RWC.runMode = 0;
      });

      // for getting/storing texture id in GL mode
      if (!RWC.tmp) RWC.tmp = _malloc(4);
      return c;
   },

   RWebCamFree: function(data) {
      RWC.contexts[data].videoElement.pause();
      URL.revokeObjectURL(RWC.contexts[data].videoElement.src);
      RWC.contexts[data].videoElement = null;
      RWC.contexts[data] = null;
   },

   RWebCamStart__deps: ['glGenTextures', 'glBindTexture', 'glGetIntegerv', 'glTexParameteri', 'malloc'],
   RWebCamStart: function(data) {
      var ret = 0;
      if (RWC.contexts[data].glTex) {
         _glGenTextures(1, RWC.tmp);
         RWC.contexts[data].glTexId = {{{ makeGetValue('RWC.tmp', '0', 'i32') }}};
         if (RWC.contexts[data].glTexId !== 0) {
            // save previous texture
            _glGetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, RWC.tmp);
            var prev = {{{ makeGetValue('RWC.tmp', '0', 'i32') }}};
            _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, RWC.contexts[data].glTexId);
            /* NPOT textures in WebGL must have these filters and clamping settings */
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2601 /* GL_LINEAR */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2601 /* GL_LINEAR */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2803 /*GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
            _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, prev);
            RWC.contexts[data].glFirstFrame = true;
            ret = 1;
         }
      }
      if (RWC.contexts[data].rawFb) {
         RWC.contexts[data].rawFbCanvas = document.createElement("canvas");
         ret = 1;
      }

      return ret;
   },

   RWebCamStop__deps: ['glDeleteTextures', 'free'],
   RWebCamStop: function(data) {
      if (RWC.contexts[data].glTexId) {
         _glDeleteTextures(1, RWC.contexts[data].glTexId);
      }

      if (RWC.contexts[data].rawFbCanvas) {
         if (RWC.contexts[data].rawBuffer) {
            _free(RWC.contexts[data].rawBuffer);
            RWC.contexts[data].rawBuffer = 0;
            RWC.contexts[data].rawFbCanvasCtx = null
         }
         RWC.contexts[data].rawFbCanvas = null;
      }
   },

   RWebCamPoll__deps: ['glBindTexture', 'glGetIntegerv'],
   RWebCamPoll: function(data, frame_raw_cb, frame_gl_cb) {
      if (!RWC.ready(data)) return 0;
      var ret = 0;

      if (RWC.contexts[data].glTexId !== 0 && frame_gl_cb !== 0) {         
         _glGetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, RWC.tmp);
         var prev = {{{ makeGetValue('RWC.tmp', '0', 'i32') }}};
         _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, RWC.contexts[data].glTexId);
         if (RWC.contexts[data].glFirstFrame) {
            Module.ctx.texImage2D(Module.ctx.TEXTURE_2D, 0, Module.ctx.RGB, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, RWC.contexts[data].videoElement);
            RWC.contexts[data].glFirstFrame = false;
         } else {
            Module.ctx.texSubImage2D(Module.ctx.TEXTURE_2D, 0, 0, 0, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, RWC.contexts[data].videoElement);
         }
         _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, prev);
         Runtime.dynCall('viii', frame_gl_cb, [RWC.contexts[data].glTexId, 0x0DE1 /* GL_TEXTURE_2D */, 0]);

         ret = 1;
      }

      if (RWC.contexts[data].rawFbCanvas && frame_raw_cb !== 0)
      {
         if (!RWC.contexts[data].rawFbCanvasCtx) {
            RWC.contexts[data].rawFbCanvas.width = RWC.contexts[data].videoElement.videoWidth;
            RWC.contexts[data].rawFbCanvas.height = RWC.contexts[data].videoElement.videoHeight;
            RWC.contexts[data].rawFbCanvasCtx = RWC.contexts[data].rawFbCanvas.getContext("2d");
            RWC.contexts[data].rawBuffer = _malloc(RWC.contexts[data].videoElement.videoWidth * RWC.contexts[data].videoElement.videoHeight * 4);
         }
         RWC.contexts[data].rawFbCanvasCtx.drawImage(RWC.contexts[data].videoElement, 0, 0, RWC.contexts[data].rawFbCanvas.width, RWC.contexts[data].rawFbCanvas.height);
         var image = RWC.contexts[data].rawFbCanvasCtx.getImageData(0, 0, RWC.contexts[data].videoElement.videoWidth, RWC.contexts[data].videoElement.videoHeight);
         Module.HEAPU8.set(image.data, RWC.contexts[data].rawBuffer);
         Runtime.dynCall('viiii', frame_raw_cb, [RWC.contexts[data].rawBuffer, RWC.contexts[data].videoElement.videoWidth, RWC.contexts[data].videoElement.videoHeight, RWC.contexts[data].videoElement.videoWidth * 4]);

         ret = 1;
      }

      return ret;
   }
};

autoAddDeps(LibraryRWebCam, '$RWC');
mergeInto(LibraryManager.library, LibraryRWebCam);
