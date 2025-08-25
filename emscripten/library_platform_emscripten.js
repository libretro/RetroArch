//"use strict";

var LibraryPlatformEmscripten = {
   $RPE: {
      canvasWidth: 0,
      canvasHeight: 0,
      battery: null,
      observer: null,
      memoryUsageTimeout: null,
      sentinelPromise: null,
      command_queue: [],
      command_reply_queue: []
   },

   $PlatformEmscriptenOnWindowResize: function() {
      RPE.observer.unobserve(Module.canvas);
      RPE.observer.observe(Module.canvas);
   },

   PlatformEmscriptenWatchCanvasSizeAndDpr__deps: ["platform_emscripten_update_canvas_dimensions_cb", "$PlatformEmscriptenOnWindowResize"],
   PlatformEmscriptenWatchCanvasSizeAndDpr__proxy: "sync",
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
         RPE.canvasWidth = width;
         RPE.canvasHeight = height;
         // doubles are too big to pass as an argument to exported functions
         {{{ makeSetValue("dpr", "0", "window.devicePixelRatio", "double") }}};
         _platform_emscripten_update_canvas_dimensions_cb(width, height, dpr);
      });
      RPE.observer.observe(Module.canvas);
      window.addEventListener("resize", PlatformEmscriptenOnWindowResize, false);
   },

   $PlatformEmscriptenOnCanvasPointerDown: function() {
      Module.canvas.focus();
   },

   $PlatformEmscriptenOnCanvasContextMenu: function(e) {
      e.preventDefault();
   },

   PlatformEmscriptenCanvasListenersInit__deps: ["$PlatformEmscriptenOnCanvasPointerDown", "$PlatformEmscriptenOnCanvasContextMenu"],
   PlatformEmscriptenCanvasListenersInit__proxy: "sync",
   PlatformEmscriptenCanvasListenersInit: function() {
      Module.canvas.addEventListener("pointerdown", PlatformEmscriptenOnCanvasPointerDown, false);
      Module.canvas.addEventListener("contextmenu", PlatformEmscriptenOnCanvasContextMenu, false);
   },

   $PlatformEmscriptenOnVisibilityChange__deps: ["platform_emscripten_update_window_hidden_cb"],
   $PlatformEmscriptenOnVisibilityChange: function() {
      _platform_emscripten_update_window_hidden_cb(document.visibilityState == "hidden");
   },

   PlatformEmscriptenWatchWindowVisibility__deps: ["$PlatformEmscriptenOnVisibilityChange"],
   PlatformEmscriptenWatchWindowVisibility__proxy: "sync",
   PlatformEmscriptenWatchWindowVisibility: function() {
      document.addEventListener("visibilitychange", PlatformEmscriptenOnVisibilityChange, false);
   },

   $PlatformEmscriptenOnPowerStateChange__deps: ["platform_emscripten_update_power_state_cb"],
   $PlatformEmscriptenOnPowerStateChange: function(e) {
      _platform_emscripten_update_power_state_cb(true, Number.isFinite(e.target.dischargingTime) ? e.target.dischargingTime : 0x7FFFFFFF, e.target.level, e.target.charging);
   },

   PlatformEmscriptenPowerStateInit__deps: ["$PlatformEmscriptenOnPowerStateChange"],
   PlatformEmscriptenPowerStateInit__proxy: "sync",
   PlatformEmscriptenPowerStateInit: function() {
      if (!navigator.getBattery) return;
      navigator.getBattery().then(function(battery) {
         RPE.battery = battery;
         battery.addEventListener("chargingchange", PlatformEmscriptenOnPowerStateChange);
         battery.addEventListener("levelchange", PlatformEmscriptenOnPowerStateChange);
         PlatformEmscriptenOnPowerStateChange({target: battery});
      });
   },

   $PlatformEmscriptenUpdateMemoryUsage__deps: ["platform_emscripten_update_memory_usage_cb"],
   $PlatformEmscriptenUpdateMemoryUsage: function() {
      // unfortunately this will be inaccurate in threaded (worker) builds
      _platform_emscripten_update_memory_usage_cb(BigInt(performance.memory.usedJSHeapSize || 0), BigInt(performance.memory.jsHeapSizeLimit || 0));
      RPE.memoryUsageTimeout = setTimeout(PlatformEmscriptenUpdateMemoryUsage, 5000);
   },

   PlatformEmscriptenMemoryUsageInit__deps: ["$PlatformEmscriptenUpdateMemoryUsage"],
   PlatformEmscriptenMemoryUsageInit: function() {
      if (!performance.memory) return;
      PlatformEmscriptenUpdateMemoryUsage();
   },

   $PlatformEmscriptenOnFullscreenChange__deps: ["platform_emscripten_update_fullscreen_state_cb"],
   $PlatformEmscriptenOnFullscreenChange: function() {
      _platform_emscripten_update_fullscreen_state_cb(!!document.fullscreenElement);
   },

   PlatformEmscriptenWatchFullscreen__deps: ["$PlatformEmscriptenOnFullscreenChange"],
   PlatformEmscriptenWatchFullscreen__proxy: "sync",
   PlatformEmscriptenWatchFullscreen: function() {
      document.addEventListener("fullscreenchange", PlatformEmscriptenOnFullscreenChange, false);
   },

   $PlatformEmscriptenOnGLContextLost__deps: ["platform_emscripten_gl_context_lost_cb"],
   $PlatformEmscriptenOnGLContextLost: function(e) {
      e.preventDefault();
      _platform_emscripten_gl_context_lost_cb();
   },

   $PlatformEmscriptenOnGLContextRestored__deps: ["platform_emscripten_gl_context_restored_cb"],
   $PlatformEmscriptenOnGLContextRestored: function() {
      _platform_emscripten_gl_context_restored_cb();
   },

   PlatformEmscriptenGLContextEventInit__deps: ["$PlatformEmscriptenOnGLContextLost", "$PlatformEmscriptenOnGLContextRestored"],
   PlatformEmscriptenGLContextEventInit__proxy: "sync",
   PlatformEmscriptenGLContextEventInit: function() {
      Module.canvas.addEventListener("webglcontextlost", PlatformEmscriptenOnGLContextLost);
      Module.canvas.addEventListener("webglcontextrestored", PlatformEmscriptenOnGLContextRestored);
   },

   $PlatformEmscriptenDoSetCanvasSize: async function(width, height) {
      // window.resizeTo is terrible; it uses window.outerWidth/outerHeight dimensions,
      // which are on their own scale entirely, which also changes with the zoom level independently from the DPR.
      // instead try 2 hardcoded sizes first and interpolate to find the correct size.
      var expAX = 600;
      var expAY = 450;
      var expBX = expAX + 100;
      var expBY = expAY + 100;
      await new Promise(r => setTimeout(r, 0));
      window.resizeTo(expAX, expAY);
      await new Promise(r => setTimeout(r, 50));
      var oldWidth = RPE.canvasWidth;
      var oldHeight = RPE.canvasHeight;
      window.resizeTo(expBX, expBY);
      await new Promise(r => setTimeout(r, 50));
      var projX = (expBX - expAX) * (width - oldWidth) / (RPE.canvasWidth - oldWidth) + expAX;
      var projY = (expBY - expAY) * (height - oldHeight) / (RPE.canvasHeight - oldHeight) + expAY;
      window.resizeTo(Math.round(projX), Math.round(projY));
   },

   PlatformEmscriptenSetCanvasSize__proxy: "sync",
   PlatformEmscriptenSetCanvasSize__deps: ["$PlatformEmscriptenDoSetCanvasSize"],
   PlatformEmscriptenSetCanvasSize: function(width, height) {
      // c-accessible library functions should not be async
      PlatformEmscriptenDoSetCanvasSize(width, height);
   },

   $PlatformEmscriptenDoSetWakeLock: async function(state) {
      if (state && !RPE.sentinelPromise && navigator?.wakeLock?.request) {
         try {
            RPE.sentinelPromise = navigator.wakeLock.request("screen");
         } catch (e) {}
      } else if (!state && RPE.sentinelPromise) {
         try {
            var sentinel = await RPE.sentinelPromise;
            sentinel.release();
         } catch (e) {}
         RPE.sentinelPromise = null;
      }
   },

   PlatformEmscriptenSetWakeLock__proxy: "sync",
   PlatformEmscriptenSetWakeLock__deps: ["$PlatformEmscriptenDoSetWakeLock"],
   PlatformEmscriptenSetWakeLock: function(state) {
      PlatformEmscriptenDoSetWakeLock(state);
   },

   PlatformEmscriptenGetSystemInfo: function() {
      var userAgent = navigator?.userAgent?.toLowerCase?.();
      if (!userAgent) return 0;
      var browser = 1 + ["chrom", "firefox", "safari"].findIndex(i => userAgent.includes(i));
      var os = 1 + [/windows/, /linux|cros|android/, /iphone|ipad/, /mac os/].findIndex(i => i.test(userAgent));
      return browser | (os << 16);
   },

   $EmscriptenSendCommand__deps: ["platform_emscripten_command_raise_flag"],
   $EmscriptenSendCommand: function(str) {
      RPE.command_queue.push(str);
      _platform_emscripten_command_raise_flag();
   },

   $EmscriptenReceiveCommandReply: function() {
      return RPE.command_reply_queue.shift();
   },

   $PlatformEmscriptenFreeBrowser__proxy: "sync",
   $PlatformEmscriptenFreeBrowser__deps: ["$PlatformEmscriptenDoSetWakeLock", "$PlatformEmscriptenOnCanvasPointerDown", "$PlatformEmscriptenOnCanvasContextMenu", "$PlatformEmscriptenOnWindowResize", "$PlatformEmscriptenOnVisibilityChange", "$PlatformEmscriptenOnFullscreenChange", "$PlatformEmscriptenOnPowerStateChange"],
   $PlatformEmscriptenFreeBrowser: function() {
      if (RPE.memoryUsageTimeout) clearTimeout(RPE.memoryUsageTimeout);
      PlatformEmscriptenDoSetWakeLock(false);
      if (RPE.observer) {
         RPE.observer.unobserve(Module.canvas);
         RPE.observer = null;
      }
      Module.canvas.removeEventListener("pointerdown", PlatformEmscriptenOnCanvasPointerDown);
      Module.canvas.removeEventListener("contextmenu", PlatformEmscriptenOnCanvasContextMenu);
      window.removeEventListener("resize", PlatformEmscriptenOnWindowResize);
      document.removeEventListener("visibilitychange", PlatformEmscriptenOnVisibilityChange);
      document.removeEventListener("fullscreenchange", PlatformEmscriptenOnFullscreenChange);
      if (RPE.battery) {
         RPE.battery.removeEventListener("chargingchange", PlatformEmscriptenOnPowerStateChange);
         RPE.battery.removeEventListener("levelchange", PlatformEmscriptenOnPowerStateChange);
         RPE.battery = null;
      }
   },

   PlatformEmscriptenFree__deps: ["$PlatformEmscriptenFreeBrowser"],
   PlatformEmscriptenFree: function() {
      Module.canvas.removeEventListener("webglcontextlost", PlatformEmscriptenOnGLContextLost);
      Module.canvas.removeEventListener("webglcontextrestored", PlatformEmscriptenOnGLContextRestored);
      PlatformEmscriptenFreeBrowser();
   }
};

autoAddDeps(LibraryPlatformEmscripten, '$RPE');
addToLibrary(LibraryPlatformEmscripten);
