//"use strict";

// HACK: This is a dummy library that forces ERRNO_CODES to be used, so it's not optimized away.
// Needed for BrowserFS.

var LibraryErrnoCodes = {
   dummyErrnoCodes__deps: ['$ERRNO_CODES'],
   dummyErrnoCodes: function() {
      if (!ERRNO_CODES) {
         console.error("ERRNO_CODES not imported!");
      }
   }
};

mergeInto(LibraryManager.library, LibraryErrnoCodes);
