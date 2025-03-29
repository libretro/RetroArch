// To work around a bug in emscripten's polyfills for setImmediate in strict mode
var setImmediate;

// To work around a deadlock in firefox
// Use platform_emscripten_has_async_atomics() to determine actual availability
if (Atomics && !Atomics.waitAsync) Atomics.waitAsync = true;
