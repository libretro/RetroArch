//"use strict";

var LibraryRWebAudio = {
   $RWA: {
      /* add 10 ms of silence on start, seems to prevent underrun on start/unpausing (terrible crackling in firefox) */
      MIN_START_OFFSET_SEC: 0.01,
      /* firefox and safari need more latency (transparent to audio driver) */
      EXTRA_LATENCY_SEC_NONCHROME: 0.01,
      EXTRA_LATENCY_SEC_CHROME: 0,
      PLATFORM_EMSCRIPTEN_BROWSER_CHROMIUM: 1,
      context: null,
      contextRunning: false,
      channels: 2,
      nonblock: false,
      endTime: 0,
      latency: 0,
      virtualBufferFrames: 0,
      currentTimeDiff: 0,
      extraLatencySec: 0
   },

   /* AudioContext.currentTime can be inaccurate: https://bugzilla.mozilla.org/show_bug.cgi?id=901247 */
   $RWebAudioGetCurrentTime: function() {
      return performance.now() / 1000 - RWA.currentTimeDiff;
   },

   $RWebAudioStateChangeCB: function() {
      RWA.contextRunning = RWA.context.state == "running";
   },

   RWebAudioResumeCtx: function() {
      if (!RWA.contextRunning) RWA.context.resume();
      return RWA.contextRunning;
   },

   RWebAudioInit__deps: ["$RWebAudioStateChangeCB", "platform_emscripten_get_browser"],
   RWebAudioInit: function(latency, channels) {
      var ac = window.AudioContext || window.webkitAudioContext;
      if (!ac) return 0;

      RWA.context = new ac();
      /* Wider than stereo where the output takes it: 8, 6 or 4 channels
       * passed through as they are, the device's own order by count. */
      if (channels > 2) {
         var dest = RWA.context.destination;
         while (channels > 2 && !(dest.maxChannelCount >= channels))
            channels = channels > 6 ? 6 : channels > 4 ? 4 : 2;
         if (channels > 2) {
            try {
               dest.channelCount = channels;
               dest.channelCountMode = "explicit";
               dest.channelInterpretation = "discrete";
            } catch (e) {
               channels = 2;
            }
         }
      }
      RWA.channels = channels > 2 ? channels : 2;
      RWA.currentTimeDiff = performance.now() / 1000 - RWA.context.currentTime;
      RWA.nonblock = false;
      RWA.endTime = 0;
      RWA.latency = latency;
      RWA.virtualBufferFrames = Math.round(RWA.latency * RWA.context.sampleRate / 1000);
      RWA.context.addEventListener("statechange", RWebAudioStateChangeCB);
      RWebAudioStateChangeCB();
      RWA.extraLatencySec = (_platform_emscripten_get_browser() == RWA.PLATFORM_EMSCRIPTEN_BROWSER_CHROMIUM) ? RWA.EXTRA_LATENCY_SEC_CHROME : RWA.EXTRA_LATENCY_SEC_NONCHROME;

      return RWA.channels;
   },

   RWebAudioSampleRate: function() {
      return RWA.context.sampleRate;
   },

   RWebAudioQueueBuffer__deps: ["$RWebAudioGetCurrentTime", "RWebAudioResumeCtx", "RWebAudioWriteAvailFrames"],
   RWebAudioQueueBuffer: function(num_frames, planes, plane_frames) {
      if (RWA.nonblock && _RWebAudioWriteAvailFrames() < num_frames) return 0;
      if (!_RWebAudioResumeCtx()) return 0;

      var buffer = RWA.context.createBuffer(RWA.channels, num_frames, RWA.context.sampleRate);
      for (var c = 0; c < RWA.channels; c++) {
         var p = (planes >> 2) + c * plane_frames;
         buffer.getChannelData(c).set(HEAPF32.subarray(p, p + num_frames));
      }
      var bufferSource = RWA.context.createBufferSource();
      bufferSource.buffer = buffer;
      bufferSource.connect(RWA.context.destination);

      var currentTime = RWebAudioGetCurrentTime();
      /* when empty, start rounded up to nearest 1 ms, add MIN_START_OFFSET_SEC */
      var startTime = RWA.endTime > currentTime ? RWA.endTime : Math.ceil(currentTime * 1000) / 1000 + RWA.MIN_START_OFFSET_SEC;
      RWA.endTime = startTime + buffer.duration;
      bufferSource.start(startTime + RWA.extraLatencySec);

      return num_frames;
   },

   RWebAudioStop: function() {
      return true;
   },

   RWebAudioStart: function() {
      return true;
   },

   RWebAudioSetNonblockState: function(state) {
      RWA.nonblock = state;
   },

   RWebAudioFree__deps: ["$RWebAudioStateChangeCB"],
   RWebAudioFree: function() {
      RWA.context.removeEventListener("statechange", RWebAudioStateChangeCB);
      RWA.context.close();
      RWA.contextRunning = false;
   },

   RWebAudioBufferSizeFrames: function() {
      return RWA.virtualBufferFrames;
   },

   RWebAudioWriteAvailFrames__deps: ["$RWebAudioGetCurrentTime"],
   RWebAudioWriteAvailFrames: function() {
      var avail = Math.round(((RWA.latency / 1000) - RWA.endTime + RWebAudioGetCurrentTime()) * RWA.context.sampleRate);
      if (avail <= 0) return 0;
      if (avail >= RWA.virtualBufferFrames) return RWA.virtualBufferFrames;
      return avail;
   },

   RWebAudioRecalibrateTime: function() {
      if (RWA.contextRunning) RWA.currentTimeDiff = performance.now() / 1000 - RWA.context.currentTime;
   }
};

autoAddDeps(LibraryRWebAudio, '$RWA');
addToLibrary(LibraryRWebAudio);
