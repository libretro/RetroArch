//"use strict";

var LibraryRWebAudio = {
   $RWA: {
      /* add 10 ms of silence on start, seems to prevent underrun on start/unpausing (terrible crackling in firefox) */
      MIN_START_OFFSET_SEC: 0.01,
      /* firefox needs more latency (transparent to audio driver) */
      EXTRA_LATENCY_FIREFOX_SEC: 0.01,
      PLATFORM_EMSCRIPTEN_BROWSER_FIREFOX: 2,
      context: null,
      contextRunning: false,
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
   RWebAudioInit: function(latency) {
      var ac = window.AudioContext || window.webkitAudioContext;
      if (!ac) return 0;

      RWA.context = new ac();
      RWA.currentTimeDiff = performance.now() / 1000 - RWA.context.currentTime;
      RWA.nonblock = false;
      RWA.endTime = 0;
      RWA.latency = latency;
      RWA.virtualBufferFrames = Math.round(RWA.latency * RWA.context.sampleRate / 1000);
      RWA.context.addEventListener("statechange", RWebAudioStateChangeCB);
      RWebAudioStateChangeCB();
      RWA.extraLatencySec = (_platform_emscripten_get_browser() == RWA.PLATFORM_EMSCRIPTEN_BROWSER_FIREFOX) ? RWA.EXTRA_LATENCY_FIREFOX_SEC : 0;

      return 1;
   },

   RWebAudioSampleRate: function() {
      return RWA.context.sampleRate;
   },

   RWebAudioQueueBuffer__deps: ["$RWebAudioGetCurrentTime", "RWebAudioResumeCtx", "RWebAudioWriteAvailFrames"],
   RWebAudioQueueBuffer: function(num_frames, left, right) {
      if (RWA.nonblock && _RWebAudioWriteAvailFrames() < num_frames) return 0;
      if (!_RWebAudioResumeCtx()) return 0;

      var buffer = RWA.context.createBuffer(2, num_frames, RWA.context.sampleRate);
      buffer.getChannelData(0).set(HEAPF32.subarray(left  >> 2, (left  >> 2) + num_frames));
      buffer.getChannelData(1).set(HEAPF32.subarray(right >> 2, (right >> 2) + num_frames));
      var bufferSource = RWA.context.createBufferSource();
      bufferSource.buffer = buffer;
      bufferSource.connect(RWA.context.destination);

      var currentTime = RWebAudioGetCurrentTime();
      var startTime = RWA.endTime > currentTime ? RWA.endTime : currentTime + RWA.MIN_START_OFFSET_SEC;
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
