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
      },
      command_queue:[],
      command_reply_queue:[],
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
   },

   $EmscriptenSendCommand__deps:["PlatformEmscriptenCommandRaiseFlag"],
   $EmscriptenSendCommand: function(str) {
      RPE.command_queue.push(str);
      _PlatformEmscriptenCommandRaiseFlag();
   },
   $EmscriptenReceiveCommandReply: function() {
      return RPE.command_reply_queue.shift();
   }
};

autoAddDeps(LibraryPlatformEmscripten, '$RPE');
mergeInto(LibraryManager.library, LibraryPlatformEmscripten);
