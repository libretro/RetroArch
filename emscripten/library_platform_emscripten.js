//"use strict";

var LibraryPlatformEmscripten = {
   $RPE: {
      powerStateChange: function(e) {
         _platform_emscripten_update_power_state(true, Number.isFinite(e.target.dischargingTime) ? e.target.dischargingTime : 0x7FFFFFFF, e.target.level, e.target.charging);
      },

      updateMemoryUsage: function() {
         // unfortunately this will be innacurate in threaded (worker) builds
         var used = BigInt(performance.memory.usedJSHeapSize || 0);
         var limit = BigInt(performance.memory.jsHeapSizeLimit || 0);
         // emscripten currently only supports passing 32 bit ints, so pack it
         _platform_emscripten_update_memory_usage(Number(used & 0xFFFFFFFFn), Number(used >> 32n), Number(limit & 0xFFFFFFFFn), Number(limit >> 32n));
         setTimeout(RPE.updateMemoryUsage, 5000);
      },
      command_queue: [],
      command_reply_queue: []
   },

   PlatformEmscriptenWatchCanvasSizeAndDpr__deps: ["platform_emscripten_update_canvas_dimensions"],
   PlatformEmscriptenWatchCanvasSizeAndDpr: function(dpr) {
      if (RPE.observer) {
         RPE.observer.unobserve(Module.canvas);
         RPE.observer.observe(Module.canvas);
         return;
      }
      RPE.observer = new ResizeObserver(function(e) {
         var width, height;
         var entry = e.find(i => i.target == Module.canvas);
         if (!entry) return;
         if (entry.devicePixelContentBoxSize) {
            width = entry.devicePixelContentBoxSize[0].inlineSize;
            height = entry.devicePixelContentBoxSize[0].blockSize;
         } else {
            width = Math.round(entry.contentRect.width * window.devicePixelRatio);
            height = Math.round(entry.contentRect.height * window.devicePixelRatio);
         }
         // doubles are too big to pass as an argument to exported functions
         {{{ makeSetValue("dpr", "0", "window.devicePixelRatio", "double") }}};
         _platform_emscripten_update_canvas_dimensions(width, height, dpr);
      });
      RPE.observer.observe(Module.canvas);
      window.addEventListener("resize", function() {
         RPE.observer.unobserve(Module.canvas);
         RPE.observer.observe(Module.canvas);
      }, false);
   },

   PlatformEmscriptenWatchWindowVisibility__deps: ["platform_emscripten_update_window_hidden"],
   PlatformEmscriptenWatchWindowVisibility: function() {
      document.addEventListener("visibilitychange", function() {
         _platform_emscripten_update_window_hidden(document.visibilityState == "hidden");
      }, false);
   },

   PlatformEmscriptenPowerStateInit__deps: ["platform_emscripten_update_power_state"],
   PlatformEmscriptenPowerStateInit: function() {
      if (!navigator.getBattery) return;
      navigator.getBattery().then(function(battery) {
         battery.addEventListener("chargingchange", RPE.powerStateChange);
         battery.addEventListener("levelchange", RPE.powerStateChange);
         RPE.powerStateChange({target: battery});
      });
   },

   PlatformEmscriptenMemoryUsageInit__deps: ["platform_emscripten_update_memory_usage"],
   PlatformEmscriptenMemoryUsageInit: function() {
      if (!performance.memory) return;
      RPE.updateMemoryUsage();
   },

   $EmscriptenSendCommand__deps: ["platform_emscripten_command_raise_flag"],
   $EmscriptenSendCommand: function(str) {
      RPE.command_queue.push(str);
      _platform_emscripten_command_raise_flag();
   },

   $EmscriptenReceiveCommandReply: function() {
      return RPE.command_reply_queue.shift();
   }
};

autoAddDeps(LibraryPlatformEmscripten, '$RPE');
mergeInto(LibraryManager.library, LibraryPlatformEmscripten);
