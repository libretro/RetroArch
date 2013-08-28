//"use strict";

var LibraryRWebAudio = {
  $RA__deps: ['$Browser'],
  $RA: {
    SCRIPTNODE_BUFFER: 1024,

    context: null,
    leftBuffer: null,
    rightBuffer: null,
    blank: null,
    scriptNode: null,
    bufferNode: null,
    start: 0,
    end: 0,
    size: 0,
    lastWrite: 0,
    nonblock: false,
    
    npot: function(n) {
      n--;
      n |= n >> 1;
      n |= n >> 2;
      n |= n >> 4;
      n |= n >> 8;
      n |= n >> 16;
      n++;
      return n;
    },

    process: function(e) {
      var left = e.outputBuffer.getChannelData(0);
      var right = e.outputBuffer.getChannelData(1);
      var samples1 = RA.size;
      var samples2 = 0;
      samples1 = e.outputBuffer.length > samples1 ? samples1 : e.outputBuffer.length;

      if (samples1 + RA.start > RA.leftBuffer.length) {
        samples2 = samples1 + RA.start - RA.leftBuffer.length;
        samples1 = samples1 - samples2;
      }

      var remaining = e.outputBuffer.length - (samples1 + samples2);

      if (samples1) {
        left.set(RA.leftBuffer.subarray(RA.start, RA.start + samples1), 0);
        right.set(RA.rightBuffer.subarray(RA.start, RA.start + samples1), 0);
      }

      if (samples2) {
        left.set(RA.leftBuffer.subarray(0, samples2), samples1);
        right.set(RA.rightBuffer.subarray(0, samples2), samples1);
      }

      /*if (remaining) {
        left.set(RA.blank.subarray(0, remaining), samples1 + samples2);
        right.set(RA.blank.subarray(0, remaining), samples1 + samples2);
      }*/

      RA.start = (RA.start + samples1 + samples2) % RA.leftBuffer.length;
      RA.size -= samples1 + samples2;
    }
  },
  
  RWebAudioSampleRate: function() {
    return RA.context.sampleRate;
  },

  RWebAudioInit: function(latency) {
    var ac = window['AudioContext'] || window['webkitAudioContext'];
    var bufferSize;

    if (!ac) return 0;

    RA.context = new ac();
    // account for script processor overhead
    latency -= 32;
    // because we have to guess on how many samples the core will send when
    // returning early, we double the buffer size to account for times when it
    // sends more than we expect it to without losing samples
    bufferSize = RA.npot(RA.context.sampleRate * latency / 1000) * 2;
    RA.leftBuffer = new Float32Array(bufferSize);
    RA.rightBuffer = new Float32Array(bufferSize);
    RA.blank = new Float32Array(RA.SCRIPTNODE_BUFFER);
    RA.bufferNode = RA.context.createBufferSource();
    RA.bufferNode.buffer = RA.context.createBuffer(2, RA.SCRIPTNODE_BUFFER, RA.context.sampleRate);
    RA.bufferNode.loop = true;
    RA.scriptNode = RA.context.createScriptProcessor(RA.SCRIPTNODE_BUFFER, 2, 2);
    RA.scriptNode.onaudioprocess = RA.process;
    RA.bufferNode.connect(RA.scriptNode);
    RA.scriptNode.connect(RA.context.destination);
    RA.bufferNode.start(0);
    RA.start = RA.end = RA.size = 0;
    RA.nonblock = false;
    return 1;
  },

  RWebAudioWrite: function (buf, size) {
    var samples = size / 8;
    var free = RA.leftBuffer.length - RA.size;
    if (free < samples)
      RA.start = (RA.start + free) % RA.leftBuffer.length;

    for (var i = 0; i < samples; i++) {
      RA.leftBuffer[RA.end] = {{{ makeGetValue('buf', 'i * 8', 'float') }}};
      RA.rightBuffer[RA.end] = {{{ makeGetValue('buf', 'i * 8 + 4', 'float') }}};
      RA.end = (RA.end + 1) % RA.leftBuffer.length;
    }

    RA.lastWrite = size;
    RA.size += samples;
    return size;
  },

  RWebAudioStop: function() {
    RA.scriptNode.onaudioprocess = null;
    return true;
  },

  RWebAudioStart: function() {
    RA.scriptNode.onaudioprocess = RA.process;
    return true;
  },

  RWebAudioSetNonblockState: function(state) {
    RA.nonblock = state;
  },

  RWebAudioFree: function() {
    RA.scriptNode.onaudioprocess = null;
    RA.start = RA.end = RA.size = RA.lastWrite = 0;
    return;
  },

  RWebAudioWriteAvail: function() {
    var free  = (RA.leftBuffer.length / 2) - RA.size;
    // 4 byte samples, 2 channels
    free *= 8;

    if (free < 0)
      return 0;
    else
      return free;
  },

  RWebAudioBufferSize: function() {
    return RA.leftBuffer.length / 2;
  },
  
  RWebAudioEnoughSpace__deps: ['RWebAudioWriteAvail'],
  RWebAudioEnoughSpace: function() {
    var guess = RA.lastWrite;
    var available = _RWebAudioWriteAvail();
    if (RA.nonblock) return true;
    if (!guess) return true;
    return (guess < available) ? 1 : 0;
  }
};

autoAddDeps(LibraryRWebAudio, '$RA');
mergeInto(LibraryManager.library, LibraryRWebAudio);
