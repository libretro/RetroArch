// Every printed line is posted to serve.py, which appends it to log.txt.
Module.print = Module.printErr = function(t) {
  var x = new XMLHttpRequest();
  x.open('POST', '/log', false);
  try { x.send(t); } catch (e) {}
};
