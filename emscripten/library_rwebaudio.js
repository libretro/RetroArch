//"use strict";

var LibraryRWebAudio = {
   $RA__deps: ['$Browser', 'usleep'],
   $RA: {
      BUFFER_SIZE: 256,

      context: null,
      buffers: [],
      numBuffers: 0,
      bufIndex: 0,
      bufOffset: 0,
      startTime: 0,
      nonblock: false,
      currentTimeWorkaround: false,

      setStartTime: function() {
         if (RA.context.currentTime) {
            RA.startTime = window['performance']['now']() - RA.context.currentTime * 1000;
            var time1 = RA.context.currentTime;
            _usleep(50);
            if (time1 === RA.context.currentTime) {
               RA.currentTimeWorkaround = true;
               if (RA.startTime === 0) throw 'startTime is 0';
            }
            Module["resumeMainLoop"]();
         } else window['setTimeout'](RA.setStartTime, 0);
      },

      getCurrentPerfTime: function() {
         if (!RA.currentTimeWorkaround) return RA.context.currentTime;
         else if (RA.startTime) return (window['performance']['now']() - RA.startTime) / 1000;
         else throw 'getCurrentPerfTime() called before start time set';
      },

      process: function(queueBuffers) {
         var currentTime = RA.getCurrentPerfTime();
         for (var i = 0; i < RA.bufIndex; i++) {
            if (RA.buffers[i].endTime < currentTime) {
               var buf = RA.buffers.splice(i, 1);
               RA.buffers[RA.numBuffers - 1] = buf[0];
               i--;
               RA.bufIndex--;
            }
         }
      },

      fillBuffer: function(buf, samples) {
         var count = 0;
         var leftBuffer = RA.buffers[RA.bufIndex].getChannelData(0);
         var rightBuffer = RA.buffers[RA.bufIndex].getChannelData(1);
         while (samples && RA.bufOffset !== RA.BUFFER_SIZE) {
            leftBuffer[RA.bufOffset] = {{{ makeGetValue('buf', 'count * 8', 'float') }}};
            rightBuffer[RA.bufOffset] = {{{ makeGetValue('buf', 'count * 8 + 4', 'float') }}};
            RA.bufOffset++;
            count++;
            samples--;
         }

         return count;
      },

      queueAudio: function() {
         var index = RA.bufIndex;

         var startTime;
         if (RA.bufIndex) startTime = RA.buffers[RA.bufIndex - 1].endTime;
         else startTime = RA.context.currentTime;
         RA.buffers[index].endTime = startTime + RA.buffers[index].duration;

         var bufferSource = RA.context.createBufferSource();
         bufferSource.buffer = RA.buffers[index];
         bufferSource.connect(RA.context.destination);
         bufferSource.start(startTime);

         RA.bufIndex++;
         RA.bufOffset = 0;
      },

      block: function() {
         do {
            RA.process();
         } while (RA.bufIndex === RA.numBuffers - 1);
      }
   },

   RWebAudioInit: function(latency) {
      var ac = window['AudioContext'] || window['webkitAudioContext'];

      if (!ac) return 0;

      RA.context = new ac();

      RA.numBuffers = ((latency * RA.context.sampleRate) / (1000 * RA.BUFFER_SIZE))|0;
      if (RA.numBuffers < 2) RA.numBuffers = 2;

      for (var i = 0; i < RA.numBuffers; i++) RA.buffers[i] = RA.context.createBuffer(2, RA.BUFFER_SIZE, RA.context.sampleRate);

      RA.nonblock = false;
      RA.startTime = 0;
      // chrome hack to get currentTime running
      RA.context.createGain();
      window['setTimeout'](RA.setStartTime, 0);
      Module["pauseMainLoop"]();
      return 1;
   },
   
   RWebAudioSampleRate: function() {
      return RA.context.sampleRate;
   },

   RWebAudioWrite: function (buf, size) {
      RA.process();
      var samples = size / 8;
      var count = 0;

      while (samples) {
         var fill = RA.fillBuffer(buf, samples);
         samples -= fill;
         count += fill;
         buf += fill * 8;

         if (RA.bufOffset === RA.BUFFER_SIZE) {
            if (RA.bufIndex === RA.numBuffers - 1) {
               if (RA.nonblock) break;
               else RA.block();
            }
            RA.queueAudio();
         }
      }

      return count * 8;
   },

   RWebAudioStop: function() {
      RA.bufIndex = 0;
      RA.bufOffset = 0;
      return true;
   },

   RWebAudioStart: function() {
      return true;
   },

   RWebAudioSetNonblockState: function(state) {
      RA.nonblock = state;
   },

   RWebAudioFree: function() {
      RA.bufIndex = 0;
      RA.bufOffset = 0;
      return;
   },

   RWebAudioBufferSize: function() {
      return RA.numBuffers * RA.BUFFER_SIZE + RA.BUFFER_SIZE;
   },

   RWebAudioWriteAvail: function() {
      RA.process();
      return ((RA.numBuffers - RA.bufIndex) * RA.BUFFER_SIZE - RA.bufOffset) * 8;
   }
};

autoAddDeps(LibraryRWebAudio, '$RA');
mergeInto(LibraryManager.library, LibraryRWebAudio);
