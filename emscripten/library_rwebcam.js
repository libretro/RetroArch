//"use strict";

var LibraryRWebCam = {
   $RWC: {
      RETRO_CAMERA_BUFFER_OPENGL_TEXTURE: 0,
      RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER: 1,
      tmp: null,
      contexts: [],
      counter: 0
   },

   $RWebCamReady: function(c) {
      try {
         /* try to start video it was paused */
         if (RWC.contexts[c].videoElement.paused) RWC.contexts[c].videoElement.play();
      } catch (e) {}
      return RWC.contexts[c].cameraRunning && !RWC.contexts[c].videoElement.paused && RWC.contexts[c].videoElement.videoWidth != 0 && RWC.contexts[c].videoElement.videoHeight != 0;
   },

   $RWebCamInitBrowser__proxy: "sync",
   $RWebCamInitBrowser: function(width, height, glTex, rawFb, debug, proxied) {
      if (!navigator?.mediaDevices?.getUserMedia) return 0;

      var c = ++RWC.counter;
      RWC.contexts[c] = {width: width, height: height, glTex: glTex, rawFb: rawFb, debug: debug, proxied: proxied, idealWidth: width, idealHeight: height};
      return c;
   },

   RWebCamInit__deps: ["malloc", "$RWebCamInitBrowser"],
   RWebCamInit: function(caps, width, height, debug) {
      var glTex = Number(caps) & (1 << RWC.RETRO_CAMERA_BUFFER_OPENGL_TEXTURE);
      var rawFb = Number(caps) & (1 << RWC.RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER);
      var c = RWebCamInitBrowser(width, height, glTex, rawFb, debug, ENVIRONMENT_IS_PTHREAD);
      if (!c) return 0;

      if (debug) console.log("RWebCamInit", c);

      if (ENVIRONMENT_IS_PTHREAD) {
         RWC.contexts[c] = {};
         RWC.contexts[c].debug = debug;
         RWC.contexts[c].glTex = glTex;
         RWC.contexts[c].rawFb = rawFb;
      }

      /* for getting/storing texture id in GL mode */
      if (!RWC.tmp) RWC.tmp = _malloc(4);
      return c;
   },

   $RWebCamFreeBrowser__proxy: "sync",
   $RWebCamFreeBrowser: function(c) {
      RWC.contexts[c] = null;
   },

   RWebCamFree__deps: ["$RWebCamFreeBrowser", "RWebCamStop"],
   RWebCamFree: function(c) {
      if (RWC.contexts[c].debug) console.log("RWebCamFree", c);
      if (RWC.contexts[c].running) _RWebCamStop(c); /* need more checks in RA */
      RWebCamFreeBrowser(c);
      if (ENVIRONMENT_IS_PTHREAD) RWC.contexts[c] = null;
   },

   $RWebCamStartBrowser__proxy: "sync",
   $RWebCamStartBrowser: function(c) {
      RWC.contexts[c].videoElement = document.createElement("video");
      RWC.contexts[c].videoElement.classList.add("retroarchWebcamVideo");
      if (RWC.contexts[c].debug) document.body.appendChild(RWC.contexts[c].videoElement);

      if (RWC.contexts[c].rawFb || RWC.contexts[c].proxied) {
         RWC.contexts[c].rawFbCanvas = document.createElement("canvas");
         RWC.contexts[c].rawFbCanvas.classList.add("retroarchWebcamCanvas");
         if (RWC.contexts[c].debug) document.body.appendChild(RWC.contexts[c].rawFbCanvas);
      }

      var videoOpts = true;
      if (RWC.contexts[c].idealWidth && RWC.contexts[c].idealHeight) {
         /* save us some cropping/scaling, only honored by some browsers */
         videoOpts = {width: RWC.contexts[c].idealWidth, height: RWC.contexts[c].idealHeight, aspectRatio: RWC.contexts[c].idealWidth / RWC.contexts[c].idealHeight};
      }

      navigator.mediaDevices.getUserMedia({audio: false, video: videoOpts}).then(function(stream) {
         if (!RWC.contexts[c]?.running) {
            /* too late */
            for (var track of stream.getVideoTracks()) {
               track.stop();
            }
            return;
         }
         RWC.contexts[c].videoElement.autoplay = true;
         RWC.contexts[c].videoElement.srcObject = stream;
         RWC.contexts[c].cameraRunning = true;
      }).catch(function(e) {
         console.log("[rwebcam] webcam request failed", e);
      });

      RWC.contexts[c].running = true;
   },

   RWebCamStart__deps: ["glGenTextures", "glBindTexture", "glGetIntegerv", "glTexParameteri", "malloc", "$RWebCamStartBrowser"],
   RWebCamStart: function(c) {
      if (RWC.contexts[c].debug) console.log("RWebCamStart", c);
      if (RWC.contexts[c].running) return;
      if (RWC.contexts[c].glTex) {
         _glGenTextures(1, RWC.tmp);
         RWC.contexts[c].glTexId = {{{ makeGetValue('RWC.tmp', 0, 'i32') }}};
         if (RWC.contexts[c].glTexId !== 0) {
            /* save previous texture */
            _glGetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, RWC.tmp);
            var prev = {{{ makeGetValue('RWC.tmp', 0, 'i32') }}};
            _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, RWC.contexts[c].glTexId);
            /* NPOT textures in WebGL must have these filters and clamping settings */
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2601 /* GL_LINEAR */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2601 /* GL_LINEAR */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
            _glTexParameteri(0x0DE1 /* GL_TEXTURE_2D */, 0x2803 /*GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
            _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, prev);
            RWC.contexts[c].glFirstFrame = true;
         }
      }

      RWebCamStartBrowser(c);
      RWC.contexts[c].running = true;
      return 1;
   },

   $RWebCamStopBrowser__proxy: "sync",
   $RWebCamStopBrowser: function(c) {
      if (RWC.contexts[c].debug && RWC.contexts[c].rawFbCanvas) document.body.removeChild(RWC.contexts[c].rawFbCanvas);
      RWC.contexts[c].rawFbCanvasCtx = null;
      RWC.contexts[c].rawFbCanvas = null;
      RWC.contexts[c].videoElement.pause();
      if (RWC.contexts[c].cameraRunning) {
         for (var track of RWC.contexts[c].videoElement.srcObject.getVideoTracks()) {
            track.stop();
         }
      }
      if (RWC.contexts[c].debug) document.body.removeChild(RWC.contexts[c].videoElement);
      RWC.contexts[c].videoElement = null;
      RWC.contexts[c].running = false;
   },

   RWebCamStop__deps: ["free", "glDeleteTextures", "$RWebCamStopBrowser"],
   RWebCamStop: function(c) {
      if (RWC.contexts[c].debug) console.log("RWebCamStop", c);
      if (!RWC.contexts[c].running) return;

      if (RWC.contexts[c].glTexId) {
         _glDeleteTextures(1, RWC.contexts[c].glTexId);
      }

      if (RWC.contexts[c].rawBuffer) {
         _free(RWC.contexts[c].rawBuffer);
         RWC.contexts[c].rawBuffer = 0;
      }

      RWebCamStopBrowser(c);
      RWC.contexts[c].running = false;
   },

   $RWebCamCheckDimensions: function(c) {
      if (!RWC.contexts[c].width)  RWC.contexts[c].width  = RWC.contexts[c].videoElement.videoWidth;
      if (!RWC.contexts[c].height) RWC.contexts[c].height = RWC.contexts[c].videoElement.videoHeight;
   },

   $RWebCamStoreDimensions__proxy: "sync",
   $RWebCamStoreDimensions__deps: ["$RWebCamReady", "$RWebCamCheckDimensions"],
   $RWebCamStoreDimensions: function(c, ptr) {
      if (!RWebCamReady(c)) return 0;
      RWebCamCheckDimensions(c);
      {{{ makeSetValue('ptr', 0, 'RWC.contexts[c].width', 'i32') }}};
      {{{ makeSetValue('ptr', 4, 'RWC.contexts[c].height', 'i32') }}};
      return 1;
   },

   $RWebCamGetImageData__deps: ["$RWebCamReady"],
   $RWebCamGetImageData: function(c) {
      if (!RWebCamReady(c)) return 0;
      if (!RWC.contexts[c].rawFbCanvasCtx) {
         RWC.contexts[c].rawFbCanvas.width  = RWC.contexts[c].width;
         RWC.contexts[c].rawFbCanvas.height = RWC.contexts[c].height;
         RWC.contexts[c].rawFbCanvasCtx = RWC.contexts[c].rawFbCanvas.getContext("2d", {willReadFrequently: true});
      }
      /* crop to desired aspect ratio if necessary */
      var oldAspect = RWC.contexts[c].videoElement.videoWidth / RWC.contexts[c].videoElement.videoHeight;
      var newAspect = RWC.contexts[c].rawFbCanvas.width / RWC.contexts[c].rawFbCanvas.height;
      var width = RWC.contexts[c].videoElement.videoWidth;
      var height = RWC.contexts[c].videoElement.videoHeight;
      var offsetX = 0;
      var offsetY = 0;
      if (oldAspect > newAspect) {
         width = height * newAspect;
         offsetX = (RWC.contexts[c].videoElement.videoWidth - width) / 2;
      } else if (oldAspect < newAspect) {
         height = width / newAspect;
         offsetY = (RWC.contexts[c].videoElement.videoHeight - height) / 2;
      }
      RWC.contexts[c].rawFbCanvasCtx.drawImage(RWC.contexts[c].videoElement,
         offsetX, offsetY, width, height,
         0, 0, RWC.contexts[c].rawFbCanvas.width, RWC.contexts[c].rawFbCanvas.height);
      return RWC.contexts[c].rawFbCanvasCtx.getImageData(0, 0, RWC.contexts[c].rawFbCanvas.width, RWC.contexts[c].rawFbCanvas.height).data;
   },

   $RWebCamStoreImageData__proxy: "sync",
   $RWebCamStoreImageData__deps: ["$RWebCamGetImageData"],
   $RWebCamStoreImageData: function(c, ptr) {
      var data = RWebCamGetImageData(c);
      if (!data) return 0;
      HEAPU8.set(data, ptr);
      return 1;
   },

   RWebCamPoll__deps: ["$RWebCamReady", "glBindTexture", "glGetIntegerv", "malloc", "free", "$RWebCamStoreDimensions", "$RWebCamCheckDimensions", "$RWebCamStoreImageData"],
   RWebCamPoll: function(c, frame_raw_cb, frame_gl_cb) {
      if (RWC.contexts[c].debug) console.log("RWebCamPoll", c, ENVIRONMENT_IS_PTHREAD, RWC.contexts[c].rawBuffer, RWC.contexts[c].running);
      if (!RWC.contexts[c].running) return 0; /* need more checks in RA */
      var ret = 0;

      if ((RWC.contexts[c].rawFb && frame_raw_cb !== 0) || ENVIRONMENT_IS_PTHREAD) {
         if (!RWC.contexts[c].rawBuffer) {
            if (ENVIRONMENT_IS_PTHREAD) {
               /* pull dimensions into this thread */
               var dimensions = _malloc(8);
               var res = RWebCamStoreDimensions(c, dimensions); /* also checks ready(c) */
               if (!res) {
                  _free(dimensions);
                  return 0;
               }
               RWC.contexts[c].width  = {{{ makeGetValue('dimensions', 0, 'i32') }}};
               RWC.contexts[c].height = {{{ makeGetValue('dimensions', 4, 'i32') }}};
               _free(dimensions);
               RWC.contexts[c].length = RWC.contexts[c].width * RWC.contexts[c].height * 4;
            } else {
               if (!RWebCamReady(c)) return 0;
               RWebCamCheckDimensions(c);
            }
            RWC.contexts[c].rawBuffer = _malloc(RWC.contexts[c].width * RWC.contexts[c].height * 4 + 1);
         }
         /* store at +1 so we can read data as XRGB */
         if (!RWebCamStoreImageData(c, RWC.contexts[c].rawBuffer + 1)) return 0;
      }

      if (RWC.contexts[c].glTexId !== 0 && frame_gl_cb !== 0) {
         var imageSrcGL;
         if (ENVIRONMENT_IS_PTHREAD) {
            imageSrcGL = HEAPU8.subarray(RWC.contexts[c].rawBuffer + 1, RWC.contexts[c].rawBuffer + RWC.contexts[c].length + 1);
         } else {
            if (!RWebCamReady(c)) return 0;
            imageSrcGL = RWC.contexts[c].videoElement;
         }

         _glGetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, RWC.tmp);
         var prev = {{{ makeGetValue('RWC.tmp', 0, 'i32') }}};
         _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, RWC.contexts[c].glTexId);
         if (RWC.contexts[c].glFirstFrame) {
            if (ENVIRONMENT_IS_PTHREAD)
               Module.ctx.texImage2D(Module.ctx.TEXTURE_2D, 0, Module.ctx.RGBA, RWC.contexts[c].width, RWC.contexts[c].height, 0, Module.ctx.RGBA, Module.ctx.UNSIGNED_BYTE, imageSrcGL);
            else
               Module.ctx.texImage2D(Module.ctx.TEXTURE_2D, 0, Module.ctx.RGB, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, imageSrcGL);
            RWC.contexts[c].glFirstFrame = false;
         } else {
            if (ENVIRONMENT_IS_PTHREAD)
               Module.ctx.texSubImage2D(Module.ctx.TEXTURE_2D, 0, 0, 0, RWC.contexts[c].width, RWC.contexts[c].height, Module.ctx.RGBA, Module.ctx.UNSIGNED_BYTE, imageSrcGL);
            else
               Module.ctx.texSubImage2D(Module.ctx.TEXTURE_2D, 0, 0, 0, Module.ctx.RGB, Module.ctx.UNSIGNED_BYTE, imageSrcGL);
         }
         _glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, prev);

         {{{ makeDynCall('viip', 'frame_gl_cb') }}}(RWC.contexts[c].glTexId, 0x0DE1 /* GL_TEXTURE_2D */, 0);
         ret = 1;
      }

      if (RWC.contexts[c].rawFb && frame_raw_cb !== 0)
      {
         {{{ makeDynCall('vpiii', 'frame_raw_cb') }}}(RWC.contexts[c].rawBuffer, RWC.contexts[c].width, RWC.contexts[c].height, RWC.contexts[c].width * 4);
         ret = 1;
      }

      return ret;
   }
};

autoAddDeps(LibraryRWebCam, '$RWC');
addToLibrary(LibraryRWebCam);
