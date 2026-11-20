/* Exercise the actual C -> JS Promise -> Asyncify -> C return path. */
#include <assert.h>
#include <stdio.h>
#include <emscripten.h>

extern int RWebAudioWaitRoom(unsigned ms);

int main(void)
{
   EM_ASM({
      RWA.context = {};
      RWA.context.sampleRate = 48000;
      RWA.context.state = 'running';
      RWA.contextRunning = true;
      RWA.virtualBufferFrames = 3072;
      RWA.latency = 64;
      RWA.currentTimeDiff = performance.now() / 1000;
      RWA.endTime = 0;
   });
   assert(RWebAudioWaitRoom(1000) == 1);
   EM_ASM({
      RWA.endTime = 100000;
      setTimeout(function() {
         if (RWA.wake) {
            var wake = RWA.wake;
            RWA.wake = null;
            wake(1);
         }
      }, 10);
   });
   assert(RWebAudioWaitRoom(1000) == 1);
   assert(RWebAudioWaitRoom(10) == 0);
   EM_ASM({ if (RWA.wake !== null) throw new Error('stale audio wake'); });
   puts("WebAudio wasm async bridge: immediate, wake and watchdog PASS");
   return 0;
}
