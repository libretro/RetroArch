//"use strict";

var LibraryPlatformEmscripten = {
   $RPE: {
      powerState: {
         supported: false,
         dischargeTime: 0,
         level: 0,
         charging: false
      },
      powerStateChange: function(e) {
         RPE.powerState.dischargeTime = Number.isFinite(e.target.dischargingTime) ? e.target.dischargingTime : 0x7FFFFFFF;
         RPE.powerState.level = e.target.level;
         RPE.powerState.charging = e.target.charging;
      }
   },

   PlatformEmscriptenWatchCanvasSize: function() {
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
         Module.setCanvasSize(width, height);
         Module.print("Setting real canvas size: " + width + " x " + height);
      });
      RPE.observer.observe(Module.canvas);
      window.addEventListener("resize", function(e) {
         RPE.observer.unobserve(Module.canvas);
         RPE.observer.observe(Module.canvas);
      }, false);
   },

   PlatformEmscriptenPowerStateInit: function() {
      if (!navigator.getBattery) return;
      navigator.getBattery().then(function(battery) {
         battery.addEventListener("chargingchange", RPE.powerStateChange);
         battery.addEventListener("levelchange", RPE.powerStateChange);
         RPE.powerStateChange({target: battery});
         RPE.powerState.supported = true;
      });
   },

   PlatformEmscriptenPowerStateGetSupported: function() {
      return RPE.powerState.supported;
   },

   PlatformEmscriptenPowerStateGetDischargeTime: function() {
      return RPE.powerState.dischargeTime;
   },

   PlatformEmscriptenPowerStateGetLevel: function() {
      return RPE.powerState.level;
   },

   PlatformEmscriptenPowerStateGetCharging: function() {
      return RPE.powerState.charging;
   },

   PlatformEmscriptenGetTotalMem: function() {
      if (!performance.memory) return 0;
      return performance.memory.jsHeapSizeLimit || 0;
   },

   PlatformEmscriptenGetFreeMem: function() {
      if (!performance.memory) return 0;
      return (performance.memory.jsHeapSizeLimit || 0) - (performance.memory.usedJSHeapSize || 0);
   }
};

autoAddDeps(LibraryPlatformEmscripten, '$RPE');
mergeInto(LibraryManager.library, LibraryPlatformEmscripten);
