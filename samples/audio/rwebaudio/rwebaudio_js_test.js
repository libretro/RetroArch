/* os/emscripten/library_rwebaudio.js under Node, against a mock
 * AudioContext: the destination is widened only as far as its
 * maxChannelCount and only past stereo, passes channels through as
 * they are, and each queued buffer carries one plane per channel. A
 * stereo request leaves the destination as the browser made it.
 *
 * And a wait for room - what a blocking write on an asyncify build
 * awaits - ends on the context's own clock: a queued source finishing,
 * or the context starting or stopping, and on its watchdog only when
 * neither comes. */
"use strict";
const fs = require("fs");
const vm = require("vm");
const path = require("path");

let failures = 0;
function check(cond, msg) { if (!cond) { console.log("      FAIL: " + msg); failures++; } }

let lib = null;
let maxChannelCount = 2;
let lastBuffer = null;
let sources = [];
let stateListener = null;
let nowMs = 0;
const HEAPF32 = new Float32Array(4096);

class MockDestination {
   constructor() { this.maxChannelCount = maxChannelCount; this.channelCount = 2;
      this.channelCountMode = "explicit"; this.channelInterpretation = "speakers"; this.touched = false; }
}
class MockBuffer {
   constructor(ch, n, rate) { this.numberOfChannels = ch; this.length = n; this.duration = n / rate;
      this.planes = []; for (let i = 0; i < ch; i++) this.planes.push(new Float32Array(n)); }
   getChannelData(c) { return this.planes[c]; }
}
class MockContext {
   constructor() { this.sampleRate = 48000; this.currentTime = 0; this.state = "running";
      this.destination = new MockDestination(); }
   addEventListener(type, cb) { if (type === "statechange") stateListener = cb; }
   removeEventListener() {} resume() {} close() {}
   createBuffer(ch, n, rate) { lastBuffer = new MockBuffer(ch, n, rate); return lastBuffer; }
   createBufferSource() { const src = { connect() {}, start() {}, onended: null }; sources.push(src); return src; }
}

const sandbox = {
   window: { AudioContext: MockContext },
   performance: { now: () => nowMs },
   setTimeout,
   clearTimeout,
   HEAPF32,
   Asyncify: { handleAsync: (fn) => fn() },
   Math,
   addToLibrary: (l) => { lib = l; },
   autoAddDeps: () => {},
};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(path.join(__dirname, "../../../os/emscripten/library_rwebaudio.js"), "utf8"), sandbox);
check(lib !== null, "the library did not register");
check(lib.RWebAudioWaitRoom__async === true, "Emscripten async metadata must be boolean");
check(lib.RWebAudioWaitRoom__deps.includes("$Asyncify"), "the wait lacks its Asyncify dependency");

/* what emscripten makes of it: $X is X, the rest _X */
for (const k of Object.keys(lib)) {
   if (k.endsWith("__deps")) continue;
   if (k[0] === "$") sandbox[k.slice(1)] = lib[k];
   else sandbox["_" + k] = lib[k];
}
sandbox._platform_emscripten_get_browser = () => 1;

function run(what, max, asked, expectCh) {
   console.log("   " + what);
   maxChannelCount = max;
   const got = sandbox._RWebAudioInit(64, asked);
   const dest = sandbox.RWA.context.destination;
   check(got === expectCh, `init granted ${got} channels, ${expectCh} expected`);
   if (expectCh > 2)
      check(dest.channelCount === expectCh && dest.channelInterpretation === "discrete",
            `destination ${dest.channelCount} channels, ${dest.channelInterpretation}`);
   else
      check(dest.channelCount === 2 && dest.channelInterpretation === "speakers",
            "a stereo destination was changed");
   /* planes of 16 frames, plane c frame f = c * 100 + f, at byte 64 */
   for (let c = 0; c < expectCh; c++)
      for (let f = 0; f < 16; f++)
         HEAPF32[16 + c * 16 + f] = c * 100 + f;
   check(sandbox._RWebAudioQueueBuffer(10, 64, 16) === 10, "the queue refused the buffer");
   check(lastBuffer && lastBuffer.numberOfChannels === expectCh,
         `the buffer has ${lastBuffer && lastBuffer.numberOfChannels} channels`);
   for (let c = 0; lastBuffer && c < expectCh; c++)
      for (let f = 0; f < 10; f++)
         if (lastBuffer.planes[c][f] !== c * 100 + f) {
            check(false, `plane ${c} frame ${f} is ${lastBuffer.planes[c][f]}`);
            return;
         }
   sandbox._RWebAudioFree();
}

/* Whether a promise has settled, after everything already queued ran. */
async function settled(p) {
   let done = false;
   p.then(() => { done = true; });
   await new Promise((r) => setImmediate(r));
   return done;
}

async function run_wait() {
   console.log("   a wait for room ends on the context's clock");
   maxChannelCount = 2;
   nowMs   = 0;
   sources = [];
   sandbox._RWebAudioInit(64, 2);

   let p = sandbox._RWebAudioWaitRoom(1000);
   check(await settled(p) && (await p) === 1, "a wait with room did not end at once");

   /* 10 ms buffers until the 64 ms of latency is queued */
   while (sandbox._RWebAudioWriteAvailFrames() > 0)
      sandbox._RWebAudioQueueBuffer(480, 64, 480);
   check(sources.length > 0 && sources.every((s) => typeof s.onended === "function"),
         "a queued source does not wake a wait when it ends");

   p = sandbox._RWebAudioWaitRoom(1000);
   check(!(await settled(p)), "a wait with no room ended before anything happened");
   nowMs += 20;
   if (sources[0].onended)
      sources[0].onended();
   check(await settled(p) && (await p) === 1, "a source that ended did not end the wait");

   nowMs = 0;
   p = sandbox._RWebAudioWaitRoom(1000);
   check(!(await settled(p)), "a wait with no room ended before anything happened");
   stateListener();
   check(await settled(p) && (await p) === 1, "the context starting did not end the wait");

   p = sandbox._RWebAudioWaitRoom(20);
   check((await p) === 0, "a wait nothing ended was not its watchdog's");
   check(sandbox.RWA.wake === null, "a wait that ran out left its resolver behind");
   sandbox._RWebAudioFree();
}

(async () => {
   console.log("rwebaudio (javascript):");
   run("a stereo output, stereo asked for", 2, 2, 2);
   run("a stereo output, 7.1 asked for: stereo, untouched", 2, 8, 2);
   run("a 6-channel output, 7.1 asked for: 6", 6, 8, 6);
   run("an 8-channel output, 5.1 asked for", 8, 6, 6);
   run("a 4-channel output, 5.1 asked for: quad", 4, 6, 4);
   await run_wait();
   if (failures) { console.log(failures + " failure(s)"); process.exit(1); }
   console.log("rwebaudio (javascript): the destination widened as far as it goes, one plane per channel; a wait for room ends on the context's clock");
})();
