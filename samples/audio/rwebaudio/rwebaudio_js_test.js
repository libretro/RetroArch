/* os/emscripten/library_rwebaudio.js under Node, against a mock
 * AudioContext: the destination is widened only as far as its
 * maxChannelCount and only past stereo, passes channels through as
 * they are, and each queued buffer carries one plane per channel. A
 * stereo request leaves the destination as the browser made it. */
"use strict";
const fs = require("fs");
const vm = require("vm");
const path = require("path");

let failures = 0;
function check(cond, msg) { if (!cond) { console.log("      FAIL: " + msg); failures++; } }

let lib = null;
let maxChannelCount = 2;
let lastBuffer = null;
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
   addEventListener() {} removeEventListener() {} resume() {} close() {}
   createBuffer(ch, n, rate) { lastBuffer = new MockBuffer(ch, n, rate); return lastBuffer; }
   createBufferSource() { return { connect() {}, start() {} }; }
}

const sandbox = {
   window: { AudioContext: MockContext },
   performance: { now: () => 0 },
   HEAPF32,
   Math,
   addToLibrary: (l) => { lib = l; },
   autoAddDeps: () => {},
};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(path.join(__dirname, "../../../os/emscripten/library_rwebaudio.js"), "utf8"), sandbox);
check(lib !== null, "the library did not register");

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

console.log("rwebaudio (javascript):");
run("a stereo output, stereo asked for", 2, 2, 2);
run("a stereo output, 7.1 asked for: stereo, untouched", 2, 8, 2);
run("a 6-channel output, 7.1 asked for: 6", 6, 8, 6);
run("an 8-channel output, 5.1 asked for", 8, 6, 6);
run("a 4-channel output, 5.1 asked for: quad", 4, 6, 4);
if (failures) { console.log(failures + " failure(s)"); process.exit(1); }
console.log("rwebaudio (javascript): the destination widened as far as it goes, one plane per channel");
