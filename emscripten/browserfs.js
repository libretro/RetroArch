(function() {
/**
* This file installs all of the polyfills that BrowserFS requires.
*/
// IE < 9 does not define this function.
if (!Date.now) {
    Date.now = function now() {
        return new Date().getTime();
    };
}

// IE < 9 does not define this function.
if (!Array.isArray) {
    Array.isArray = function (vArg) {
        return Object.prototype.toString.call(vArg) === "[object Array]";
    };
}

// IE < 9 does not define this function.
// From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/keys
if (!Object.keys) {
    Object.keys = (function () {
        
        var hasOwnProperty = Object.prototype.hasOwnProperty, hasDontEnumBug = !({ toString: null }).propertyIsEnumerable('toString'), dontEnums = [
            'toString',
            'toLocaleString',
            'valueOf',
            'hasOwnProperty',
            'isPrototypeOf',
            'propertyIsEnumerable',
            'constructor'
        ], dontEnumsLength = dontEnums.length;

        return function (obj) {
            if (typeof obj !== 'object' && (typeof obj !== 'function' || obj === null)) {
                throw new TypeError('Object.keys called on non-object');
            }

            var result = [], prop, i;

            for (prop in obj) {
                if (hasOwnProperty.call(obj, prop)) {
                    result.push(prop);
                }
            }

            if (hasDontEnumBug) {
                for (i = 0; i < dontEnumsLength; i++) {
                    if (hasOwnProperty.call(obj, dontEnums[i])) {
                        result.push(dontEnums[i]);
                    }
                }
            }
            return result;
        };
    }());
}

// IE substr does not support negative indices
if ('ab'.substr(-1) !== 'b') {
    String.prototype.substr = function (substr) {
        return function (start, length) {
            // did we get a negative start, calculate how much it is from the
            // beginning of the string
            if (start < 0)
                start = this.length + start;

            // call the original function
            return substr.call(this, start, length);
        };
    }(String.prototype.substr);
}

// IE < 9 does not support forEach
if (!Array.prototype.forEach) {
    Array.prototype.forEach = function (fn, scope) {
        for (var i = 0; i < this.length; ++i) {
            if (i in this) {
                fn.call(scope, this[i], i, this);
            }
        }
    };
}

// Only IE10 has setImmediate.
// @todo: Determine viability of switching to the 'proper' polyfill for this.
if (typeof setImmediate === 'undefined') {
    // XXX avoid importing the global module.
    var gScope = typeof window !== 'undefined' ? window : typeof self !== 'undefined' ? self : global;
    var timeouts = [];
    var messageName = "zero-timeout-message";
    var canUsePostMessage = function () {
        if (typeof gScope.importScripts !== 'undefined' || !gScope.postMessage) {
            return false;
        }
        var postMessageIsAsync = true;
        var oldOnMessage = gScope.onmessage;
        gScope.onmessage = function () {
            postMessageIsAsync = false;
        };
        gScope.postMessage('', '*');
        gScope.onmessage = oldOnMessage;
        return postMessageIsAsync;
    };
    if (canUsePostMessage()) {
        gScope.setImmediate = function (fn) {
            timeouts.push(fn);
            gScope.postMessage(messageName, "*");
        };
        var handleMessage = function (event) {
            if (event.source === self && event.data === messageName) {
                if (event.stopPropagation) {
                    event.stopPropagation();
                } else {
                    event.cancelBubble = true;
                }
                if (timeouts.length > 0) {
                    var fn = timeouts.shift();
                    return fn();
                }
            }
        };
        if (gScope.addEventListener) {
            gScope.addEventListener('message', handleMessage, true);
        } else {
            gScope.attachEvent('onmessage', handleMessage);
        }
    } else if (gScope.MessageChannel) {
        // WebWorker MessageChannel
        var channel = new gScope.MessageChannel();
        channel.port1.onmessage = function (event) {
            if (timeouts.length > 0) {
                return timeouts.shift()();
            }
        };
        gScope.setImmediate = function (fn) {
            timeouts.push(fn);
            channel.port2.postMessage('');
        };
    } else {
        gScope.setImmediate = function (fn) {
            return setTimeout(fn, 0);
            var scriptEl = window.document.createElement("script");
            scriptEl.onreadystatechange = function () {
                fn();
                scriptEl.onreadystatechange = null;
                scriptEl.parentNode.removeChild(scriptEl);
                return scriptEl = null;
            };
            gScope.document.documentElement.appendChild(scriptEl);
        };
    }
}

// IE<9 does not define indexOf.
// From: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/indexOf
if (!Array.prototype.indexOf) {
    Array.prototype.indexOf = function (searchElement, fromIndex) {
        if (typeof fromIndex === "undefined") { fromIndex = 0; }
        if (!this) {
            throw new TypeError();
        }

        var length = this.length;
        if (length === 0 || pivot >= length) {
            return -1;
        }

        var pivot = fromIndex;
        if (pivot < 0) {
            pivot = length + pivot;
        }

        for (var i = pivot; i < length; i++) {
            if (this[i] === searchElement) {
                return i;
            }
        }
        return -1;
    };
}

// IE<9 does not support forEach
// From: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach
if (!Array.prototype.forEach) {
    Array.prototype.forEach = function (fn, scope) {
        var i, len;
        for (i = 0, len = this.length; i < len; ++i) {
            if (i in this) {
                fn.call(scope, this[i], i, this);
            }
        }
    };
}

// IE<9 does not support map
// From: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/map
if (!Array.prototype.map) {
    Array.prototype.map = function (callback, thisArg) {
        var T, A, k;
        if (this == null) {
            throw new TypeError(" this is null or not defined");
        }

        // 1. Let O be the result of calling ToObject passing the |this| value as the argument.
        var O = Object(this);

        // 2. Let lenValue be the result of calling the Get internal method of O with the argument "length".
        // 3. Let len be ToUint32(lenValue).
        var len = O.length >>> 0;

        // 4. If IsCallable(callback) is false, throw a TypeError exception.
        // See: http://es5.github.com/#x9.11
        if (typeof callback !== "function") {
            throw new TypeError(callback + " is not a function");
        }

        // 5. If thisArg was supplied, let T be thisArg; else let T be undefined.
        if (thisArg) {
            T = thisArg;
        }

        // 6. Let A be a new array created as if by the expression new Array(len) where Array is
        // the standard built-in constructor with that name and len is the value of len.
        A = new Array(len);

        // 7. Let k be 0
        k = 0;

        while (k < len) {
            var kValue, mappedValue;

            // a. Let Pk be ToString(k).
            //   This is implicit for LHS operands of the in operator
            // b. Let kPresent be the result of calling the HasProperty internal method of O with argument Pk.
            //   This step can be combined with c
            // c. If kPresent is true, then
            if (k in O) {
                // i. Let kValue be the result of calling the Get internal method of O with argument Pk.
                kValue = O[k];

                // ii. Let mappedValue be the result of calling the Call internal method of callback
                // with T as the this value and argument list containing kValue, k, and O.
                mappedValue = callback.call(T, kValue, k, O);

                // iii. Call the DefineOwnProperty internal method of A with arguments
                // Pk, Property Descriptor {Value: mappedValue, : true, Enumerable: true, Configurable: true},
                // and false.
                // In browsers that support Object.defineProperty, use the following:
                // Object.defineProperty(A, Pk, { value: mappedValue, writable: true, enumerable: true, configurable: true });
                // For best browser support, use the following:
                A[k] = mappedValue;
            }

            // d. Increase k by 1.
            k++;
        }

        // 9. return A
        return A;
    };
}

/**
* IE9 and below only: Injects a VBScript function that converts the
* 'responseBody' attribute of an XMLHttpRequest into a bytestring.
* From: http://miskun.com/javascript/internet-explorer-and-binary-files-data-access/#comment-17
*
* This must be performed *before* the page finishes loading, otherwise
* document.write will refresh the page. :(
*
* This is harmless to inject into non-IE browsers.
*/
if (typeof document !== 'undefined' && window['chrome'] === undefined) {
    document.write("<!-- IEBinaryToArray_ByteStr -->\r\n" + "<script type='text/vbscript'>\r\n" + "Function IEBinaryToArray_ByteStr(Binary)\r\n" + " IEBinaryToArray_ByteStr = CStr(Binary)\r\n" + "End Function\r\n" + "Function IEBinaryToArray_ByteStr_Last(Binary)\r\n" + " Dim lastIndex\r\n" + " lastIndex = LenB(Binary)\r\n" + " if lastIndex mod 2 Then\r\n" + " IEBinaryToArray_ByteStr_Last = Chr( AscB( MidB( Binary, lastIndex, 1 ) ) )\r\n" + " Else\r\n" + " IEBinaryToArray_ByteStr_Last = " + '""' + "\r\n" + " End If\r\n" + "End Function\r\n" + "</script>\r\n");
}
//# sourceMappingURL=polyfills.js.map



/**
 * @license almond 0.3.0 Copyright (c) 2011-2014, The Dojo Foundation All Rights Reserved.
 * Available via the MIT or new BSD license.
 * see: http://github.com/jrburke/almond for details
 */
//Going sloppy to avoid 'use strict' string cost, but strict practices should
//be followed.
/*jslint sloppy: true */
/*global setTimeout: false */

var requirejs, require, define;
(function (undef) {
    var main, req, makeMap, handlers,
        defined = {},
        waiting = {},
        config = {},
        defining = {},
        hasOwn = Object.prototype.hasOwnProperty,
        aps = [].slice,
        jsSuffixRegExp = /\.js$/;

    function hasProp(obj, prop) {
        return hasOwn.call(obj, prop);
    }

    /**
     * Given a relative module name, like ./something, normalize it to
     * a real name that can be mapped to a path.
     * @param {String} name the relative name
     * @param {String} baseName a real name that the name arg is relative
     * to.
     * @returns {String} normalized name
     */
    function normalize(name, baseName) {
        var nameParts, nameSegment, mapValue, foundMap, lastIndex,
            foundI, foundStarMap, starI, i, j, part,
            baseParts = baseName && baseName.split("/"),
            map = config.map,
            starMap = (map && map['*']) || {};

        //Adjust any relative paths.
        if (name && name.charAt(0) === ".") {
            //If have a base name, try to normalize against it,
            //otherwise, assume it is a top-level require that will
            //be relative to baseUrl in the end.
            if (baseName) {
                //Convert baseName to array, and lop off the last part,
                //so that . matches that "directory" and not name of the baseName's
                //module. For instance, baseName of "one/two/three", maps to
                //"one/two/three.js", but we want the directory, "one/two" for
                //this normalization.
                baseParts = baseParts.slice(0, baseParts.length - 1);
                name = name.split('/');
                lastIndex = name.length - 1;

                // Node .js allowance:
                if (config.nodeIdCompat && jsSuffixRegExp.test(name[lastIndex])) {
                    name[lastIndex] = name[lastIndex].replace(jsSuffixRegExp, '');
                }

                name = baseParts.concat(name);

                //start trimDots
                for (i = 0; i < name.length; i += 1) {
                    part = name[i];
                    if (part === ".") {
                        name.splice(i, 1);
                        i -= 1;
                    } else if (part === "..") {
                        if (i === 1 && (name[2] === '..' || name[0] === '..')) {
                            //End of the line. Keep at least one non-dot
                            //path segment at the front so it can be mapped
                            //correctly to disk. Otherwise, there is likely
                            //no path mapping for a path starting with '..'.
                            //This can still fail, but catches the most reasonable
                            //uses of ..
                            break;
                        } else if (i > 0) {
                            name.splice(i - 1, 2);
                            i -= 2;
                        }
                    }
                }
                //end trimDots

                name = name.join("/");
            } else if (name.indexOf('./') === 0) {
                // No baseName, so this is ID is resolved relative
                // to baseUrl, pull off the leading dot.
                name = name.substring(2);
            }
        }

        //Apply map config if available.
        if ((baseParts || starMap) && map) {
            nameParts = name.split('/');

            for (i = nameParts.length; i > 0; i -= 1) {
                nameSegment = nameParts.slice(0, i).join("/");

                if (baseParts) {
                    //Find the longest baseName segment match in the config.
                    //So, do joins on the biggest to smallest lengths of baseParts.
                    for (j = baseParts.length; j > 0; j -= 1) {
                        mapValue = map[baseParts.slice(0, j).join('/')];

                        //baseName segment has  config, find if it has one for
                        //this name.
                        if (mapValue) {
                            mapValue = mapValue[nameSegment];
                            if (mapValue) {
                                //Match, update name to the new value.
                                foundMap = mapValue;
                                foundI = i;
                                break;
                            }
                        }
                    }
                }

                if (foundMap) {
                    break;
                }

                //Check for a star map match, but just hold on to it,
                //if there is a shorter segment match later in a matching
                //config, then favor over this star map.
                if (!foundStarMap && starMap && starMap[nameSegment]) {
                    foundStarMap = starMap[nameSegment];
                    starI = i;
                }
            }

            if (!foundMap && foundStarMap) {
                foundMap = foundStarMap;
                foundI = starI;
            }

            if (foundMap) {
                nameParts.splice(0, foundI, foundMap);
                name = nameParts.join('/');
            }
        }

        return name;
    }

    function makeRequire(relName, forceSync) {
        return function () {
            //A version of a require function that passes a moduleName
            //value for items that may need to
            //look up paths relative to the moduleName
            var args = aps.call(arguments, 0);

            //If first arg is not require('string'), and there is only
            //one arg, it is the array form without a callback. Insert
            //a null so that the following concat is correct.
            if (typeof args[0] !== 'string' && args.length === 1) {
                args.push(null);
            }
            return req.apply(undef, args.concat([relName, forceSync]));
        };
    }

    function makeNormalize(relName) {
        return function (name) {
            return normalize(name, relName);
        };
    }

    function makeLoad(depName) {
        return function (value) {
            defined[depName] = value;
        };
    }

    function callDep(name) {
        if (hasProp(waiting, name)) {
            var args = waiting[name];
            delete waiting[name];
            defining[name] = true;
            main.apply(undef, args);
        }

        if (!hasProp(defined, name) && !hasProp(defining, name)) {
            throw new Error('No ' + name);
        }
        return defined[name];
    }

    //Turns a plugin!resource to [plugin, resource]
    //with the plugin being undefined if the name
    //did not have a plugin prefix.
    function splitPrefix(name) {
        var prefix,
            index = name ? name.indexOf('!') : -1;
        if (index > -1) {
            prefix = name.substring(0, index);
            name = name.substring(index + 1, name.length);
        }
        return [prefix, name];
    }

    /**
     * Makes a name map, normalizing the name, and using a plugin
     * for normalization if necessary. Grabs a ref to plugin
     * too, as an optimization.
     */
    makeMap = function (name, relName) {
        var plugin,
            parts = splitPrefix(name),
            prefix = parts[0];

        name = parts[1];

        if (prefix) {
            prefix = normalize(prefix, relName);
            plugin = callDep(prefix);
        }

        //Normalize according
        if (prefix) {
            if (plugin && plugin.normalize) {
                name = plugin.normalize(name, makeNormalize(relName));
            } else {
                name = normalize(name, relName);
            }
        } else {
            name = normalize(name, relName);
            parts = splitPrefix(name);
            prefix = parts[0];
            name = parts[1];
            if (prefix) {
                plugin = callDep(prefix);
            }
        }

        //Using ridiculous property names for space reasons
        return {
            f: prefix ? prefix + '!' + name : name, //fullName
            n: name,
            pr: prefix,
            p: plugin
        };
    };

    function makeConfig(name) {
        return function () {
            return (config && config.config && config.config[name]) || {};
        };
    }

    handlers = {
        require: function (name) {
            return makeRequire(name);
        },
        exports: function (name) {
            var e = defined[name];
            if (typeof e !== 'undefined') {
                return e;
            } else {
                return (defined[name] = {});
            }
        },
        module: function (name) {
            return {
                id: name,
                uri: '',
                exports: defined[name],
                config: makeConfig(name)
            };
        }
    };

    main = function (name, deps, callback, relName) {
        var cjsModule, depName, ret, map, i,
            args = [],
            callbackType = typeof callback,
            usingExports;

        //Use name if no relName
        relName = relName || name;

        //Call the callback to define the module, if necessary.
        if (callbackType === 'undefined' || callbackType === 'function') {
            //Pull out the defined dependencies and pass the ordered
            //values to the callback.
            //Default to [require, exports, module] if no deps
            deps = !deps.length && callback.length ? ['require', 'exports', 'module'] : deps;
            for (i = 0; i < deps.length; i += 1) {
                map = makeMap(deps[i], relName);
                depName = map.f;

                //Fast path CommonJS standard dependencies.
                if (depName === "require") {
                    args[i] = handlers.require(name);
                } else if (depName === "exports") {
                    //CommonJS module spec 1.1
                    args[i] = handlers.exports(name);
                    usingExports = true;
                } else if (depName === "module") {
                    //CommonJS module spec 1.1
                    cjsModule = args[i] = handlers.module(name);
                } else if (hasProp(defined, depName) ||
                           hasProp(waiting, depName) ||
                           hasProp(defining, depName)) {
                    args[i] = callDep(depName);
                } else if (map.p) {
                    map.p.load(map.n, makeRequire(relName, true), makeLoad(depName), {});
                    args[i] = defined[depName];
                } else {
                    throw new Error(name + ' missing ' + depName);
                }
            }

            ret = callback ? callback.apply(defined[name], args) : undefined;

            if (name) {
                //If setting exports via "module" is in play,
                //favor that over return value and exports. After that,
                //favor a non-undefined return value over exports use.
                if (cjsModule && cjsModule.exports !== undef &&
                        cjsModule.exports !== defined[name]) {
                    defined[name] = cjsModule.exports;
                } else if (ret !== undef || !usingExports) {
                    //Use the return value from the function.
                    defined[name] = ret;
                }
            }
        } else if (name) {
            //May just be an object definition for the module. Only
            //worry about defining if have a module name.
            defined[name] = callback;
        }
    };

    requirejs = require = req = function (deps, callback, relName, forceSync, alt) {
        if (typeof deps === "string") {
            if (handlers[deps]) {
                //callback in this case is really relName
                return handlers[deps](callback);
            }
            //Just return the module wanted. In this scenario, the
            //deps arg is the module name, and second arg (if passed)
            //is just the relName.
            //Normalize module name, if it contains . or ..
            return callDep(makeMap(deps, callback).f);
        } else if (!deps.splice) {
            //deps is a config object, not an array.
            config = deps;
            if (config.deps) {
                req(config.deps, config.callback);
            }
            if (!callback) {
                return;
            }

            if (callback.splice) {
                //callback is an array, which means it is a dependency list.
                //Adjust args if there are dependencies
                deps = callback;
                callback = relName;
                relName = null;
            } else {
                deps = undef;
            }
        }

        //Support require(['a'])
        callback = callback || function () {};

        //If relName is a function, it is an errback handler,
        //so remove it.
        if (typeof relName === 'function') {
            relName = forceSync;
            forceSync = alt;
        }

        //Simulate async callback;
        if (forceSync) {
            main(undef, deps, callback, relName);
        } else {
            //Using a non-zero value because of concern for what old browsers
            //do, and latest browsers "upgrade" to 4 if lower value is used:
            //http://www.whatwg.org/specs/web-apps/current-work/multipage/timers.html#dom-windowtimers-settimeout:
            //If want a value immediately, use require('id') instead -- something
            //that works in almond on the global level, but not guaranteed and
            //unlikely to work in other AMD implementations.
            setTimeout(function () {
                main(undef, deps, callback, relName);
            }, 4);
        }

        return req;
    };

    /**
     * Just drops the config on the floor, but returns req in case
     * the config return value is used.
     */
    req.config = function (cfg) {
        return req(cfg);
    };

    /**
     * Expose module registry for debugging and tooling
     */
    requirejs._defined = defined;

    define = function (name, deps, callback) {

        //This module may not have dependencies
        if (!deps.splice) {
            //deps is not an array, so probably means
            //an object literal or factory function for
            //the value. Adjust args.
            callback = deps;
            deps = [];
        }

        if (!hasProp(defined, name) && !hasProp(waiting, name)) {
            waiting[name] = [name, deps, callback];
        }
    };

    define.amd = {
        jQuery: true
    };
}());

define("../../vendor/almond/almond", function(){});

/**
* @module core/api_error
*/
define('core/api_error',["require", "exports"], function(require, exports) {
    /**
    * Standard libc error codes. Add more to this enum and ErrorStrings as they are
    * needed.
    * @url http://www.gnu.org/software/libc/manual/html_node/Error-Codes.html
    */
    (function (ErrorCode) {
        ErrorCode[ErrorCode["EPERM"] = 0] = "EPERM";
        ErrorCode[ErrorCode["ENOENT"] = 1] = "ENOENT";
        ErrorCode[ErrorCode["EIO"] = 2] = "EIO";
        ErrorCode[ErrorCode["EBADF"] = 3] = "EBADF";
        ErrorCode[ErrorCode["EACCES"] = 4] = "EACCES";
        ErrorCode[ErrorCode["EBUSY"] = 5] = "EBUSY";
        ErrorCode[ErrorCode["EEXIST"] = 6] = "EEXIST";
        ErrorCode[ErrorCode["ENOTDIR"] = 7] = "ENOTDIR";
        ErrorCode[ErrorCode["EISDIR"] = 8] = "EISDIR";
        ErrorCode[ErrorCode["EINVAL"] = 9] = "EINVAL";
        ErrorCode[ErrorCode["EFBIG"] = 10] = "EFBIG";
        ErrorCode[ErrorCode["ENOSPC"] = 11] = "ENOSPC";
        ErrorCode[ErrorCode["EROFS"] = 12] = "EROFS";
        ErrorCode[ErrorCode["ENOTEMPTY"] = 13] = "ENOTEMPTY";
        ErrorCode[ErrorCode["ENOTSUP"] = 14] = "ENOTSUP";
    })(exports.ErrorCode || (exports.ErrorCode = {}));
    var ErrorCode = exports.ErrorCode;

    /**
    * Strings associated with each error code.
    */
    var ErrorStrings = {};
    ErrorStrings[0 /* EPERM */] = 'Operation not permitted.';
    ErrorStrings[1 /* ENOENT */] = 'No such file or directory.';
    ErrorStrings[2 /* EIO */] = 'Input/output error.';
    ErrorStrings[3 /* EBADF */] = 'Bad file descriptor.';
    ErrorStrings[4 /* EACCES */] = 'Permission denied.';
    ErrorStrings[5 /* EBUSY */] = 'Resource busy or locked.';
    ErrorStrings[6 /* EEXIST */] = 'File exists.';
    ErrorStrings[7 /* ENOTDIR */] = 'File is not a directory.';
    ErrorStrings[8 /* EISDIR */] = 'File is a directory.';
    ErrorStrings[9 /* EINVAL */] = 'Invalid argument.';
    ErrorStrings[10 /* EFBIG */] = 'File is too big.';
    ErrorStrings[11 /* ENOSPC */] = 'No space left on disk.';
    ErrorStrings[12 /* EROFS */] = 'Cannot modify a read-only file system.';
    ErrorStrings[13 /* ENOTEMPTY */] = 'Directory is not empty.';
    ErrorStrings[14 /* ENOTSUP */] = 'Operation is not supported.';

    /**
    * Represents a BrowserFS error. Passed back to applications after a failed
    * call to the BrowserFS API.
    */
    var ApiError = (function () {
        /**
        * Represents a BrowserFS error. Passed back to applications after a failed
        * call to the BrowserFS API.
        *
        * Error codes mirror those returned by regular Unix file operations, which is
        * what Node returns.
        * @constructor ApiError
        * @param type The type of the error.
        * @param [message] A descriptive error message.
        */
        function ApiError(type, message) {
            this.type = type;
            this.code = ErrorCode[type];
            if (message != null) {
                this.message = message;
            } else {
                this.message = ErrorStrings[type];
            }
        }
        /**
        * @return A friendly error message.
        */
        ApiError.prototype.toString = function () {
            return this.code + ": " + ErrorStrings[this.type] + " " + this.message;
        };

        ApiError.FileError = function (code, p) {
            return new ApiError(code, p + ": " + ErrorStrings[code]);
        };
        ApiError.ENOENT = function (path) {
            return this.FileError(1 /* ENOENT */, path);
        };

        ApiError.EEXIST = function (path) {
            return this.FileError(6 /* EEXIST */, path);
        };

        ApiError.EISDIR = function (path) {
            return this.FileError(8 /* EISDIR */, path);
        };

        ApiError.ENOTDIR = function (path) {
            return this.FileError(7 /* ENOTDIR */, path);
        };

        ApiError.EPERM = function (path) {
            return this.FileError(0 /* EPERM */, path);
        };
        return ApiError;
    })();
    exports.ApiError = ApiError;
});
//# sourceMappingURL=api_error.js.map
;
define('core/buffer_core',["require", "exports", './api_error'], function(require, exports, api_error) {
    var FLOAT_POS_INFINITY = Math.pow(2, 128);
    var FLOAT_NEG_INFINITY = -1 * FLOAT_POS_INFINITY;
    var FLOAT_POS_INFINITY_AS_INT = 0x7F800000;
    var FLOAT_NEG_INFINITY_AS_INT = -8388608;
    var FLOAT_NaN_AS_INT = 0x7fc00000;

    

    /**
    * Contains common definitions for most of the BufferCore classes.
    * Subclasses only need to implement write/readUInt8 for full functionality.
    */
    var BufferCoreCommon = (function () {
        function BufferCoreCommon() {
        }
        BufferCoreCommon.prototype.getLength = function () {
            throw new api_error.ApiError(14 /* ENOTSUP */, 'BufferCore implementations should implement getLength.');
        };
        BufferCoreCommon.prototype.writeInt8 = function (i, data) {
            // Pack the sign bit as the highest bit.
            // Note that we keep the highest bit in the value byte as the sign bit if it
            // exists.
            this.writeUInt8(i, (data & 0xFF) | ((data & 0x80000000) >>> 24));
        };
        BufferCoreCommon.prototype.writeInt16LE = function (i, data) {
            this.writeUInt8(i, data & 0xFF);

            // Pack the sign bit as the highest bit.
            // Note that we keep the highest bit in the value byte as the sign bit if it
            // exists.
            this.writeUInt8(i + 1, ((data >>> 8) & 0xFF) | ((data & 0x80000000) >>> 24));
        };
        BufferCoreCommon.prototype.writeInt16BE = function (i, data) {
            this.writeUInt8(i + 1, data & 0xFF);

            // Pack the sign bit as the highest bit.
            // Note that we keep the highest bit in the value byte as the sign bit if it
            // exists.
            this.writeUInt8(i, ((data >>> 8) & 0xFF) | ((data & 0x80000000) >>> 24));
        };
        BufferCoreCommon.prototype.writeInt32LE = function (i, data) {
            this.writeUInt8(i, data & 0xFF);
            this.writeUInt8(i + 1, (data >>> 8) & 0xFF);
            this.writeUInt8(i + 2, (data >>> 16) & 0xFF);
            this.writeUInt8(i + 3, (data >>> 24) & 0xFF);
        };
        BufferCoreCommon.prototype.writeInt32BE = function (i, data) {
            this.writeUInt8(i + 3, data & 0xFF);
            this.writeUInt8(i + 2, (data >>> 8) & 0xFF);
            this.writeUInt8(i + 1, (data >>> 16) & 0xFF);
            this.writeUInt8(i, (data >>> 24) & 0xFF);
        };
        BufferCoreCommon.prototype.writeUInt8 = function (i, data) {
            throw new api_error.ApiError(14 /* ENOTSUP */, 'BufferCore implementations should implement writeUInt8.');
        };
        BufferCoreCommon.prototype.writeUInt16LE = function (i, data) {
            this.writeUInt8(i, data & 0xFF);
            this.writeUInt8(i + 1, (data >> 8) & 0xFF);
        };
        BufferCoreCommon.prototype.writeUInt16BE = function (i, data) {
            this.writeUInt8(i + 1, data & 0xFF);
            this.writeUInt8(i, (data >> 8) & 0xFF);
        };
        BufferCoreCommon.prototype.writeUInt32LE = function (i, data) {
            this.writeInt32LE(i, data | 0);
        };
        BufferCoreCommon.prototype.writeUInt32BE = function (i, data) {
            this.writeInt32BE(i, data | 0);
        };
        BufferCoreCommon.prototype.writeFloatLE = function (i, data) {
            this.writeInt32LE(i, this.float2intbits(data));
        };
        BufferCoreCommon.prototype.writeFloatBE = function (i, data) {
            this.writeInt32BE(i, this.float2intbits(data));
        };
        BufferCoreCommon.prototype.writeDoubleLE = function (i, data) {
            var doubleBits = this.double2longbits(data);
            this.writeInt32LE(i, doubleBits[0]);
            this.writeInt32LE(i + 4, doubleBits[1]);
        };
        BufferCoreCommon.prototype.writeDoubleBE = function (i, data) {
            var doubleBits = this.double2longbits(data);
            this.writeInt32BE(i + 4, doubleBits[0]);
            this.writeInt32BE(i, doubleBits[1]);
        };
        BufferCoreCommon.prototype.readInt8 = function (i) {
            var val = this.readUInt8(i);
            if (val & 0x80) {
                // Sign bit is set, so perform sign extension.
                return val | 0xFFFFFF80;
            } else {
                return val;
            }
        };
        BufferCoreCommon.prototype.readInt16LE = function (i) {
            var val = this.readUInt16LE(i);
            if (val & 0x8000) {
                // Sign bit is set, so perform sign extension.
                return val | 0xFFFF8000;
            } else {
                return val;
            }
        };
        BufferCoreCommon.prototype.readInt16BE = function (i) {
            var val = this.readUInt16BE(i);
            if (val & 0x8000) {
                // Sign bit is set, so perform sign extension.
                return val | 0xFFFF8000;
            } else {
                return val;
            }
        };
        BufferCoreCommon.prototype.readInt32LE = function (i) {
            return this.readUInt32LE(i) | 0;
        };
        BufferCoreCommon.prototype.readInt32BE = function (i) {
            return this.readUInt32BE(i) | 0;
        };
        BufferCoreCommon.prototype.readUInt8 = function (i) {
            throw new api_error.ApiError(14 /* ENOTSUP */, 'BufferCore implementations should implement readUInt8.');
        };
        BufferCoreCommon.prototype.readUInt16LE = function (i) {
            return (this.readUInt8(i + 1) << 8) | this.readUInt8(i);
        };
        BufferCoreCommon.prototype.readUInt16BE = function (i) {
            return (this.readUInt8(i) << 8) | this.readUInt8(i + 1);
        };
        BufferCoreCommon.prototype.readUInt32LE = function (i) {
            return ((this.readUInt8(i + 3) << 24) | (this.readUInt8(i + 2) << 16) | (this.readUInt8(i + 1) << 8) | this.readUInt8(i)) >>> 0;
        };
        BufferCoreCommon.prototype.readUInt32BE = function (i) {
            return ((this.readUInt8(i) << 24) | (this.readUInt8(i + 1) << 16) | (this.readUInt8(i + 2) << 8) | this.readUInt8(i + 3)) >>> 0;
        };
        BufferCoreCommon.prototype.readFloatLE = function (i) {
            return this.intbits2float(this.readInt32LE(i));
        };
        BufferCoreCommon.prototype.readFloatBE = function (i) {
            return this.intbits2float(this.readInt32BE(i));
        };
        BufferCoreCommon.prototype.readDoubleLE = function (i) {
            return this.longbits2double(this.readInt32LE(i + 4), this.readInt32LE(i));
        };
        BufferCoreCommon.prototype.readDoubleBE = function (i) {
            return this.longbits2double(this.readInt32BE(i), this.readInt32BE(i + 4));
        };
        BufferCoreCommon.prototype.copy = function (start, end) {
            throw new api_error.ApiError(14 /* ENOTSUP */, 'BufferCore implementations should implement copy.');
        };
        BufferCoreCommon.prototype.fill = function (value, start, end) {
            for (var i = start; i < end; i++) {
                this.writeUInt8(i, value);
            }
        };

        BufferCoreCommon.prototype.float2intbits = function (f_val) {
            var exp, f_view, i_view, sig, sign;

            // Special cases!
            if (f_val === 0) {
                return 0;
            }

            // We map the infinities to JavaScript infinities. Map them back.
            if (f_val === Number.POSITIVE_INFINITY) {
                return FLOAT_POS_INFINITY_AS_INT;
            }
            if (f_val === Number.NEGATIVE_INFINITY) {
                return FLOAT_NEG_INFINITY_AS_INT;
            }

            // Convert JavaScript NaN to Float NaN value.
            if (isNaN(f_val)) {
                return FLOAT_NaN_AS_INT;
            }

            // We have more bits of precision than a float, so below we round to
            // the nearest significand. This appears to be what the x86
            // Java does for normal floating point operations.
            sign = f_val < 0 ? 1 : 0;
            f_val = Math.abs(f_val);

            // Subnormal zone!
            // (−1)^signbits×2^−126×0.significandbits
            // Largest subnormal magnitude:
            // 0000 0000 0111 1111 1111 1111 1111 1111
            // Smallest subnormal magnitude:
            // 0000 0000 0000 0000 0000 0000 0000 0001
            if (f_val <= 1.1754942106924411e-38 && f_val >= 1.4012984643248170e-45) {
                exp = 0;
                sig = Math.round((f_val / Math.pow(2, -126)) * Math.pow(2, 23));
                return (sign << 31) | (exp << 23) | sig;
            } else {
                // Regular FP numbers
                exp = Math.floor(Math.log(f_val) / Math.LN2);
                sig = Math.round((f_val / Math.pow(2, exp) - 1) * Math.pow(2, 23));
                return (sign << 31) | ((exp + 127) << 23) | sig;
            }
        };

        BufferCoreCommon.prototype.double2longbits = function (d_val) {
            var d_view, exp, high_bits, i_view, sig, sign;

            // Special cases
            if (d_val === 0) {
                return [0, 0];
            }
            if (d_val === Number.POSITIVE_INFINITY) {
                // High bits: 0111 1111 1111 0000 0000 0000 0000 0000
                //  Low bits: 0000 0000 0000 0000 0000 0000 0000 0000
                return [0, 2146435072];
            } else if (d_val === Number.NEGATIVE_INFINITY) {
                // High bits: 1111 1111 1111 0000 0000 0000 0000 0000
                //  Low bits: 0000 0000 0000 0000 0000 0000 0000 0000
                return [0, -1048576];
            } else if (isNaN(d_val)) {
                // High bits: 0111 1111 1111 1000 0000 0000 0000 0000
                //  Low bits: 0000 0000 0000 0000 0000 0000 0000 0000
                return [0, 2146959360];
            }
            sign = d_val < 0 ? 1 << 31 : 0;
            d_val = Math.abs(d_val);

            // Check if it is a subnormal number.
            // (-1)s × 0.f × 2-1022
            // Largest subnormal magnitude:
            // 0000 0000 0000 1111 1111 1111 1111 1111
            // 1111 1111 1111 1111 1111 1111 1111 1111
            // Smallest subnormal magnitude:
            // 0000 0000 0000 0000 0000 0000 0000 0000
            // 0000 0000 0000 0000 0000 0000 0000 0001
            if (d_val <= 2.2250738585072010e-308 && d_val >= 5.0000000000000000e-324) {
                exp = 0;
                sig = (d_val / Math.pow(2, -1022)) * Math.pow(2, 52);
            } else {
                exp = Math.floor(Math.log(d_val) / Math.LN2);

                // If d_val is close to a power of two, there's a chance that exp
                // will be 1 greater than it should due to loss of accuracy in the
                // log result.
                if (d_val < Math.pow(2, exp)) {
                    exp = exp - 1;
                }
                sig = (d_val / Math.pow(2, exp) - 1) * Math.pow(2, 52);
                exp = (exp + 1023) << 20;
            }

            // Simulate >> 32
            high_bits = ((sig * Math.pow(2, -32)) | 0) | sign | exp;
            return [sig & 0xFFFF, high_bits];
        };

        BufferCoreCommon.prototype.intbits2float = function (int32) {
            // Map +/- infinity to JavaScript equivalents
            if (int32 === FLOAT_POS_INFINITY_AS_INT) {
                return Number.POSITIVE_INFINITY;
            } else if (int32 === FLOAT_NEG_INFINITY_AS_INT) {
                return Number.NEGATIVE_INFINITY;
            }
            var sign = (int32 & 0x80000000) >>> 31;
            var exponent = (int32 & 0x7F800000) >>> 23;
            var significand = int32 & 0x007FFFFF;
            var value;
            if (exponent === 0) {
                value = Math.pow(-1, sign) * significand * Math.pow(2, -149);
            } else {
                value = Math.pow(-1, sign) * (1 + significand * Math.pow(2, -23)) * Math.pow(2, exponent - 127);
            }

            // NaN check
            if (value < FLOAT_NEG_INFINITY || value > FLOAT_POS_INFINITY) {
                value = NaN;
            }
            return value;
        };

        BufferCoreCommon.prototype.longbits2double = function (uint32_a, uint32_b) {
            var sign = (uint32_a & 0x80000000) >>> 31;
            var exponent = (uint32_a & 0x7FF00000) >>> 20;
            var significand = ((uint32_a & 0x000FFFFF) * Math.pow(2, 32)) + uint32_b;

            // Special values!
            if (exponent === 0 && significand === 0) {
                return 0;
            }
            if (exponent === 2047) {
                if (significand === 0) {
                    if (sign === 1) {
                        return Number.NEGATIVE_INFINITY;
                    }
                    return Number.POSITIVE_INFINITY;
                } else {
                    return NaN;
                }
            }
            if (exponent === 0)
                return Math.pow(-1, sign) * significand * Math.pow(2, -1074);
            return Math.pow(-1, sign) * (1 + significand * Math.pow(2, -52)) * Math.pow(2, exponent - 1023);
        };
        return BufferCoreCommon;
    })();
    exports.BufferCoreCommon = BufferCoreCommon;
});
//# sourceMappingURL=buffer_core.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/buffer_core_array',["require", "exports", './buffer_core'], function(require, exports, buffer_core) {
    // Used to clear segments of an array index.
    var clearMasks = [0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0x00FFFFFF];

    /**
    * Implementation of BufferCore that is backed by an array of 32-bit ints.
    * Data is stored little endian.
    * Example: Bytes 0 through 3 are present in the first int:
    *  BYTE 3      BYTE 2      BYTE 1      BYTE 0
    * 0000 0000 | 0000 0000 | 0000 0000 | 0000 0000
    */
    var BufferCoreArray = (function (_super) {
        __extends(BufferCoreArray, _super);
        function BufferCoreArray(length) {
            _super.call(this);
            this.length = length;
            this.buff = new Array(Math.ceil(length / 4));

            // Zero-fill the array.
            var bufflen = this.buff.length;
            for (var i = 0; i < bufflen; i++) {
                this.buff[i] = 0;
            }
        }
        BufferCoreArray.isAvailable = function () {
            return true;
        };

        BufferCoreArray.prototype.getLength = function () {
            return this.length;
        };
        BufferCoreArray.prototype.writeUInt8 = function (i, data) {
            data &= 0xFF;

            // Which int? (Equivalent to (i/4)|0)
            var arrIdx = i >> 2;

            // Which offset? (Equivalent to i - arrIdx*4)
            var intIdx = i & 3;
            this.buff[arrIdx] = this.buff[arrIdx] & clearMasks[intIdx];
            this.buff[arrIdx] = this.buff[arrIdx] | (data << (intIdx << 3));
        };
        BufferCoreArray.prototype.readUInt8 = function (i) {
            // Which int?
            var arrIdx = i >> 2;

            // Which offset?
            var intIdx = i & 3;

            // Bring the data we want into the lowest 8 bits, and truncate.
            return (this.buff[arrIdx] >> (intIdx << 3)) & 0xFF;
        };
        BufferCoreArray.prototype.copy = function (start, end) {
            // Stupid unoptimized copy. Later, we could do optimizations when aligned.
            var newBC = new BufferCoreArray(end - start);
            for (var i = start; i < end; i++) {
                newBC.writeUInt8(i - start, this.readUInt8(i));
            }
            return newBC;
        };
        return BufferCoreArray;
    })(buffer_core.BufferCoreCommon);
    exports.BufferCoreArray = BufferCoreArray;

    // Type-check the class.
    var _ = BufferCoreArray;
});
//# sourceMappingURL=buffer_core_array.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/buffer_core_arraybuffer',["require", "exports", './buffer_core'], function(require, exports, buffer_core) {
    /**
    * Represents data using an ArrayBuffer.
    */
    var BufferCoreArrayBuffer = (function (_super) {
        __extends(BufferCoreArrayBuffer, _super);
        function BufferCoreArrayBuffer(arg1) {
            _super.call(this);
            if (typeof arg1 === 'number') {
                this.buff = new DataView(new ArrayBuffer(arg1));
            } else if (arg1 instanceof DataView) {
                this.buff = arg1;
            } else {
                this.buff = new DataView(arg1);
            }
            this.length = this.buff.byteLength;
        }
        BufferCoreArrayBuffer.isAvailable = function () {
            return typeof DataView !== 'undefined';
        };

        BufferCoreArrayBuffer.prototype.getLength = function () {
            return this.length;
        };
        BufferCoreArrayBuffer.prototype.writeInt8 = function (i, data) {
            this.buff.setInt8(i, data);
        };
        BufferCoreArrayBuffer.prototype.writeInt16LE = function (i, data) {
            this.buff.setInt16(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeInt16BE = function (i, data) {
            this.buff.setInt16(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.writeInt32LE = function (i, data) {
            this.buff.setInt32(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeInt32BE = function (i, data) {
            this.buff.setInt32(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.writeUInt8 = function (i, data) {
            this.buff.setUint8(i, data);
        };
        BufferCoreArrayBuffer.prototype.writeUInt16LE = function (i, data) {
            this.buff.setUint16(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeUInt16BE = function (i, data) {
            this.buff.setUint16(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.writeUInt32LE = function (i, data) {
            this.buff.setUint32(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeUInt32BE = function (i, data) {
            this.buff.setUint32(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.writeFloatLE = function (i, data) {
            this.buff.setFloat32(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeFloatBE = function (i, data) {
            this.buff.setFloat32(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.writeDoubleLE = function (i, data) {
            this.buff.setFloat64(i, data, true);
        };
        BufferCoreArrayBuffer.prototype.writeDoubleBE = function (i, data) {
            this.buff.setFloat64(i, data, false);
        };
        BufferCoreArrayBuffer.prototype.readInt8 = function (i) {
            return this.buff.getInt8(i);
        };
        BufferCoreArrayBuffer.prototype.readInt16LE = function (i) {
            return this.buff.getInt16(i, true);
        };
        BufferCoreArrayBuffer.prototype.readInt16BE = function (i) {
            return this.buff.getInt16(i, false);
        };
        BufferCoreArrayBuffer.prototype.readInt32LE = function (i) {
            return this.buff.getInt32(i, true);
        };
        BufferCoreArrayBuffer.prototype.readInt32BE = function (i) {
            return this.buff.getInt32(i, false);
        };
        BufferCoreArrayBuffer.prototype.readUInt8 = function (i) {
            return this.buff.getUint8(i);
        };
        BufferCoreArrayBuffer.prototype.readUInt16LE = function (i) {
            return this.buff.getUint16(i, true);
        };
        BufferCoreArrayBuffer.prototype.readUInt16BE = function (i) {
            return this.buff.getUint16(i, false);
        };
        BufferCoreArrayBuffer.prototype.readUInt32LE = function (i) {
            return this.buff.getUint32(i, true);
        };
        BufferCoreArrayBuffer.prototype.readUInt32BE = function (i) {
            return this.buff.getUint32(i, false);
        };
        BufferCoreArrayBuffer.prototype.readFloatLE = function (i) {
            return this.buff.getFloat32(i, true);
        };
        BufferCoreArrayBuffer.prototype.readFloatBE = function (i) {
            return this.buff.getFloat32(i, false);
        };
        BufferCoreArrayBuffer.prototype.readDoubleLE = function (i) {
            return this.buff.getFloat64(i, true);
        };
        BufferCoreArrayBuffer.prototype.readDoubleBE = function (i) {
            return this.buff.getFloat64(i, false);
        };
        BufferCoreArrayBuffer.prototype.copy = function (start, end) {
            var aBuff = this.buff.buffer;
            var newBuff;

            // Some ArrayBuffer implementations (IE10) do not have 'slice'.
            // XXX: Type hacks - the typings don't have slice either.
            if (ArrayBuffer.prototype.slice) {
                // ArrayBuffer.slice is copying; exactly what we want.
                newBuff = aBuff.slice(start, end);
            } else {
                var len = end - start;
                newBuff = new ArrayBuffer(len);

                // Copy the old contents in.
                var newUintArray = new Uint8Array(newBuff);
                var oldUintArray = new Uint8Array(aBuff);
                newUintArray.set(oldUintArray.subarray(start, end));
            }
            return new BufferCoreArrayBuffer(newBuff);
        };
        BufferCoreArrayBuffer.prototype.fill = function (value, start, end) {
            // Value must be a byte wide.
            value = value & 0xFF;
            var i;
            var len = end - start;
            var intBytes = (((len) / 4) | 0) * 4;

            // Optimization: Write 4 bytes at a time.
            // TODO: Could we copy 8 bytes at a time using Float64, or could we
            //       lose precision?
            var intVal = (value << 24) | (value << 16) | (value << 8) | value;
            for (i = 0; i < intBytes; i += 4) {
                this.writeInt32LE(i + start, intVal);
            }
            for (i = intBytes; i < len; i++) {
                this.writeUInt8(i + start, value);
            }
        };

        /**
        * Custom method for this buffer core. Get the backing object.
        */
        BufferCoreArrayBuffer.prototype.getDataView = function () {
            return this.buff;
        };
        return BufferCoreArrayBuffer;
    })(buffer_core.BufferCoreCommon);
    exports.BufferCoreArrayBuffer = BufferCoreArrayBuffer;

    // Type-check the class.
    var _ = BufferCoreArrayBuffer;
});
//# sourceMappingURL=buffer_core_arraybuffer.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/buffer_core_imagedata',["require", "exports", './buffer_core'], function(require, exports, buffer_core) {
    

    /**
    * Implementation of BufferCore that is backed by an ImageData object.
    * Useful in browsers with HTML5 canvas support, but no TypedArray support
    * (IE9).
    */
    var BufferCoreImageData = (function (_super) {
        __extends(BufferCoreImageData, _super);
        function BufferCoreImageData(length) {
            _super.call(this);
            this.length = length;
            this.buff = BufferCoreImageData.getCanvasPixelArray(length);
        }
        /**
        * Constructs a CanvasPixelArray that represents the given amount of bytes.
        */
        BufferCoreImageData.getCanvasPixelArray = function (bytes) {
            var ctx = BufferCoreImageData.imageDataFactory;

            // Lazily initialize, otherwise every browser (even those that will never
            // use this code) will create a canvas on script load.
            if (ctx === undefined) {
                BufferCoreImageData.imageDataFactory = ctx = document.createElement('canvas').getContext('2d');
            }

            // You cannot create image data with size 0, so up it to size 1.
            if (bytes === 0)
                bytes = 1;
            return ctx.createImageData(Math.ceil(bytes / 4), 1).data;
        };
        BufferCoreImageData.isAvailable = function () {
            // Modern browsers have removed this deprecated API, so it is not always around.
            return typeof CanvasPixelArray !== 'undefined';
        };

        BufferCoreImageData.prototype.getLength = function () {
            return this.length;
        };
        BufferCoreImageData.prototype.writeUInt8 = function (i, data) {
            this.buff[i] = data;
        };
        BufferCoreImageData.prototype.readUInt8 = function (i) {
            return this.buff[i];
        };
        BufferCoreImageData.prototype.copy = function (start, end) {
            // AFAIK, there's no efficient way to clone ImageData.
            var newBC = new BufferCoreImageData(end - start);
            for (var i = start; i < end; i++) {
                newBC.writeUInt8(i - start, this.buff[i]);
            }
            return newBC;
        };
        return BufferCoreImageData;
    })(buffer_core.BufferCoreCommon);
    exports.BufferCoreImageData = BufferCoreImageData;

    // Type-check the class.
    var _ = BufferCoreImageData;
});
//# sourceMappingURL=buffer_core_imagedata.js.map
;
define('core/string_util',["require", "exports"], function(require, exports) {
    

    /**
    * Find the 'utility' object for the given string encoding. Throws an exception
    * if the encoding is invalid.
    * @param [String] encoding a string encoding
    * @return [BrowserFS.StringUtil.*] The StringUtil object for the given encoding
    */
    function FindUtil(encoding) {
        encoding = (function () {
            switch (typeof encoding) {
                case 'object':
                    return "" + encoding;
                case 'string':
                    return encoding;
                default:
                    throw new Error('Invalid encoding argument specified');
            }
        })();
        encoding = encoding.toLowerCase();

        switch (encoding) {
            case 'utf8':
            case 'utf-8':
                return UTF8;
            case 'ascii':
                return ASCII;
            case 'binary':
                return BINARY;
            case 'ucs2':
            case 'ucs-2':
            case 'utf16le':
            case 'utf-16le':
                return UCS2;
            case 'hex':
                return HEX;
            case 'base64':
                return BASE64;

            case 'binary_string':
                return BINSTR;
            case 'binary_string_ie':
                return BINSTRIE;
            case 'extended_ascii':
                return ExtendedASCII;

            default:
                throw new Error("Unknown encoding: " + encoding);
        }
    }
    exports.FindUtil = FindUtil;

    /**
    * String utility functions for UTF-8. Note that some UTF-8 strings *cannot* be
    * expressed in terms of JavaScript UTF-16 strings.
    * @see http://en.wikipedia.org/wiki/UTF-8
    */
    var UTF8 = (function () {
        function UTF8() {
        }
        UTF8.str2byte = function (str, buf) {
            var length = buf.length;
            var i = 0;
            var j = 0;
            var maxJ = length;
            var rv = [];
            var numChars = 0;
            while (i < str.length && j < maxJ) {
                var code = str.charCodeAt(i++);
                var next = str.charCodeAt(i);
                if (0xD800 <= code && code <= 0xDBFF && 0xDC00 <= next && next <= 0xDFFF) {
                    // 4 bytes: Surrogate pairs! UTF-16 fun time.
                    if (j + 3 >= maxJ) {
                        break;
                    } else {
                        numChars++;
                    }

                    // First pair: 10 bits of data, with an implicitly set 11th bit
                    // Second pair: 10 bits of data
                    var codePoint = (((code & 0x3FF) | 0x400) << 10) | (next & 0x3FF);

                    // Highest 3 bits in first byte
                    buf.writeUInt8((codePoint >> 18) | 0xF0, j++);

                    // Rest are all 6 bits
                    buf.writeUInt8(((codePoint >> 12) & 0x3F) | 0x80, j++);
                    buf.writeUInt8(((codePoint >> 6) & 0x3F) | 0x80, j++);
                    buf.writeUInt8((codePoint & 0x3F) | 0x80, j++);
                    i++;
                } else if (code < 0x80) {
                    // One byte
                    buf.writeUInt8(code, j++);
                    numChars++;
                } else if (code < 0x800) {
                    // Two bytes
                    if (j + 1 >= maxJ) {
                        break;
                    } else {
                        numChars++;
                    }

                    // Highest 5 bits in first byte
                    buf.writeUInt8((code >> 6) | 0xC0, j++);

                    // Lower 6 bits in second byte
                    buf.writeUInt8((code & 0x3F) | 0x80, j++);
                } else if (code < 0x10000) {
                    // Three bytes
                    if (j + 2 >= maxJ) {
                        break;
                    } else {
                        numChars++;
                    }

                    // Highest 4 bits in first byte
                    buf.writeUInt8((code >> 12) | 0xE0, j++);

                    // Middle 6 bits in second byte
                    buf.writeUInt8(((code >> 6) & 0x3F) | 0x80, j++);

                    // Lowest 6 bits in third byte
                    buf.writeUInt8((code & 0x3F) | 0x80, j++);
                }
            }
            return j;
        };

        UTF8.byte2str = function (buff) {
            var chars = [];
            var i = 0;
            while (i < buff.length) {
                var code = buff.readUInt8(i++);
                if (code < 0x80) {
                    chars.push(String.fromCharCode(code));
                } else if (code < 0xC0) {
                    throw new Error('Found incomplete part of character in string.');
                } else if (code < 0xE0) {
                    // 2 bytes: 5 and 6 bits
                    chars.push(String.fromCharCode(((code & 0x1F) << 6) | (buff.readUInt8(i++) & 0x3F)));
                } else if (code < 0xF0) {
                    // 3 bytes: 4, 6, and 6 bits
                    chars.push(String.fromCharCode(((code & 0xF) << 12) | ((buff.readUInt8(i++) & 0x3F) << 6) | (buff.readUInt8(i++) & 0x3F)));
                } else if (code < 0xF8) {
                    // 4 bytes: 3, 6, 6, 6 bits; surrogate pairs time!
                    // First 11 bits; remove 11th bit as per UTF-16 standard
                    var byte3 = buff.readUInt8(i + 2);
                    chars.push(String.fromCharCode(((((code & 0x7) << 8) | ((buff.readUInt8(i++) & 0x3F) << 2) | ((buff.readUInt8(i++) & 0x3F) >> 4)) & 0x3FF) | 0xD800));

                    // Final 10 bits
                    chars.push(String.fromCharCode((((byte3 & 0xF) << 6) | (buff.readUInt8(i++) & 0x3F)) | 0xDC00));
                } else {
                    throw new Error('Unable to represent UTF-8 string as UTF-16 JavaScript string.');
                }
            }
            return chars.join('');
        };

        UTF8.byteLength = function (str) {
            // Matches only the 10.. bytes that are non-initial characters in a
            // multi-byte sequence.
            // @todo This may be slower than iterating through the string in some cases.
            var m = encodeURIComponent(str).match(/%[89ABab]/g);
            return str.length + (m ? m.length : 0);
        };
        return UTF8;
    })();
    exports.UTF8 = UTF8;

    /**
    * String utility functions for 8-bit ASCII. Like Node, we mask the high bits of
    * characters in JavaScript UTF-16 strings.
    * @see http://en.wikipedia.org/wiki/ASCII
    */
    var ASCII = (function () {
        function ASCII() {
        }
        ASCII.str2byte = function (str, buf) {
            var length = str.length > buf.length ? buf.length : str.length;
            for (var i = 0; i < length; i++) {
                buf.writeUInt8(str.charCodeAt(i) % 256, i);
            }
            return length;
        };

        ASCII.byte2str = function (buff) {
            var chars = new Array(buff.length);
            for (var i = 0; i < buff.length; i++) {
                chars[i] = String.fromCharCode(buff.readUInt8(i) & 0x7F);
            }
            return chars.join('');
        };

        ASCII.byteLength = function (str) {
            return str.length;
        };
        return ASCII;
    })();
    exports.ASCII = ASCII;

    /**
    * (Nonstandard) String utility function for 8-bit ASCII with the extended
    * character set. Unlike the ASCII above, we do not mask the high bits.
    * @see http://en.wikipedia.org/wiki/Extended_ASCII
    */
    var ExtendedASCII = (function () {
        function ExtendedASCII() {
        }
        ExtendedASCII.str2byte = function (str, buf) {
            var length = str.length > buf.length ? buf.length : str.length;
            for (var i = 0; i < length; i++) {
                var charCode = str.charCodeAt(i);
                if (charCode > 0x7F) {
                    // Check if extended ASCII.
                    var charIdx = ExtendedASCII.extendedChars.indexOf(str.charAt(i));
                    if (charIdx > -1) {
                        charCode = charIdx + 0x80;
                    }
                    // Otherwise, keep it as-is.
                }
                buf.writeUInt8(charCode, i);
            }
            return length;
        };

        ExtendedASCII.byte2str = function (buff) {
            var chars = new Array(buff.length);
            for (var i = 0; i < buff.length; i++) {
                var charCode = buff.readUInt8(i);
                if (charCode > 0x7F) {
                    chars[i] = ExtendedASCII.extendedChars[charCode - 128];
                } else {
                    chars[i] = String.fromCharCode(charCode);
                }
            }
            return chars.join('');
        };

        ExtendedASCII.byteLength = function (str) {
            return str.length;
        };
        ExtendedASCII.extendedChars = [
            '\u00C7', '\u00FC', '\u00E9', '\u00E2', '\u00E4',
            '\u00E0', '\u00E5', '\u00E7', '\u00EA', '\u00EB', '\u00E8', '\u00EF',
            '\u00EE', '\u00EC', '\u00C4', '\u00C5', '\u00C9', '\u00E6', '\u00C6',
            '\u00F4', '\u00F6', '\u00F2', '\u00FB', '\u00F9', '\u00FF', '\u00D6',
            '\u00DC', '\u00F8', '\u00A3', '\u00D8', '\u00D7', '\u0192', '\u00E1',
            '\u00ED', '\u00F3', '\u00FA', '\u00F1', '\u00D1', '\u00AA', '\u00BA',
            '\u00BF', '\u00AE', '\u00AC', '\u00BD', '\u00BC', '\u00A1', '\u00AB',
            '\u00BB', '_', '_', '_', '\u00A6', '\u00A6', '\u00C1', '\u00C2', '\u00C0',
            '\u00A9', '\u00A6', '\u00A6', '+', '+', '\u00A2', '\u00A5', '+', '+', '-',
            '-', '+', '-', '+', '\u00E3', '\u00C3', '+', '+', '-', '-', '\u00A6', '-',
            '+', '\u00A4', '\u00F0', '\u00D0', '\u00CA', '\u00CB', '\u00C8', 'i',
            '\u00CD', '\u00CE', '\u00CF', '+', '+', '_', '_', '\u00A6', '\u00CC', '_',
            '\u00D3', '\u00DF', '\u00D4', '\u00D2', '\u00F5', '\u00D5', '\u00B5',
            '\u00FE', '\u00DE', '\u00DA', '\u00DB', '\u00D9', '\u00FD', '\u00DD',
            '\u00AF', '\u00B4', '\u00AD', '\u00B1', '_', '\u00BE', '\u00B6', '\u00A7',
            '\u00F7', '\u00B8', '\u00B0', '\u00A8', '\u00B7', '\u00B9', '\u00B3',
            '\u00B2', '_', ' '];
        return ExtendedASCII;
    })();
    exports.ExtendedASCII = ExtendedASCII;

    /**
    * String utility functions for Node's BINARY strings, which represent a single
    * byte per character.
    */
    var BINARY = (function () {
        function BINARY() {
        }
        BINARY.str2byte = function (str, buf) {
            var length = str.length > buf.length ? buf.length : str.length;
            for (var i = 0; i < length; i++) {
                buf.writeUInt8(str.charCodeAt(i) & 0xFF, i);
            }
            return length;
        };

        BINARY.byte2str = function (buff) {
            var chars = new Array(buff.length);
            for (var i = 0; i < buff.length; i++) {
                chars[i] = String.fromCharCode(buff.readUInt8(i) & 0xFF);
            }
            return chars.join('');
        };

        BINARY.byteLength = function (str) {
            return str.length;
        };
        return BINARY;
    })();
    exports.BINARY = BINARY;

    /**
    * Contains string utility functions for base-64 encoding.
    *
    * Adapted from the StackOverflow comment linked below.
    * @see http://stackoverflow.com/questions/246801/how-can-you-encode-to-base64-using-javascript#246813
    * @see http://en.wikipedia.org/wiki/Base64
    * @todo Bake in support for btoa() and atob() if available.
    */
    var BASE64 = (function () {
        function BASE64() {
        }
        BASE64.byte2str = function (buff) {
            var output = '';
            var i = 0;
            while (i < buff.length) {
                var chr1 = buff.readUInt8(i++);
                var chr2 = i < buff.length ? buff.readUInt8(i++) : NaN;
                var chr3 = i < buff.length ? buff.readUInt8(i++) : NaN;
                var enc1 = chr1 >> 2;
                var enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
                var enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
                var enc4 = chr3 & 63;
                if (isNaN(chr2)) {
                    enc3 = enc4 = 64;
                } else if (isNaN(chr3)) {
                    enc4 = 64;
                }
                output = output + BASE64.num2b64[enc1] + BASE64.num2b64[enc2] + BASE64.num2b64[enc3] + BASE64.num2b64[enc4];
            }
            return output;
        };

        BASE64.str2byte = function (str, buf) {
            var length = buf.length;
            var output = '';
            var i = 0;
            str = str.replace(/[^A-Za-z0-9\+\/\=\-\_]/g, '');
            var j = 0;
            while (i < str.length) {
                var enc1 = BASE64.b642num[str.charAt(i++)];
                var enc2 = BASE64.b642num[str.charAt(i++)];
                var enc3 = BASE64.b642num[str.charAt(i++)];
                var enc4 = BASE64.b642num[str.charAt(i++)];
                var chr1 = (enc1 << 2) | (enc2 >> 4);
                var chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
                var chr3 = ((enc3 & 3) << 6) | enc4;
                buf.writeUInt8(chr1, j++);
                if (j === length) {
                    break;
                }
                if (enc3 !== 64) {
                    output += buf.writeUInt8(chr2, j++);
                }
                if (j === length) {
                    break;
                }
                if (enc4 !== 64) {
                    output += buf.writeUInt8(chr3, j++);
                }
                if (j === length) {
                    break;
                }
            }
            return j;
        };

        BASE64.byteLength = function (str) {
            return Math.floor(((str.replace(/[^A-Za-z0-9\+\/\-\_]/g, '')).length * 6) / 8);
        };
        BASE64.b64chars = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '='];
        BASE64.num2b64 = (function () {
            var obj = new Array(BASE64.b64chars.length);
            for (var idx = 0; idx < BASE64.b64chars.length; idx++) {
                var i = BASE64.b64chars[idx];
                obj[idx] = i;
            }
            return obj;
        })();

        BASE64.b642num = (function () {
            var obj = {};
            for (var idx = 0; idx < BASE64.b64chars.length; idx++) {
                var i = BASE64.b64chars[idx];
                obj[i] = idx;
            }
            obj['-'] = 62;
            obj['_'] = 63;
            return obj;
        })();
        return BASE64;
    })();
    exports.BASE64 = BASE64;

    /**
    * String utility functions for the UCS-2 encoding. Note that our UCS-2 handling
    * is identical to our UTF-16 handling.
    *
    * Note: UCS-2 handling is identical to UTF-16.
    * @see http://en.wikipedia.org/wiki/UCS2
    */
    var UCS2 = (function () {
        function UCS2() {
        }
        UCS2.str2byte = function (str, buf) {
            var len = str.length;

            // Clip length to longest string of valid characters that can fit in the
            // byte range.
            if (len * 2 > buf.length) {
                len = buf.length % 2 === 1 ? (buf.length - 1) / 2 : buf.length / 2;
            }
            for (var i = 0; i < len; i++) {
                buf.writeUInt16LE(str.charCodeAt(i), i * 2);
            }
            return len * 2;
        };

        UCS2.byte2str = function (buff) {
            if (buff.length % 2 !== 0) {
                throw new Error('Invalid UCS2 byte array.');
            }
            var chars = new Array(buff.length / 2);
            for (var i = 0; i < buff.length; i += 2) {
                chars[i / 2] = String.fromCharCode(buff.readUInt8(i) | (buff.readUInt8(i + 1) << 8));
            }
            return chars.join('');
        };

        UCS2.byteLength = function (str) {
            return str.length * 2;
        };
        return UCS2;
    })();
    exports.UCS2 = UCS2;

    /**
    * Contains string utility functions for hex encoding.
    * @see http://en.wikipedia.org/wiki/Hexadecimal
    */
    var HEX = (function () {
        function HEX() {
        }
        HEX.str2byte = function (str, buf) {
            if (str.length % 2 === 1) {
                throw new Error('Invalid hex string');
            }

            // Each character is 1 byte encoded as two hex characters; so 1 byte becomes
            // 2 bytes.
            var numBytes = str.length >> 1;
            if (numBytes > buf.length) {
                numBytes = buf.length;
            }
            for (var i = 0; i < numBytes; i++) {
                var char1 = this.hex2num[str.charAt(i << 1)];
                var char2 = this.hex2num[str.charAt((i << 1) + 1)];
                buf.writeUInt8((char1 << 4) | char2, i);
            }
            return numBytes;
        };

        HEX.byte2str = function (buff) {
            var len = buff.length;
            var chars = new Array(len << 1);
            var j = 0;
            for (var i = 0; i < len; i++) {
                var hex2 = buff.readUInt8(i) & 0xF;
                var hex1 = buff.readUInt8(i) >> 4;
                chars[j++] = this.num2hex[hex1];
                chars[j++] = this.num2hex[hex2];
            }
            return chars.join('');
        };

        HEX.byteLength = function (str) {
            // Assuming a valid string.
            return str.length >> 1;
        };
        HEX.HEXCHARS = '0123456789abcdef';

        HEX.num2hex = (function () {
            var obj = new Array(HEX.HEXCHARS.length);
            for (var idx = 0; idx < HEX.HEXCHARS.length; idx++) {
                var i = HEX.HEXCHARS[idx];
                obj[idx] = i;
            }
            return obj;
        })();

        HEX.hex2num = (function () {
            var idx, i;
            var obj = {};
            for (idx = 0; idx < HEX.HEXCHARS.length; idx++) {
                i = HEX.HEXCHARS[idx];
                obj[i] = idx;
            }
            var capitals = 'ABCDEF';
            for (idx = 0; idx < capitals.length; idx++) {
                i = capitals[idx];
                obj[i] = idx + 10;
            }
            return obj;
        })();
        return HEX;
    })();
    exports.HEX = HEX;

    /**
    * Contains string utility functions for binary string encoding. This is where we
    * pack arbitrary binary data as a UTF-16 string.
    *
    * Each character in the string is two bytes. The first character in the string
    * is special: The first byte specifies if the binary data is of odd byte length.
    * If it is, then it is a 1 and the second byte is the first byte of data; if
    * not, it is a 0 and the second byte is 0.
    *
    * Everything is little endian.
    */
    var BINSTR = (function () {
        function BINSTR() {
        }
        BINSTR.str2byte = function (str, buf) {
            // Special case: Empty string
            if (str.length === 0) {
                return 0;
            }
            var numBytes = BINSTR.byteLength(str);
            if (numBytes > buf.length) {
                numBytes = buf.length;
            }
            var j = 0;
            var startByte = 0;
            var endByte = startByte + numBytes;

            // Handle first character separately
            var firstChar = str.charCodeAt(j++);
            if (firstChar !== 0) {
                buf.writeUInt8(firstChar & 0xFF, 0);
                startByte = 1;
            }
            for (var i = startByte; i < endByte; i += 2) {
                var chr = str.charCodeAt(j++);
                if (endByte - i === 1) {
                    // Write first byte of character
                    buf.writeUInt8(chr >> 8, i);
                }
                if (endByte - i >= 2) {
                    // Write both bytes in character
                    buf.writeUInt16BE(chr, i);
                }
            }
            return numBytes;
        };

        BINSTR.byte2str = function (buff) {
            var len = buff.length;

            // Special case: Empty string
            if (len === 0) {
                return '';
            }
            var chars = new Array((len >> 1) + 1);
            var j = 0;
            for (var i = 0; i < chars.length; i++) {
                if (i === 0) {
                    if (len % 2 === 1) {
                        chars[i] = String.fromCharCode((1 << 8) | buff.readUInt8(j++));
                    } else {
                        chars[i] = String.fromCharCode(0);
                    }
                } else {
                    chars[i] = String.fromCharCode((buff.readUInt8(j++) << 8) | buff.readUInt8(j++));
                }
            }
            return chars.join('');
        };

        BINSTR.byteLength = function (str) {
            if (str.length === 0) {
                // Special case: Empty string.
                return 0;
            }
            var firstChar = str.charCodeAt(0);
            var bytelen = (str.length - 1) << 1;
            if (firstChar !== 0) {
                bytelen++;
            }
            return bytelen;
        };
        return BINSTR;
    })();
    exports.BINSTR = BINSTR;

    /**
    * IE/older FF version of binary string. One byte per character, offset by 0x20.
    */
    var BINSTRIE = (function () {
        function BINSTRIE() {
        }
        BINSTRIE.str2byte = function (str, buf) {
            var length = str.length > buf.length ? buf.length : str.length;
            for (var i = 0; i < length; i++) {
                buf.writeUInt8(str.charCodeAt(i) - 0x20, i);
            }
            return length;
        };

        BINSTRIE.byte2str = function (buff) {
            var chars = new Array(buff.length);
            for (var i = 0; i < buff.length; i++) {
                chars[i] = String.fromCharCode(buff.readUInt8(i) + 0x20);
            }
            return chars.join('');
        };

        BINSTRIE.byteLength = function (str) {
            return str.length;
        };
        return BINSTRIE;
    })();
    exports.BINSTRIE = BINSTRIE;
});
//# sourceMappingURL=string_util.js.map
;
define('core/buffer',["require", "exports", './buffer_core', './buffer_core_array', './buffer_core_arraybuffer', './buffer_core_imagedata', './string_util'], function(require, exports, buffer_core, buffer_core_array, buffer_core_arraybuffer, buffer_core_imagedata, string_util) {
    // BC implementations earlier in the array are preferred.
    var BufferCorePreferences = [
        buffer_core_arraybuffer.BufferCoreArrayBuffer,
        buffer_core_imagedata.BufferCoreImageData,
        buffer_core_array.BufferCoreArray
    ];

    var PreferredBufferCore = (function () {
        var i, bci;
        for (i = 0; i < BufferCorePreferences.length; i++) {
            bci = BufferCorePreferences[i];
            if (bci.isAvailable())
                return bci;
        }

        throw new Error("This browser does not support any available BufferCore implementations.");
    })();

    

    

    /**
    * Emulates Node's Buffer API. Wraps a BufferCore object that is responsible
    * for actually writing/reading data from some data representation in memory.
    */
    var Buffer = (function () {
        function Buffer(arg1, arg2, arg3) {
            if (typeof arg2 === "undefined") { arg2 = 'utf8'; }
            this.offset = 0;
            var i;

            // Node apparently allows you to construct buffers w/o 'new'.
            if (!(this instanceof Buffer)) {
                return new Buffer(arg1, arg2);
            }

            if (arg1 instanceof buffer_core.BufferCoreCommon) {
                // constructor (data: buffer_core.BufferCore, start?: number, end?: number)
                this.data = arg1;
                var start = typeof arg2 === 'number' ? arg2 : 0;
                var end = typeof arg3 === 'number' ? arg3 : this.data.getLength();
                this.offset = start;
                this.length = end - start;
            } else if (typeof arg1 === 'number') {
                // constructor (size: number);
                if (arg1 !== (arg1 >>> 0)) {
                    throw new TypeError('Buffer size must be a uint32.');
                }
                this.length = arg1;
                this.data = new PreferredBufferCore(arg1);
            } else if (typeof DataView !== 'undefined' && arg1 instanceof DataView) {
                // constructor (data: DataView);
                this.data = new buffer_core_arraybuffer.BufferCoreArrayBuffer(arg1);
                this.length = arg1.byteLength;
            } else if (typeof ArrayBuffer !== 'undefined' && arg1 instanceof ArrayBuffer) {
                // constructor (data: ArrayBuffer);
                this.data = new buffer_core_arraybuffer.BufferCoreArrayBuffer(arg1);
                this.length = arg1.byteLength;
            } else if (arg1 instanceof Buffer) {
                // constructor (data: Buffer);
                var argBuff = arg1;
                this.data = new PreferredBufferCore(arg1.length);
                this.length = arg1.length;
                argBuff.copy(this);
            } else if (Array.isArray(arg1) || (arg1 != null && typeof arg1 === 'object' && typeof arg1[0] === 'number')) {
                // constructor (data: number[]);
                this.data = new PreferredBufferCore(arg1.length);
                for (i = 0; i < arg1.length; i++) {
                    this.data.writeUInt8(i, arg1[i]);
                }
                this.length = arg1.length;
            } else if (typeof arg1 === 'string') {
                // constructor (data: string, encoding?: string);
                this.length = Buffer.byteLength(arg1, arg2);
                this.data = new PreferredBufferCore(this.length);
                this.write(arg1, 0, this.length, arg2);
            } else {
                throw new Error("Invalid argument to Buffer constructor: " + arg1);
            }
        }
        Buffer.prototype.getBufferCore = function () {
            return this.data;
        };

        Buffer.prototype.getOffset = function () {
            return this.offset;
        };

        /**
        * **NONSTANDARD**: Set the octet at index. Emulates NodeJS buffer's index
        * operation. Octet can be signed or unsigned.
        * @param {number} index - the index to set the value at
        * @param {number} value - the value to set at the given index
        */
        Buffer.prototype.set = function (index, value) {
            // In Node, the following happens:
            // buffer[0] = -1;
            // buffer[0]; // 255
            if (value < 0) {
                return this.writeInt8(value, index);
            } else {
                return this.writeUInt8(value, index);
            }
        };

        /**
        * **NONSTANDARD**: Get the octet at index.
        * @param {number} index - index to fetch the value at
        * @return {number} the value at the given index
        */
        Buffer.prototype.get = function (index) {
            return this.readUInt8(index);
        };

        /**
        * Writes string to the buffer at offset using the given encoding.
        * If buffer did not contain enough space to fit the entire string, it will
        * write a partial amount of the string.
        * @param {string} str - Data to be written to buffer
        * @param {number} [offset=0] - Offset in the buffer to write to
        * @param {number} [length=this.length] - Number of bytes to write
        * @param {string} [encoding=utf8] - Character encoding
        * @return {number} Number of octets written.
        */
        Buffer.prototype.write = function (str, offset, length, encoding) {
            if (typeof offset === "undefined") { offset = 0; }
            if (typeof length === "undefined") { length = this.length; }
            if (typeof encoding === "undefined") { encoding = 'utf8'; }
            // I hate Node's optional arguments.
            if (typeof offset === 'string') {
                // 'str' and 'encoding' specified
                encoding = "" + offset;
                offset = 0;
                length = this.length;
            } else if (typeof length === 'string') {
                // 'str', 'offset', and 'encoding' specified
                encoding = "" + length;
                length = this.length;
            }

            // Don't waste our time if the offset is beyond the buffer length
            if (offset >= this.length) {
                return 0;
            }
            var strUtil = string_util.FindUtil(encoding);

            // Are we trying to write past the buffer?
            length = length + offset > this.length ? this.length - offset : length;
            offset += this.offset;
            return strUtil.str2byte(str, offset === 0 && length === this.length ? this : new Buffer(this.data, offset, length + offset));
        };

        /**
        * Decodes a portion of the Buffer into a String.
        * @param {string} encoding - Character encoding to decode to
        * @param {number} [start=0] - Start position in the buffer
        * @param {number} [end=this.length] - Ending position in the buffer
        * @return {string} A string from buffer data encoded with encoding, beginning
        *   at start, and ending at end.
        */
        Buffer.prototype.toString = function (encoding, start, end) {
            if (typeof encoding === "undefined") { encoding = 'utf8'; }
            if (typeof start === "undefined") { start = 0; }
            if (typeof end === "undefined") { end = this.length; }
            if (!(start <= end)) {
                throw new Error("Invalid start/end positions: " + start + " - " + end);
            }
            if (start === end) {
                return '';
            }
            if (end > this.length) {
                end = this.length;
            }
            var strUtil = string_util.FindUtil(encoding);

            // Get the string representation of the given slice. Create a new buffer
            // if need be.
            return strUtil.byte2str(start === 0 && end === this.length ? this : new Buffer(this.data, start + this.offset, end + this.offset));
        };

        /**
        * Returns a JSON-representation of the Buffer instance, which is identical to
        * the output for JSON Arrays. JSON.stringify implicitly calls this function
        * when stringifying a Buffer instance.
        * @return {object} An object that can be used for JSON stringification.
        */
        Buffer.prototype.toJSON = function () {
            // Construct a byte array for the JSON 'data'.
            var len = this.length;
            var byteArr = new Array(len);
            for (var i = 0; i < len; i++) {
                byteArr[i] = this.readUInt8(i);
            }
            return {
                type: 'Buffer',
                data: byteArr
            };
        };

        /**
        * Does copy between buffers. The source and target regions can be overlapped.
        * All values passed that are undefined/NaN or are out of bounds are set equal
        * to their respective defaults.
        * @param {Buffer} target - Buffer to copy into
        * @param {number} [targetStart=0] - Index to start copying to in the targetBuffer
        * @param {number} [sourceStart=0] - Index in this buffer to start copying from
        * @param {number} [sourceEnd=this.length] - Index in this buffer stop copying at
        * @return {number} The number of bytes copied into the target buffer.
        */
        Buffer.prototype.copy = function (target, targetStart, sourceStart, sourceEnd) {
            if (typeof targetStart === "undefined") { targetStart = 0; }
            if (typeof sourceStart === "undefined") { sourceStart = 0; }
            if (typeof sourceEnd === "undefined") { sourceEnd = this.length; }
            // The Node code is weird. It sets some out-of-bounds args to their defaults
            // and throws exceptions for others (sourceEnd).
            targetStart = targetStart < 0 ? 0 : targetStart;
            sourceStart = sourceStart < 0 ? 0 : sourceStart;

            // Need to sanity check all of the input. Node has really odd rules regarding
            // when to apply default arguments. I decided to copy Node's logic.
            if (sourceEnd < sourceStart) {
                throw new RangeError('sourceEnd < sourceStart');
            }
            if (sourceEnd === sourceStart) {
                return 0;
            }
            if (targetStart >= target.length) {
                throw new RangeError('targetStart out of bounds');
            }
            if (sourceStart >= this.length) {
                throw new RangeError('sourceStart out of bounds');
            }
            if (sourceEnd > this.length) {
                throw new RangeError('sourceEnd out of bounds');
            }
            var bytesCopied = Math.min(sourceEnd - sourceStart, target.length - targetStart, this.length - sourceStart);

            for (var i = 0; i < bytesCopied; i++) {
                target.writeUInt8(this.readUInt8(sourceStart + i), targetStart + i);
            }
            return bytesCopied;
        };

        /**
        * Returns a slice of this buffer.
        * @param {number} [start=0] - Index to start slicing from
        * @param {number} [end=this.length] - Index to stop slicing at
        * @return {Buffer} A new buffer which references the same
        *   memory as the old, but offset and cropped by the start (defaults to 0) and
        *   end (defaults to buffer.length) indexes. Negative indexes start from the end
        *   of the buffer.
        */
        Buffer.prototype.slice = function (start, end) {
            if (typeof start === "undefined") { start = 0; }
            if (typeof end === "undefined") { end = this.length; }
            // Translate negative indices to positive ones.
            if (start < 0) {
                start += this.length;
                if (start < 0) {
                    start = 0;
                }
            }
            if (end < 0) {
                end += this.length;
                if (end < 0) {
                    end = 0;
                }
            }
            if (end > this.length) {
                end = this.length;
            }
            if (start > end) {
                start = end;
            }

            // Sanity check.
            if (start < 0 || end < 0 || start >= this.length || end > this.length) {
                throw new Error("Invalid slice indices.");
            }

            // Create a new buffer backed by the same BufferCore.
            return new Buffer(this.data, start + this.offset, end + this.offset);
        };

        /**
        * [NONSTANDARD] A copy-based version of Buffer.slice.
        */
        Buffer.prototype.sliceCopy = function (start, end) {
            if (typeof start === "undefined") { start = 0; }
            if (typeof end === "undefined") { end = this.length; }
            // Translate negative indices to positive ones.
            if (start < 0) {
                start += this.length;
                if (start < 0) {
                    start = 0;
                }
            }
            if (end < 0) {
                end += this.length;
                if (end < 0) {
                    end = 0;
                }
            }
            if (end > this.length) {
                end = this.length;
            }
            if (start > end) {
                start = end;
            }

            // Sanity check.
            if (start < 0 || end < 0 || start >= this.length || end > this.length) {
                throw new Error("Invalid slice indices.");
            }

            // Copy the BufferCore.
            return new Buffer(this.data.copy(start + this.offset, end + this.offset));
        };

        /**
        * Fills the buffer with the specified value. If the offset and end are not
        * given it will fill the entire buffer.
        * @param {(string|number)} value - The value to fill the buffer with
        * @param {number} [offset=0]
        * @param {number} [end=this.length]
        */
        Buffer.prototype.fill = function (value, offset, end) {
            if (typeof offset === "undefined") { offset = 0; }
            if (typeof end === "undefined") { end = this.length; }
            var i;
            var valType = typeof value;
            switch (valType) {
                case "string":
                    // Trim to a byte.
                    value = value.charCodeAt(0) & 0xFF;
                    break;
                case "number":
                    break;
                default:
                    throw new Error('Invalid argument to fill.');
            }
            offset += this.offset;
            end += this.offset;
            this.data.fill(value, offset, end);
        };

        Buffer.prototype.readUInt8 = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readUInt8(offset);
        };

        Buffer.prototype.readUInt16LE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readUInt16LE(offset);
        };

        Buffer.prototype.readUInt16BE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readUInt16BE(offset);
        };

        Buffer.prototype.readUInt32LE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readUInt32LE(offset);
        };

        Buffer.prototype.readUInt32BE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readUInt32BE(offset);
        };

        Buffer.prototype.readInt8 = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readInt8(offset);
        };

        Buffer.prototype.readInt16LE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readInt16LE(offset);
        };

        Buffer.prototype.readInt16BE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readInt16BE(offset);
        };

        Buffer.prototype.readInt32LE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readInt32LE(offset);
        };

        Buffer.prototype.readInt32BE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readInt32BE(offset);
        };

        Buffer.prototype.readFloatLE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readFloatLE(offset);
        };

        Buffer.prototype.readFloatBE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readFloatBE(offset);
        };

        Buffer.prototype.readDoubleLE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readDoubleLE(offset);
        };

        Buffer.prototype.readDoubleBE = function (offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            return this.data.readDoubleBE(offset);
        };

        Buffer.prototype.writeUInt8 = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeUInt8(offset, value);
        };

        Buffer.prototype.writeUInt16LE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeUInt16LE(offset, value);
        };

        Buffer.prototype.writeUInt16BE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeUInt16BE(offset, value);
        };

        Buffer.prototype.writeUInt32LE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeUInt32LE(offset, value);
        };

        Buffer.prototype.writeUInt32BE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeUInt32BE(offset, value);
        };

        Buffer.prototype.writeInt8 = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeInt8(offset, value);
        };

        Buffer.prototype.writeInt16LE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeInt16LE(offset, value);
        };

        Buffer.prototype.writeInt16BE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeInt16BE(offset, value);
        };

        Buffer.prototype.writeInt32LE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeInt32LE(offset, value);
        };

        Buffer.prototype.writeInt32BE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeInt32BE(offset, value);
        };

        Buffer.prototype.writeFloatLE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeFloatLE(offset, value);
        };

        Buffer.prototype.writeFloatBE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeFloatBE(offset, value);
        };

        Buffer.prototype.writeDoubleLE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeDoubleLE(offset, value);
        };

        Buffer.prototype.writeDoubleBE = function (value, offset, noAssert) {
            if (typeof noAssert === "undefined") { noAssert = false; }
            offset += this.offset;
            this.data.writeDoubleBE(offset, value);
        };

        ///**************************STATIC METHODS********************************///
        /**
        * Checks if enc is a valid string encoding type.
        * @param {string} enc - Name of a string encoding type.
        * @return {boolean} Whether or not enc is a valid encoding type.
        */
        Buffer.isEncoding = function (enc) {
            try  {
                string_util.FindUtil(enc);
            } catch (e) {
                return false;
            }
            return true;
        };

        /**
        * Tests if obj is a Buffer.
        * @param {object} obj - An arbitrary object
        * @return {boolean} True if this object is a Buffer.
        */
        Buffer.isBuffer = function (obj) {
            return obj instanceof Buffer;
        };

        /**
        * Gives the actual byte length of a string. This is not the same as
        * String.prototype.length since that returns the number of characters in a
        * string.
        * @param {string} str - The string to get the byte length of
        * @param {string} [encoding=utf8] - Character encoding of the string
        * @return {number} The number of bytes in the string
        */
        Buffer.byteLength = function (str, encoding) {
            if (typeof encoding === "undefined") { encoding = 'utf8'; }
            var strUtil = string_util.FindUtil(encoding);
            return strUtil.byteLength(str);
        };

        /**
        * Returns a buffer which is the result of concatenating all the buffers in the
        * list together.
        * If the list has no items, or if the totalLength is 0, then it returns a
        * zero-length buffer.
        * If the list has exactly one item, then the first item of the list is
        * returned.
        * If the list has more than one item, then a new Buffer is created.
        * If totalLength is not provided, it is read from the buffers in the list.
        * However, this adds an additional loop to the function, so it is faster to
        * provide the length explicitly.
        * @param {Buffer[]} list - List of Buffer objects to concat
        * @param {number} [totalLength] - Total length of the buffers when concatenated
        * @return {Buffer}
        */
        Buffer.concat = function (list, totalLength) {
            var item;
            if (list.length === 0 || totalLength === 0) {
                return new Buffer(0);
            } else if (list.length === 1) {
                return list[0];
            } else {
                if (totalLength == null) {
                    // Calculate totalLength
                    totalLength = 0;
                    for (var i = 0; i < list.length; i++) {
                        item = list[i];
                        totalLength += item.length;
                    }
                }
                var buf = new Buffer(totalLength);
                var curPos = 0;
                for (var j = 0; j < list.length; j++) {
                    item = list[j];
                    curPos += item.copy(buf, curPos);
                }
                return buf;
            }
        };
        return Buffer;
    })();
    exports.Buffer = Buffer;

    // Type-check the class.
    var _ = Buffer;
});
//# sourceMappingURL=buffer.js.map
;
define('core/file_flag',["require", "exports", './api_error'], function(require, exports, api_error) {
    /**
    * @class
    */
    (function (ActionType) {
        // Indicates that the code should not do anything.
        ActionType[ActionType["NOP"] = 0] = "NOP";

        // Indicates that the code should throw an exception.
        ActionType[ActionType["THROW_EXCEPTION"] = 1] = "THROW_EXCEPTION";

        // Indicates that the code should truncate the file, but only if it is a file.
        ActionType[ActionType["TRUNCATE_FILE"] = 2] = "TRUNCATE_FILE";

        // Indicates that the code should create the file.
        ActionType[ActionType["CREATE_FILE"] = 3] = "CREATE_FILE";
    })(exports.ActionType || (exports.ActionType = {}));
    var ActionType = exports.ActionType;

    /**
    * Represents one of the following file flags. A convenience object.
    *
    * * `'r'` - Open file for reading. An exception occurs if the file does not exist.
    * * `'r+'` - Open file for reading and writing. An exception occurs if the file does not exist.
    * * `'rs'` - Open file for reading in synchronous mode. Instructs the filesystem to not cache writes.
    * * `'rs+'` - Open file for reading and writing, and opens the file in synchronous mode.
    * * `'w'` - Open file for writing. The file is created (if it does not exist) or truncated (if it exists).
    * * `'wx'` - Like 'w' but opens the file in exclusive mode.
    * * `'w+'` - Open file for reading and writing. The file is created (if it does not exist) or truncated (if it exists).
    * * `'wx+'` - Like 'w+' but opens the file in exclusive mode.
    * * `'a'` - Open file for appending. The file is created if it does not exist.
    * * `'ax'` - Like 'a' but opens the file in exclusive mode.
    * * `'a+'` - Open file for reading and appending. The file is created if it does not exist.
    * * `'ax+'` - Like 'a+' but opens the file in exclusive mode.
    *
    * Exclusive mode ensures that the file path is newly created.
    * @class
    */
    var FileFlag = (function () {
        /**
        * This should never be called directly.
        * @param [String] modeStr The string representing the mode
        * @throw [BrowserFS.ApiError] when the mode string is invalid
        */
        function FileFlag(flagStr) {
            this.flagStr = flagStr;
            if (FileFlag.validFlagStrs.indexOf(flagStr) < 0) {
                throw new api_error.ApiError(9 /* EINVAL */, "Invalid flag: " + flagStr);
            }
        }
        /**
        * Get an object representing the given file mode.
        * @param [String] modeStr The string representing the mode
        * @return [BrowserFS.FileMode] The FileMode object representing the mode
        * @throw [BrowserFS.ApiError] when the mode string is invalid
        */
        FileFlag.getFileFlag = function (flagStr) {
            // Check cache first.
            if (FileFlag.flagCache.hasOwnProperty(flagStr)) {
                return FileFlag.flagCache[flagStr];
            }
            return FileFlag.flagCache[flagStr] = new FileFlag(flagStr);
        };

        /**
        * Returns true if the file is readable.
        * @return [Boolean]
        */
        FileFlag.prototype.isReadable = function () {
            return this.flagStr.indexOf('r') !== -1 || this.flagStr.indexOf('+') !== -1;
        };

        /**
        * Returns true if the file is writeable.
        * @return [Boolean]
        */
        FileFlag.prototype.isWriteable = function () {
            return this.flagStr.indexOf('w') !== -1 || this.flagStr.indexOf('a') !== -1 || this.flagStr.indexOf('+') !== -1;
        };

        /**
        * Returns true if the file mode should truncate.
        * @return [Boolean]
        */
        FileFlag.prototype.isTruncating = function () {
            return this.flagStr.indexOf('w') !== -1;
        };

        /**
        * Returns true if the file is appendable.
        * @return [Boolean]
        */
        FileFlag.prototype.isAppendable = function () {
            return this.flagStr.indexOf('a') !== -1;
        };

        /**
        * Returns true if the file is open in synchronous mode.
        * @return [Boolean]
        */
        FileFlag.prototype.isSynchronous = function () {
            return this.flagStr.indexOf('s') !== -1;
        };

        /**
        * Returns true if the file is open in exclusive mode.
        * @return [Boolean]
        */
        FileFlag.prototype.isExclusive = function () {
            return this.flagStr.indexOf('x') !== -1;
        };

        /**
        * Returns one of the static fields on this object that indicates the
        * appropriate response to the path existing.
        * @return [Number]
        */
        FileFlag.prototype.pathExistsAction = function () {
            if (this.isExclusive()) {
                return 1 /* THROW_EXCEPTION */;
            } else if (this.isTruncating()) {
                return 2 /* TRUNCATE_FILE */;
            } else {
                return 0 /* NOP */;
            }
        };

        /**
        * Returns one of the static fields on this object that indicates the
        * appropriate response to the path not existing.
        * @return [Number]
        */
        FileFlag.prototype.pathNotExistsAction = function () {
            if ((this.isWriteable() || this.isAppendable()) && this.flagStr !== 'r+') {
                return 3 /* CREATE_FILE */;
            } else {
                return 1 /* THROW_EXCEPTION */;
            }
        };
        FileFlag.flagCache = {};

        FileFlag.validFlagStrs = ['r', 'r+', 'rs', 'rs+', 'w', 'wx', 'w+', 'wx+', 'a', 'ax', 'a+', 'ax+'];
        return FileFlag;
    })();
    exports.FileFlag = FileFlag;
});
//# sourceMappingURL=file_flag.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/node_eventemitter',["require", "exports", './buffer', './api_error'], function(require, exports, buffer, api_error) {
    var Buffer = buffer.Buffer;
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;

    /**
    * Internal class. Represents a buffered event.
    */
    var BufferedEvent = (function () {
        function BufferedEvent(data, encoding, cb) {
            this.data = data;
            this.encoding = encoding;
            this.cb = cb;
            this.size = typeof (data) !== 'string' ? data.length : Buffer.byteLength(data, encoding != null ? encoding : undefined);

            // If data is a buffer, we need to copy it.
            if (typeof (this.data) !== 'string') {
                this.data = this.data.sliceCopy();
            }
        }
        BufferedEvent.prototype.getData = function (encoding) {
            if (encoding == null) {
                if (typeof (this.data) === 'string') {
                    return new Buffer(this.data, this.encoding != null ? this.encoding : undefined);
                } else {
                    return this.data;
                }
            } else {
                if (typeof (this.data) === 'string') {
                    if (encoding === this.encoding) {
                        return this.data;
                    } else {
                        return (new Buffer(this.data, this.encoding != null ? this.encoding : undefined)).toString(encoding);
                    }
                } else {
                    return this.data.toString(encoding);
                }
            }
        };
        return BufferedEvent;
    })();

    /**
    * Provides an abstract implementation of the EventEmitter interface.
    */
    var AbstractEventEmitter = (function () {
        function AbstractEventEmitter() {
            this._listeners = {};
            this.maxListeners = 10;
        }
        /**
        * Adds a listener for the particular event.
        */
        AbstractEventEmitter.prototype.addListener = function (event, listener) {
            if (typeof (this._listeners[event]) === 'undefined') {
                this._listeners[event] = [];
            }
            if (this._listeners[event].push(listener) > this.maxListeners) {
                process.stdout.write("Warning: Event " + event + " has more than " + this.maxListeners + " listeners.\n");
            }
            this.emit('newListener', event, listener);
            return this;
        };

        /**
        * Adds a listener for the particular event.
        */
        AbstractEventEmitter.prototype.on = function (event, listener) {
            return this.addListener(event, listener);
        };

        /**
        * Adds a listener for the particular event that fires only once.
        */
        AbstractEventEmitter.prototype.once = function (event, listener) {
            // Create a new callback that will only fire once.
            var fired = false, newListener = function () {
                this.removeListener(event, newListener);

                if (!fired) {
                    fired = true;
                    listener.apply(this, arguments);
                }
            };
            return this.addListener(event, newListener);
        };

        /**
        * Emits the 'removeListener' event for the specified listeners.
        */
        AbstractEventEmitter.prototype._emitRemoveListener = function (event, listeners) {
            var i;

            // Only emit the event if someone is listening.
            if (this._listeners['removeListener'] && this._listeners['removeListener'].length > 0) {
                for (i = 0; i < listeners.length; i++) {
                    this.emit('removeListener', event, listeners[i]);
                }
            }
        };

        /**
        * Removes the particular listener for the given event.
        */
        AbstractEventEmitter.prototype.removeListener = function (event, listener) {
            var listeners = this._listeners[event];
            if (typeof (listeners) !== 'undefined') {
                // Remove listener, if present.
                var idx = listeners.indexOf(listener);
                if (idx > -1) {
                    listeners.splice(idx, 1);
                }
            }
            this.emit('removeListener', event, listener);
            return this;
        };

        /**
        * Removes all listeners, or those of the specified event.
        */
        AbstractEventEmitter.prototype.removeAllListeners = function (event) {
            var removed, keys, i;
            if (typeof (event) !== 'undefined') {
                removed = this._listeners[event];

                // Clear one event.
                this._listeners[event] = [];
                this._emitRemoveListener(event, removed);
            } else {
                // Clear all events.
                keys = Object.keys(this._listeners);
                for (i = 0; i < keys.length; i++) {
                    this.removeAllListeners(keys[i]);
                }
            }
            return this;
        };

        /**
        * EventEmitters print a warning when an event has greater than this specified
        * number of listeners.
        */
        AbstractEventEmitter.prototype.setMaxListeners = function (n) {
            this.maxListeners = n;
        };

        /**
        * Returns the listeners for the given event.
        */
        AbstractEventEmitter.prototype.listeners = function (event) {
            if (typeof (this._listeners[event]) === 'undefined') {
                this._listeners[event] = [];
            }

            // Return a *copy* of our internal structure.
            return this._listeners[event].slice(0);
        };

        /**
        * Emits the specified event to all listeners of the particular event.
        */
        AbstractEventEmitter.prototype.emit = function (event) {
            var args = [];
            for (var _i = 0; _i < (arguments.length - 1); _i++) {
                args[_i] = arguments[_i + 1];
            }
            var listeners = this._listeners[event], rv = false;
            if (typeof (listeners) !== 'undefined') {
                var i;
                for (i = 0; i < listeners.length; i++) {
                    rv = true;
                    listeners[i].apply(this, args);
                }
            }
            return rv;
        };
        return AbstractEventEmitter;
    })();
    exports.AbstractEventEmitter = AbstractEventEmitter;

    /**
    * Provides an abstract implementation of the WritableStream and ReadableStream
    * interfaces.
    * @todo: Check readable/writable status.
    */
    var AbstractDuplexStream = (function (_super) {
        __extends(AbstractDuplexStream, _super);
        /**
        * Abstract stream implementation that can be configured to be readable and/or
        * writable.
        */
        function AbstractDuplexStream(writable, readable) {
            _super.call(this);
            this.writable = writable;
            this.readable = readable;
            /**
            * How should the data output be encoded? 'null' means 'Buffer'.
            */
            this.encoding = null;
            /**
            * Is this stream currently flowing (resumed) or non-flowing (paused)?
            */
            this.flowing = false;
            /**
            * Event buffer. Simply queues up all write requests.
            */
            this.buffer = [];
            /**
            * Once set, the stream is closed. Emitted once 'buffer' is empty.
            */
            this.endEvent = null;
            /**
            * Has the stream ended?
            */
            this.ended = false;
            /**
            * The last time we checked, was the buffer empty?
            * We emit 'readable' events when this transitions from 'true' -> 'false'.
            */
            this.drained = true;
        }
        /**
        * Adds a listener for the particular event.
        * Implemented here so that we can capture data EventListeners, which trigger
        * us to 'resume'.
        */
        AbstractDuplexStream.prototype.addListener = function (event, listener) {
            var rv = _super.prototype.addListener.call(this, event, listener), _this = this;
            if (event === 'data' && !this.flowing) {
                this.resume();
            } else if (event === 'readable' && this.buffer.length > 0) {
                setTimeout(function () {
                    _this.emit('readable');
                }, 0);
            }
            return rv;
        };

        /**
        * Helper function for 'write' and 'end' functions.
        */
        AbstractDuplexStream.prototype._processArgs = function (data, arg2, arg3) {
            if (typeof (arg2) === 'string') {
                // data, encoding, cb?
                return new BufferedEvent(data, arg2, arg3);
            } else {
                // data, cb?
                return new BufferedEvent(data, null, arg2);
            }
        };

        /**
        * If flowing, this will process pending events.
        */
        AbstractDuplexStream.prototype._processEvents = function () {
            var drained = this.buffer.length === 0;
            if (this.drained !== drained) {
                if (this.drained) {
                    // Went from drained to not drained. New stuff is available.
                    // @todo: Is this event relevant in flowing mode?
                    this.emit('readable');
                }
            }

            if (this.flowing && this.buffer.length !== 0) {
                this.emit('data', this.read());
            }

            // Are we drained? Check.
            this.drained = this.buffer.length === 0;
        };

        /**
        * Emits the given buffered event.
        */
        AbstractDuplexStream.prototype.emitEvent = function (type, event) {
            this.emit(type, event.getData(this.encoding));
            if (event.cb) {
                event.cb();
            }
        };

        AbstractDuplexStream.prototype.write = function (data, arg2, arg3) {
            if (this.ended) {
                throw new ApiError(0 /* EPERM */, 'Cannot write to an ended stream.');
            }
            var event = this._processArgs(data, arg2, arg3);
            this._push(event);
            return this.flowing;
        };

        AbstractDuplexStream.prototype.end = function (data, arg2, arg3) {
            if (this.ended) {
                throw new ApiError(0 /* EPERM */, 'Stream is already closed.');
            }
            var event = this._processArgs(data, arg2, arg3);
            this.ended = true;
            this.endEvent = event;
            this._processEvents();
        };

        /**** Readable Interface ****/
        /**
        * Read a given number of bytes from the buffer. Should only be called in
        * non-flowing mode.
        * If we do not have `size` bytes available, return null.
        */
        AbstractDuplexStream.prototype.read = function (size) {
            var events = [], eventsCbs = [], lastCb, eventsSize = 0, event, buff, trueSize, i = 0, sizeUnspecified = typeof (size) !== 'number';

            // I do this so I do not need to specialize the loop below.
            if (sizeUnspecified)
                size = 4294967295;

            for (i = 0; i < this.buffer.length && eventsSize < size; i++) {
                event = this.buffer[i];
                events.push(event.getData());
                if (event.cb) {
                    eventsCbs.push(event.cb);
                }
                eventsSize += event.size;
                lastCb = event.cb;
            }

            if (!sizeUnspecified && eventsSize < size) {
                // For some reason, the Node stream API specifies that we either return
                // 'size' bytes of data, or nothing at all.
                return null;
            }

            // Remove all of the events we are processing from the buffer.
            this.buffer = this.buffer.slice(events.length);

            // The 'true size' of the final event we're going to send out.
            trueSize = eventsSize > size ? size : eventsSize;

            // Concat at all of the events into one buffer.
            buff = Buffer.concat(events);
            if (eventsSize > size) {
                // If last event had a cb, ignore it -- we trigger it when that *entire*
                // write finishes.
                if (lastCb)
                    eventsCbs.pop();

                // Make a new event for the remaining data.
                this._push(new BufferedEvent(buff.slice(size), null, lastCb));
            }

            // Schedule the relevant cbs to fire *after* we've returned these values.
            if (eventsCbs.length > 0) {
                setTimeout(function () {
                    var i;
                    for (i = 0; i < eventsCbs.length; i++) {
                        eventsCbs[i]();
                    }
                }, 0);
            }

            // If we're at the end of the buffer and an endEvent is specified, schedule
            // the event to fire.
            if (this.ended && this.buffer.length === 0 && this.endEvent !== null) {
                var endEvent = this.endEvent, _this = this;

                // Erase it so we don't accidentally trigger it again.
                this.endEvent = null;
                setTimeout(function () {
                    _this.emitEvent('end', endEvent);
                }, 0);
            }

            // Return in correct encoding.
            if (events.length === 0) {
                // Buffer was empty. We're supposed to return 'null', as opposed to an
                // empty buffer or string.
                // [BFS] Emit a '_read' event to signal that maybe the write-end of this
                //       should push some data into the pipe.
                this.emit('_read');
                return null;
            } else if (this.encoding === null) {
                return buff.slice(0, trueSize);
            } else {
                return buff.toString(this.encoding, 0, trueSize);
            }
        };

        /**
        * Set the encoding for the 'data' event.
        */
        AbstractDuplexStream.prototype.setEncoding = function (encoding) {
            this.encoding = encoding;
        };

        /**
        * Pause the stream.
        */
        AbstractDuplexStream.prototype.pause = function () {
            this.flowing = false;
        };

        /**
        * Resume the stream.
        */
        AbstractDuplexStream.prototype.resume = function () {
            this.flowing = true;

            // Process any buffered writes.
            this._processEvents();
        };

        /**
        * Pipe a readable stream into a writable stream. Currently unimplemented.
        */
        AbstractDuplexStream.prototype.pipe = function (destination, options) {
            throw new ApiError(0 /* EPERM */, "Unimplemented.");
        };
        AbstractDuplexStream.prototype.unpipe = function (destination) {
        };

        AbstractDuplexStream.prototype.unshift = function (chunk) {
            if (this.ended) {
                throw new ApiError(0 /* EPERM */, "Stream has ended.");
            }
            this.buffer.unshift(new BufferedEvent(chunk, this.encoding));
            this._processEvents();
        };

        /**
        * 'Push' the given piece of data to the back of the buffer.
        * Returns true if the event was sent out, false if buffered.
        */
        AbstractDuplexStream.prototype._push = function (event) {
            this.buffer.push(event);
            this._processEvents();
        };

        /**
        * Enables backwards-compatibility with older versions of Node and their
        * stream interface. Unimplemented.
        */
        AbstractDuplexStream.prototype.wrap = function (stream) {
            throw new ApiError(0 /* EPERM */, "Unimplemented.");
        };
        return AbstractDuplexStream;
    })(AbstractEventEmitter);
    exports.AbstractDuplexStream = AbstractDuplexStream;
});
//# sourceMappingURL=node_eventemitter.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/node_process',["require", "exports", './node_eventemitter'], function(require, exports, eventemitter) {
    var path = null;

    var TTY = (function (_super) {
        __extends(TTY, _super);
        function TTY() {
            _super.call(this, true, true);
            this.isRaw = false;
            this.columns = 80;
            this.rows = 120;
            this.isTTY = true;
        }
        /**
        * Set read mode to 'true' to enable raw mode.
        */
        TTY.prototype.setReadMode = function (mode) {
            if (this.isRaw !== mode) {
                this.isRaw = mode;

                // [BFS] TTY implementations can use this to change their event emitting
                //       patterns.
                this.emit('modeChange');
            }
        };

        /**
        * [BFS] Update the number of columns available on the terminal.
        */
        TTY.prototype.changeColumns = function (columns) {
            if (columns !== this.columns) {
                this.columns = columns;

                // Resize event.
                this.emit('resize');
            }
        };

        /**
        * [BFS] Update the number of rows available on the terminal.
        */
        TTY.prototype.changeRows = function (rows) {
            if (rows !== this.rows) {
                this.rows = rows;

                // Resize event.
                this.emit('resize');
            }
        };

        /**
        * Returns 'true' if the given object is a TTY.
        */
        TTY.isatty = function (fd) {
            return fd instanceof TTY;
        };
        return TTY;
    })(eventemitter.AbstractDuplexStream);
    exports.TTY = TTY;

    /**
    * Partial implementation of Node's `process` module.
    * We implement the portions that are relevant for the filesystem.
    * @see http://nodejs.org/api/process.html
    * @class
    */
    var Process = (function () {
        function Process() {
            this.startTime = Date.now();
            this._cwd = '/';
            /**
            * Returns what platform you are running on.
            * @return [String]
            */
            this.platform = 'browser';
            this.argv = [];
            this.stdout = new TTY();
            this.stderr = new TTY();
            this.stdin = new TTY();
        }
        /**
        * Changes the current working directory.
        *
        * **Note**: BrowserFS does not validate that the directory actually exists.
        *
        * @example Usage example
        *   console.log('Starting directory: ' + process.cwd());
        *   process.chdir('/tmp');
        *   console.log('New directory: ' + process.cwd());
        * @param [String] dir The directory to change to.
        */
        Process.prototype.chdir = function (dir) {
            // XXX: Circular dependency hack.
            if (path === null) {
                path = require('./node_path').path;
            }
            this._cwd = path.resolve(dir);
        };

        /**
        * Returns the current working directory.
        * @example Usage example
        *   console.log('Current directory: ' + process.cwd());
        * @return [String] The current working directory.
        */
        Process.prototype.cwd = function () {
            return this._cwd;
        };

        /**
        * Number of seconds BrowserFS has been running.
        * @return [Number]
        */
        Process.prototype.uptime = function () {
            return ((Date.now() - this.startTime) / 1000) | 0;
        };
        return Process;
    })();
    exports.Process = Process;

    // process is a singleton.
    exports.process = new Process();
});
//# sourceMappingURL=node_process.js.map
;
define('core/node_path',["require", "exports", './node_process'], function(require, exports, node_process) {
    var process = node_process.process;

    /**
    * Emulates Node's `path` module. This module contains utilities for handling and
    * transforming file paths. **All** of these methods perform only string
    * transformations. The file system is not consulted to check whether paths are
    * valid.
    * @see http://nodejs.org/api/path.html
    * @class
    */
    var path = (function () {
        function path() {
        }
        /**
        * Normalize a string path, taking care of '..' and '.' parts.
        *
        * When multiple slashes are found, they're replaced by a single one; when the path contains a trailing slash, it is preserved. On Windows backslashes are used.
        * @example Usage example
        *   path.normalize('/foo/bar//baz/asdf/quux/..')
        *   // returns
        *   '/foo/bar/baz/asdf'
        * @param [String] p The path to normalize.
        * @return [String]
        */
        path.normalize = function (p) {
            // Special case: '' -> '.'
            if (p === '') {
                p = '.';
            }

            // It's very important to know if the path is relative or not, since it
            // changes how we process .. and reconstruct the split string.
            var absolute = p.charAt(0) === path.sep;

            // Remove repeated //s
            p = path._removeDuplicateSeps(p);

            // Try to remove as many '../' as possible, and remove '.' completely.
            var components = p.split(path.sep);
            var goodComponents = [];
            for (var idx = 0; idx < components.length; idx++) {
                var c = components[idx];
                if (c === '.') {
                    continue;
                } else if (c === '..' && (absolute || (!absolute && goodComponents.length > 0 && goodComponents[0] !== '..'))) {
                    // In the absolute case: Path is relative to root, so we may pop even if
                    // goodComponents is empty (e.g. /../ => /)
                    // In the relative case: We're getting rid of a directory that preceded
                    // it (e.g. /foo/../bar -> /bar)
                    goodComponents.pop();
                } else {
                    goodComponents.push(c);
                }
            }

            // Add in '.' when it's a relative path with no other nonempty components.
            // Possible results: '.' and './' (input: [''] or [])
            // @todo Can probably simplify this logic.
            if (!absolute && goodComponents.length < 2) {
                switch (goodComponents.length) {
                    case 1:
                        if (goodComponents[0] === '') {
                            goodComponents.unshift('.');
                        }
                        break;
                    default:
                        goodComponents.push('.');
                }
            }
            p = goodComponents.join(path.sep);
            if (absolute && p.charAt(0) !== path.sep) {
                p = path.sep + p;
            }
            return p;
        };

        /**
        * Join all arguments together and normalize the resulting path.
        *
        * Arguments must be strings.
        * @example Usage
        *   path.join('/foo', 'bar', 'baz/asdf', 'quux', '..')
        *   // returns
        *   '/foo/bar/baz/asdf'
        *
        *   path.join('foo', {}, 'bar')
        *   // throws exception
        *   TypeError: Arguments to path.join must be strings
        * @param [String,...] paths Each component of the path
        * @return [String]
        */
        path.join = function () {
            var paths = [];
            for (var _i = 0; _i < (arguments.length - 0); _i++) {
                paths[_i] = arguments[_i + 0];
            }
            // Required: Prune any non-strings from the path. I also prune empty segments
            // so we can do a simple join of the array.
            var processed = [];
            for (var i = 0; i < paths.length; i++) {
                var segment = paths[i];
                if (typeof segment !== 'string') {
                    throw new TypeError("Invalid argument type to path.join: " + (typeof segment));
                } else if (segment !== '') {
                    processed.push(segment);
                }
            }
            return path.normalize(processed.join(path.sep));
        };

        /**
        * Resolves to to an absolute path.
        *
        * If to isn't already absolute from arguments are prepended in right to left
        * order, until an absolute path is found. If after using all from paths still
        * no absolute path is found, the current working directory is used as well.
        * The resulting path is normalized, and trailing slashes are removed unless
        * the path gets resolved to the root directory. Non-string arguments are
        * ignored.
        *
        * Another way to think of it is as a sequence of cd commands in a shell.
        *
        *     path.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile')
        *
        * Is similar to:
        *
        *     cd foo/bar
        *     cd /tmp/file/
        *     cd ..
        *     cd a/../subfile
        *     pwd
        *
        * The difference is that the different paths don't need to exist and may also
        * be files.
        * @example Usage example
        *   path.resolve('/foo/bar', './baz')
        *   // returns
        *   '/foo/bar/baz'
        *
        *   path.resolve('/foo/bar', '/tmp/file/')
        *   // returns
        *   '/tmp/file'
        *
        *   path.resolve('wwwroot', 'static_files/png/', '../gif/image.gif')
        *   // if currently in /home/myself/node, it returns
        *   '/home/myself/node/wwwroot/static_files/gif/image.gif'
        * @param [String,...] paths
        * @return [String]
        */
        path.resolve = function () {
            var paths = [];
            for (var _i = 0; _i < (arguments.length - 0); _i++) {
                paths[_i] = arguments[_i + 0];
            }
            // Monitor for invalid paths, throw out empty paths, and look for the *last*
            // absolute path that we see.
            var processed = [];
            for (var i = 0; i < paths.length; i++) {
                var p = paths[i];
                if (typeof p !== 'string') {
                    throw new TypeError("Invalid argument type to path.join: " + (typeof p));
                } else if (p !== '') {
                    // Remove anything that has occurred before this absolute path, as it
                    // doesn't matter.
                    if (p.charAt(0) === path.sep) {
                        processed = [];
                    }
                    processed.push(p);
                }
            }

            // Special: Remove trailing slash unless it's the root
            var resolved = path.normalize(processed.join(path.sep));
            if (resolved.length > 1 && resolved.charAt(resolved.length - 1) === path.sep) {
                return resolved.substr(0, resolved.length - 1);
            }

            // Special: If it doesn't start with '/', it's relative and we need to append
            // the current directory.
            if (resolved.charAt(0) !== path.sep) {
                // Remove ./, since we're going to append the current directory.
                if (resolved.charAt(0) === '.' && (resolved.length === 1 || resolved.charAt(1) === path.sep)) {
                    resolved = resolved.length === 1 ? '' : resolved.substr(2);
                }

                // Append the current directory, which *must* be an absolute path.
                var cwd = process.cwd();
                if (resolved !== '') {
                    // cwd will never end in a /... unless it's the root.
                    resolved = this.normalize(cwd + (cwd !== '/' ? path.sep : '') + resolved);
                } else {
                    resolved = cwd;
                }
            }
            return resolved;
        };

        /**
        * Solve the relative path from from to to.
        *
        * At times we have two absolute paths, and we need to derive the relative path
        * from one to the other. This is actually the reverse transform of
        * path.resolve, which means we see that:
        *
        *    path.resolve(from, path.relative(from, to)) == path.resolve(to)
        *
        * @example Usage example
        *   path.relative('C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb')
        *   // returns
        *   '..\\..\\impl\\bbb'
        *
        *   path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb')
        *   // returns
        *   '../../impl/bbb'
        * @param [String] from
        * @param [String] to
        * @return [String]
        */
        path.relative = function (from, to) {
            var i;

            // Alright. Let's resolve these two to absolute paths and remove any
            // weirdness.
            from = path.resolve(from);
            to = path.resolve(to);
            var fromSegs = from.split(path.sep);
            var toSegs = to.split(path.sep);

            // Remove the first segment on both, as it's '' (both are absolute paths)
            toSegs.shift();
            fromSegs.shift();

            // There are two segments to this path:
            // * Going *up* the directory hierarchy with '..'
            // * Going *down* the directory hierarchy with foo/baz/bat.
            var upCount = 0;
            var downSegs = [];

            for (i = 0; i < fromSegs.length; i++) {
                var seg = fromSegs[i];
                if (seg === toSegs[i]) {
                    continue;
                }

                // The rest of 'from', including the current element, indicates how many
                // directories we need to go up.
                upCount = fromSegs.length - i;
                break;
            }

            // The rest of 'to' indicates where we need to change to. We place this
            // outside of the loop, as toSegs.length may be greater than fromSegs.length.
            downSegs = toSegs.slice(i);

            // Special case: If 'from' is '/'
            if (fromSegs.length === 1 && fromSegs[0] === '') {
                upCount = 0;
            }

            // upCount can't be greater than the number of fromSegs
            // (cd .. from / is still /)
            if (upCount > fromSegs.length) {
                upCount = fromSegs.length;
            }

            // Create the final string!
            var rv = '';
            for (i = 0; i < upCount; i++) {
                rv += '../';
            }
            rv += downSegs.join(path.sep);

            // Special case: Remove trailing '/'. Happens if it's all up and no down.
            if (rv.length > 1 && rv.charAt(rv.length - 1) === path.sep) {
                rv = rv.substr(0, rv.length - 1);
            }
            return rv;
        };

        /**
        * Return the directory name of a path. Similar to the Unix `dirname` command.
        *
        * Note that BrowserFS does not validate if the path is actually a valid
        * directory.
        * @example Usage example
        *   path.dirname('/foo/bar/baz/asdf/quux')
        *   // returns
        *   '/foo/bar/baz/asdf'
        * @param [String] p The path to get the directory name of.
        * @return [String]
        */
        path.dirname = function (p) {
            // We get rid of //, but we don't modify anything else (e.g. any extraneous .
            // and ../ are kept intact)
            p = path._removeDuplicateSeps(p);
            var absolute = p.charAt(0) === path.sep;
            var sections = p.split(path.sep);

            // Do 1 if it's /foo/bar, 2 if it's /foo/bar/
            if (sections.pop() === '' && sections.length > 0) {
                sections.pop();
            }
            if (sections.length > 1) {
                return sections.join(path.sep);
            } else if (absolute) {
                return path.sep;
            } else {
                return '.';
            }
        };

        /**
        * Return the last portion of a path. Similar to the Unix basename command.
        * @example Usage example
        *   path.basename('/foo/bar/baz/asdf/quux.html')
        *   // returns
        *   'quux.html'
        *
        *   path.basename('/foo/bar/baz/asdf/quux.html', '.html')
        *   // returns
        *   'quux'
        * @param [String] p
        * @param [String?] ext
        * @return [String]
        */
        path.basename = function (p, ext) {
            if (typeof ext === "undefined") { ext = ""; }
            // Special case: Normalize will modify this to '.'
            if (p === '') {
                return p;
            }

            // Normalize the string first to remove any weirdness.
            p = path.normalize(p);

            // Get the last part of the string.
            var sections = p.split(path.sep);
            var lastPart = sections[sections.length - 1];

            // Special case: If it's empty, then we have a string like so: foo/
            // Meaning, 'foo' is guaranteed to be a directory.
            if (lastPart === '' && sections.length > 1) {
                return sections[sections.length - 2];
            }

            // Remove the extension, if need be.
            if (ext.length > 0) {
                var lastPartExt = lastPart.substr(lastPart.length - ext.length);
                if (lastPartExt === ext) {
                    return lastPart.substr(0, lastPart.length - ext.length);
                }
            }
            return lastPart;
        };

        /**
        * Return the extension of the path, from the last '.' to end of string in the
        * last portion of the path. If there is no '.' in the last portion of the path
        * or the first character of it is '.', then it returns an empty string.
        * @example Usage example
        *   path.extname('index.html')
        *   // returns
        *   '.html'
        *
        *   path.extname('index.')
        *   // returns
        *   '.'
        *
        *   path.extname('index')
        *   // returns
        *   ''
        * @param [String] p
        * @return [String]
        */
        path.extname = function (p) {
            p = path.normalize(p);
            var sections = p.split(path.sep);
            p = sections.pop();

            // Special case: foo/file.ext/ should return '.ext'
            if (p === '' && sections.length > 0) {
                p = sections.pop();
            }
            if (p === '..') {
                return '';
            }
            var i = p.lastIndexOf('.');
            if (i === -1 || i === 0) {
                return '';
            }
            return p.substr(i);
        };

        /**
        * Checks if the given path is an absolute path.
        *
        * Despite not being documented, this is a tested part of Node's path API.
        * @param [String] p
        * @return [Boolean] True if the path appears to be an absolute path.
        */
        path.isAbsolute = function (p) {
            return p.length > 0 && p.charAt(0) === path.sep;
        };

        /**
        * Unknown. Undocumented.
        */
        path._makeLong = function (p) {
            return p;
        };

        path._removeDuplicateSeps = function (p) {
            p = p.replace(this._replaceRegex, this.sep);
            return p;
        };
        path.sep = '/';

        path._replaceRegex = new RegExp("//+", 'g');

        path.delimiter = ':';
        return path;
    })();
    exports.path = path;
});
//# sourceMappingURL=node_path.js.map
;
define('core/node_fs',["require", "exports", './api_error', './file_flag', './buffer', './node_path'], function(require, exports, api_error, file_flag, buffer, node_path) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var FileFlag = file_flag.FileFlag;
    var Buffer = buffer.Buffer;
    var path = node_path.path;

    /**
    * Wraps a callback with a setImmediate call.
    * @param [Function] cb The callback to wrap.
    * @param [Number] numArgs The number of arguments that the callback takes.
    * @return [Function] The wrapped callback.
    */
    function wrapCb(cb, numArgs) {
        if (typeof cb !== 'function') {
            throw new ApiError(9 /* EINVAL */, 'Callback must be a function.');
        }

        // @todo This is used for unit testing. Maybe we should inject this logic
        //       dynamically rather than bundle it in 'production' code.
        if (typeof __numWaiting === 'undefined') {
            __numWaiting = 0;
        }
        __numWaiting++;

        switch (numArgs) {
            case 1:
                return function (arg1) {
                    setImmediate(function () {
                        __numWaiting--;
                        return cb(arg1);
                    });
                };
            case 2:
                return function (arg1, arg2) {
                    setImmediate(function () {
                        __numWaiting--;
                        return cb(arg1, arg2);
                    });
                };
            case 3:
                return function (arg1, arg2, arg3) {
                    setImmediate(function () {
                        __numWaiting--;
                        return cb(arg1, arg2, arg3);
                    });
                };
            default:
                throw new Error('Invalid invocation of wrapCb.');
        }
    }

    /**
    * Checks if the fd is valid.
    * @param [BrowserFS.File] fd A file descriptor (in BrowserFS, it's a File object)
    * @return [Boolean, BrowserFS.ApiError] Returns `true` if the FD is OK,
    *   otherwise returns an ApiError.
    */
    function checkFd(fd) {
        if (typeof fd['write'] !== 'function') {
            throw new ApiError(3 /* EBADF */, 'Invalid file descriptor.');
        }
    }

    function normalizeMode(mode, def) {
        switch (typeof mode) {
            case 'number':
                // (path, flag, mode, cb?)
                return mode;
            case 'string':
                // (path, flag, modeString, cb?)
                var trueMode = parseInt(mode, 8);
                if (trueMode !== NaN) {
                    return trueMode;
                }

            default:
                return def;
        }
    }

    function normalizePath(p) {
        // Node doesn't allow null characters in paths.
        if (p.indexOf('\u0000') >= 0) {
            throw new ApiError(9 /* EINVAL */, 'Path must be a string without null bytes.');
        } else if (p === '') {
            throw new ApiError(9 /* EINVAL */, 'Path must not be empty.');
        }
        return path.resolve(p);
    }

    function normalizeOptions(options, defEnc, defFlag, defMode) {
        switch (typeof options) {
            case 'object':
                return {
                    encoding: typeof options['encoding'] !== 'undefined' ? options['encoding'] : defEnc,
                    flag: typeof options['flag'] !== 'undefined' ? options['flag'] : defFlag,
                    mode: normalizeMode(options['mode'], defMode)
                };
            case 'string':
                return {
                    encoding: options,
                    flag: defFlag,
                    mode: defMode
                };
            default:
                return {
                    encoding: defEnc,
                    flag: defFlag,
                    mode: defMode
                };
        }
    }

    // The default callback is a NOP.
    function nopCb() {
    }
    ;

    /**
    * The node frontend to all filesystems.
    * This layer handles:
    *
    * * Sanity checking inputs.
    * * Normalizing paths.
    * * Resetting stack depth for asynchronous operations which may not go through
    *   the browser by wrapping all input callbacks using `setImmediate`.
    * * Performing the requested operation through the filesystem or the file
    *   descriptor, as appropriate.
    * * Handling optional arguments and setting default arguments.
    * @see http://nodejs.org/api/fs.html
    * @class
    */
    var fs = (function () {
        function fs() {
        }
        fs._initialize = function (rootFS) {
            if (!rootFS.constructor.isAvailable()) {
                throw new ApiError(9 /* EINVAL */, 'Tried to instantiate BrowserFS with an unavailable file system.');
            }
            return fs.root = rootFS;
        };

        fs._toUnixTimestamp = function (time) {
            if (typeof time === 'number') {
                return time;
            } else if (time instanceof Date) {
                return time.getTime() / 1000;
            }
            throw new Error("Cannot parse time: " + time);
        };

        /**
        * **NONSTANDARD**: Grab the FileSystem instance that backs this API.
        * @return [BrowserFS.FileSystem | null] Returns null if the file system has
        *   not been initialized.
        */
        fs.getRootFS = function () {
            if (fs.root) {
                return fs.root;
            } else {
                return null;
            }
        };

        // FILE OR DIRECTORY METHODS
        /**
        * Asynchronous rename. No arguments other than a possible exception are given
        * to the completion callback.
        * @param [String] oldPath
        * @param [String] newPath
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.rename = function (oldPath, newPath, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                fs.root.rename(normalizePath(oldPath), normalizePath(newPath), newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous rename.
        * @param [String] oldPath
        * @param [String] newPath
        */
        fs.renameSync = function (oldPath, newPath) {
            fs.root.renameSync(normalizePath(oldPath), normalizePath(newPath));
        };

        /**
        * Test whether or not the given path exists by checking with the file system.
        * Then call the callback argument with either true or false.
        * @example Sample invocation
        *   fs.exists('/etc/passwd', function (exists) {
        *     util.debug(exists ? "it's there" : "no passwd!");
        *   });
        * @param [String] path
        * @param [Function(Boolean)] callback
        */
        fs.exists = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                return fs.root.exists(normalizePath(path), newCb);
            } catch (e) {
                // Doesn't return an error. If something bad happens, we assume it just
                // doesn't exist.
                return newCb(false);
            }
        };

        /**
        * Test whether or not the given path exists by checking with the file system.
        * @param [String] path
        * @return [boolean]
        */
        fs.existsSync = function (path) {
            try  {
                return fs.root.existsSync(normalizePath(path));
            } catch (e) {
                // Doesn't return an error. If something bad happens, we assume it just
                // doesn't exist.
                return false;
            }
        };

        /**
        * Asynchronous `stat`.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError, BrowserFS.node.fs.Stats)] callback
        */
        fs.stat = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 2);
            try  {
                return fs.root.stat(normalizePath(path), false, newCb);
            } catch (e) {
                return newCb(e, null);
            }
        };

        /**
        * Synchronous `stat`.
        * @param [String] path
        * @return [BrowserFS.node.fs.Stats]
        */
        fs.statSync = function (path) {
            return fs.root.statSync(normalizePath(path), false);
        };

        /**
        * Asynchronous `lstat`.
        * `lstat()` is identical to `stat()`, except that if path is a symbolic link,
        * then the link itself is stat-ed, not the file that it refers to.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError, BrowserFS.node.fs.Stats)] callback
        */
        fs.lstat = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 2);
            try  {
                return fs.root.stat(normalizePath(path), true, newCb);
            } catch (e) {
                return newCb(e, null);
            }
        };

        /**
        * Synchronous `lstat`.
        * `lstat()` is identical to `stat()`, except that if path is a symbolic link,
        * then the link itself is stat-ed, not the file that it refers to.
        * @param [String] path
        * @return [BrowserFS.node.fs.Stats]
        */
        fs.lstatSync = function (path) {
            return fs.root.statSync(normalizePath(path), true);
        };

        fs.truncate = function (path, arg2, cb) {
            if (typeof arg2 === "undefined") { arg2 = 0; }
            if (typeof cb === "undefined") { cb = nopCb; }
            var len = 0;
            if (typeof arg2 === 'function') {
                cb = arg2;
            } else if (typeof arg2 === 'number') {
                len = arg2;
            }

            var newCb = wrapCb(cb, 1);
            try  {
                if (len < 0) {
                    throw new ApiError(9 /* EINVAL */);
                }
                return fs.root.truncate(normalizePath(path), len, newCb);
            } catch (e) {
                return newCb(e);
            }
        };

        /**
        * Synchronous `truncate`.
        * @param [String] path
        * @param [Number] len
        */
        fs.truncateSync = function (path, len) {
            if (typeof len === "undefined") { len = 0; }
            if (len < 0) {
                throw new ApiError(9 /* EINVAL */);
            }
            return fs.root.truncateSync(normalizePath(path), len);
        };

        /**
        * Asynchronous `unlink`.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.unlink = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                return fs.root.unlink(normalizePath(path), newCb);
            } catch (e) {
                return newCb(e);
            }
        };

        /**
        * Synchronous `unlink`.
        * @param [String] path
        */
        fs.unlinkSync = function (path) {
            return fs.root.unlinkSync(normalizePath(path));
        };

        fs.open = function (path, flag, arg2, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var mode = normalizeMode(arg2, 0x1a4);
            cb = typeof arg2 === 'function' ? arg2 : cb;
            var newCb = wrapCb(cb, 2);
            try  {
                return fs.root.open(normalizePath(path), FileFlag.getFileFlag(flag), mode, newCb);
            } catch (e) {
                return newCb(e, null);
            }
        };

        fs.openSync = function (path, flag, mode) {
            if (typeof mode === "undefined") { mode = 0x1a4; }
            return fs.root.openSync(normalizePath(path), FileFlag.getFileFlag(flag), mode);
        };

        fs.readFile = function (filename, arg2, cb) {
            if (typeof arg2 === "undefined") { arg2 = {}; }
            if (typeof cb === "undefined") { cb = nopCb; }
            var options = normalizeOptions(arg2, null, 'r', null);
            cb = typeof arg2 === 'function' ? arg2 : cb;
            var newCb = wrapCb(cb, 2);
            try  {
                var flag = FileFlag.getFileFlag(options['flag']);
                if (!flag.isReadable()) {
                    return newCb(new ApiError(9 /* EINVAL */, 'Flag passed to readFile must allow for reading.'));
                }
                return fs.root.readFile(normalizePath(filename), options.encoding, flag, newCb);
            } catch (e) {
                return newCb(e, null);
            }
        };

        fs.readFileSync = function (filename, arg2) {
            if (typeof arg2 === "undefined") { arg2 = {}; }
            var options = normalizeOptions(arg2, null, 'r', null);
            var flag = FileFlag.getFileFlag(options.flag);
            if (!flag.isReadable()) {
                throw new ApiError(9 /* EINVAL */, 'Flag passed to readFile must allow for reading.');
            }
            return fs.root.readFileSync(normalizePath(filename), options.encoding, flag);
        };

        fs.writeFile = function (filename, data, arg3, cb) {
            if (typeof arg3 === "undefined") { arg3 = {}; }
            if (typeof cb === "undefined") { cb = nopCb; }
            var options = normalizeOptions(arg3, 'utf8', 'w', 0x1a4);
            cb = typeof arg3 === 'function' ? arg3 : cb;
            var newCb = wrapCb(cb, 1);
            try  {
                var flag = FileFlag.getFileFlag(options.flag);
                if (!flag.isWriteable()) {
                    return newCb(new ApiError(9 /* EINVAL */, 'Flag passed to writeFile must allow for writing.'));
                }
                return fs.root.writeFile(normalizePath(filename), data, options.encoding, flag, options.mode, newCb);
            } catch (e) {
                return newCb(e);
            }
        };

        fs.writeFileSync = function (filename, data, arg3) {
            var options = normalizeOptions(arg3, 'utf8', 'w', 0x1a4);
            var flag = FileFlag.getFileFlag(options.flag);
            if (!flag.isWriteable()) {
                throw new ApiError(9 /* EINVAL */, 'Flag passed to writeFile must allow for writing.');
            }
            return fs.root.writeFileSync(normalizePath(filename), data, options.encoding, flag, options.mode);
        };

        fs.appendFile = function (filename, data, arg3, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var options = normalizeOptions(arg3, 'utf8', 'a', 0x1a4);
            cb = typeof arg3 === 'function' ? arg3 : cb;
            var newCb = wrapCb(cb, 1);
            try  {
                var flag = FileFlag.getFileFlag(options.flag);
                if (!flag.isAppendable()) {
                    return newCb(new ApiError(9 /* EINVAL */, 'Flag passed to appendFile must allow for appending.'));
                }
                fs.root.appendFile(normalizePath(filename), data, options.encoding, flag, options.mode, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.appendFileSync = function (filename, data, arg3) {
            var options = normalizeOptions(arg3, 'utf8', 'a', 0x1a4);
            var flag = FileFlag.getFileFlag(options.flag);
            if (!flag.isAppendable()) {
                throw new ApiError(9 /* EINVAL */, 'Flag passed to appendFile must allow for appending.');
            }
            return fs.root.appendFileSync(normalizePath(filename), data, options.encoding, flag, options.mode);
        };

        // FILE DESCRIPTOR METHODS
        /**
        * Asynchronous `fstat`.
        * `fstat()` is identical to `stat()`, except that the file to be stat-ed is
        * specified by the file descriptor `fd`.
        * @param [BrowserFS.File] fd
        * @param [Function(BrowserFS.ApiError, BrowserFS.node.fs.Stats)] callback
        */
        fs.fstat = function (fd, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 2);
            try  {
                checkFd(fd);
                fd.stat(newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `fstat`.
        * `fstat()` is identical to `stat()`, except that the file to be stat-ed is
        * specified by the file descriptor `fd`.
        * @param [BrowserFS.File] fd
        * @return [BrowserFS.node.fs.Stats]
        */
        fs.fstatSync = function (fd) {
            checkFd(fd);
            return fd.statSync();
        };

        /**
        * Asynchronous close.
        * @param [BrowserFS.File] fd
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.close = function (fd, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                checkFd(fd);
                fd.close(newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous close.
        * @param [BrowserFS.File] fd
        */
        fs.closeSync = function (fd) {
            checkFd(fd);
            return fd.closeSync();
        };

        fs.ftruncate = function (fd, arg2, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var length = typeof arg2 === 'number' ? arg2 : 0;
            cb = typeof arg2 === 'function' ? arg2 : cb;
            var newCb = wrapCb(cb, 1);
            try  {
                checkFd(fd);
                if (length < 0) {
                    throw new ApiError(9 /* EINVAL */);
                }
                fd.truncate(length, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous ftruncate.
        * @param [BrowserFS.File] fd
        * @param [Number] len
        */
        fs.ftruncateSync = function (fd, len) {
            if (typeof len === "undefined") { len = 0; }
            checkFd(fd);
            return fd.truncateSync(len);
        };

        /**
        * Asynchronous fsync.
        * @param [BrowserFS.File] fd
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.fsync = function (fd, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                checkFd(fd);
                fd.sync(newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous fsync.
        * @param [BrowserFS.File] fd
        */
        fs.fsyncSync = function (fd) {
            checkFd(fd);
            return fd.syncSync();
        };

        /**
        * Asynchronous fdatasync.
        * @param [BrowserFS.File] fd
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.fdatasync = function (fd, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                checkFd(fd);
                fd.datasync(newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous fdatasync.
        * @param [BrowserFS.File] fd
        */
        fs.fdatasyncSync = function (fd) {
            checkFd(fd);
            fd.datasyncSync();
        };

        fs.write = function (fd, arg2, arg3, arg4, arg5, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var buffer, offset, length, position = null;
            if (typeof arg2 === 'string') {
                // Signature 1: (fd, string, [position?, [encoding?]], cb?)
                var encoding = 'utf8';
                switch (typeof arg3) {
                    case 'function':
                        // (fd, string, cb)
                        cb = arg3;
                        break;
                    case 'number':
                        // (fd, string, position, encoding?, cb?)
                        position = arg3;
                        encoding = typeof arg4 === 'string' ? arg4 : 'utf8';
                        cb = typeof arg5 === 'function' ? arg5 : cb;
                        break;
                    default:
                        // ...try to find the callback and get out of here!
                        cb = typeof arg4 === 'function' ? arg4 : typeof arg5 === 'function' ? arg5 : cb;
                        return cb(new ApiError(9 /* EINVAL */, 'Invalid arguments.'));
                }
                buffer = new Buffer(arg2, encoding);
                offset = 0;
                length = buffer.length;
            } else {
                // Signature 2: (fd, buffer, offset, length, position?, cb?)
                buffer = arg2;
                offset = arg3;
                length = arg4;
                position = typeof arg5 === 'number' ? arg5 : null;
                cb = typeof arg5 === 'function' ? arg5 : cb;
            }

            var newCb = wrapCb(cb, 3);
            try  {
                checkFd(fd);
                if (position == null) {
                    position = fd.getPos();
                }
                fd.write(buffer, offset, length, position, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.writeSync = function (fd, arg2, arg3, arg4, arg5) {
            var buffer, offset = 0, length, position;
            if (typeof arg2 === 'string') {
                // Signature 1: (fd, string, [position?, [encoding?]])
                position = typeof arg3 === 'number' ? arg3 : null;
                var encoding = typeof arg4 === 'string' ? arg4 : 'utf8';
                offset = 0;
                buffer = new Buffer(arg2, encoding);
                length = buffer.length;
            } else {
                // Signature 2: (fd, buffer, offset, length, position?)
                buffer = arg2;
                offset = arg3;
                length = arg4;
                position = typeof arg5 === 'number' ? arg5 : null;
            }

            checkFd(fd);
            if (position == null) {
                position = fd.getPos();
            }
            return fd.writeSync(buffer, offset, length, position);
        };

        fs.read = function (fd, arg2, arg3, arg4, arg5, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var position, offset, length, buffer, newCb;
            if (typeof arg2 === 'number') {
                // legacy interface
                // (fd, length, position, encoding, callback)
                length = arg2;
                position = arg3;
                var encoding = arg4;
                cb = typeof arg5 === 'function' ? arg5 : cb;
                offset = 0;
                buffer = new Buffer(length);

                // XXX: Inefficient.
                // Wrap the cb so we shelter upper layers of the API from these
                // shenanigans.
                newCb = wrapCb((function (err, bytesRead, buf) {
                    if (err) {
                        return cb(err);
                    }
                    cb(err, buf.toString(encoding), bytesRead);
                }), 3);
            } else {
                buffer = arg2;
                offset = arg3;
                length = arg4;
                position = arg5;
                newCb = wrapCb(cb, 3);
            }

            try  {
                checkFd(fd);
                if (position == null) {
                    position = fd.getPos();
                }
                fd.read(buffer, offset, length, position, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.readSync = function (fd, arg2, arg3, arg4, arg5) {
            var shenanigans = false;
            var buffer, offset, length, position;
            if (typeof arg2 === 'number') {
                length = arg2;
                position = arg3;
                var encoding = arg4;
                offset = 0;
                buffer = new Buffer(length);
                shenanigans = true;
            } else {
                buffer = arg2;
                offset = arg3;
                length = arg4;
                position = arg5;
            }
            checkFd(fd);
            if (position == null) {
                position = fd.getPos();
            }

            var rv = fd.readSync(buffer, offset, length, position);
            if (!shenanigans) {
                return rv;
            } else {
                return [buffer.toString(encoding), rv];
            }
        };

        /**
        * Asynchronous `fchown`.
        * @param [BrowserFS.File] fd
        * @param [Number] uid
        * @param [Number] gid
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.fchown = function (fd, uid, gid, callback) {
            if (typeof callback === "undefined") { callback = nopCb; }
            var newCb = wrapCb(callback, 1);
            try  {
                checkFd(fd);
                fd.chown(uid, gid, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `fchown`.
        * @param [BrowserFS.File] fd
        * @param [Number] uid
        * @param [Number] gid
        */
        fs.fchownSync = function (fd, uid, gid) {
            checkFd(fd);
            return fd.chownSync(uid, gid);
        };

        fs.fchmod = function (fd, mode, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
                checkFd(fd);
                fd.chmod(mode, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.fchmodSync = function (fd, mode) {
            mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
            checkFd(fd);
            return fd.chmodSync(mode);
        };

        fs.futimes = function (fd, atime, mtime, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                checkFd(fd);
                if (typeof atime === 'number') {
                    atime = new Date(atime * 1000);
                }
                if (typeof mtime === 'number') {
                    mtime = new Date(mtime * 1000);
                }
                fd.utimes(atime, mtime, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.futimesSync = function (fd, atime, mtime) {
            checkFd(fd);
            if (typeof atime === 'number') {
                atime = new Date(atime * 1000);
            }
            if (typeof mtime === 'number') {
                mtime = new Date(mtime * 1000);
            }
            return fd.utimesSync(atime, mtime);
        };

        // DIRECTORY-ONLY METHODS
        /**
        * Asynchronous `rmdir`.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.rmdir = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                path = normalizePath(path);
                fs.root.rmdir(path, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `rmdir`.
        * @param [String] path
        */
        fs.rmdirSync = function (path) {
            path = normalizePath(path);
            return fs.root.rmdirSync(path);
        };

        /**
        * Asynchronous `mkdir`.
        * @param [String] path
        * @param [Number?] mode defaults to `0777`
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.mkdir = function (path, mode, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            if (typeof mode === 'function') {
                cb = mode;
                mode = 0x1ff;
            }
            var newCb = wrapCb(cb, 1);
            try  {
                path = normalizePath(path);
                fs.root.mkdir(path, mode, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.mkdirSync = function (path, mode) {
            if (typeof mode === "undefined") { mode = 0x1ff; }
            mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
            path = normalizePath(path);
            return fs.root.mkdirSync(path, mode);
        };

        /**
        * Asynchronous `readdir`. Reads the contents of a directory.
        * The callback gets two arguments `(err, files)` where `files` is an array of
        * the names of the files in the directory excluding `'.'` and `'..'`.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError, String[])] callback
        */
        fs.readdir = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 2);
            try  {
                path = normalizePath(path);
                fs.root.readdir(path, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `readdir`. Reads the contents of a directory.
        * @param [String] path
        * @return [String[]]
        */
        fs.readdirSync = function (path) {
            path = normalizePath(path);
            return fs.root.readdirSync(path);
        };

        // SYMLINK METHODS
        /**
        * Asynchronous `link`.
        * @param [String] srcpath
        * @param [String] dstpath
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.link = function (srcpath, dstpath, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                srcpath = normalizePath(srcpath);
                dstpath = normalizePath(dstpath);
                fs.root.link(srcpath, dstpath, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `link`.
        * @param [String] srcpath
        * @param [String] dstpath
        */
        fs.linkSync = function (srcpath, dstpath) {
            srcpath = normalizePath(srcpath);
            dstpath = normalizePath(dstpath);
            return fs.root.linkSync(srcpath, dstpath);
        };

        fs.symlink = function (srcpath, dstpath, arg3, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var type = typeof arg3 === 'string' ? arg3 : 'file';
            cb = typeof arg3 === 'function' ? arg3 : cb;
            var newCb = wrapCb(cb, 1);
            try  {
                if (type !== 'file' && type !== 'dir') {
                    return newCb(new ApiError(9 /* EINVAL */, "Invalid type: " + type));
                }
                srcpath = normalizePath(srcpath);
                dstpath = normalizePath(dstpath);
                fs.root.symlink(srcpath, dstpath, type, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `symlink`.
        * @param [String] srcpath
        * @param [String] dstpath
        * @param [String?] type can be either `'dir'` or `'file'` (default is `'file'`)
        */
        fs.symlinkSync = function (srcpath, dstpath, type) {
            if (type == null) {
                type = 'file';
            } else if (type !== 'file' && type !== 'dir') {
                throw new ApiError(9 /* EINVAL */, "Invalid type: " + type);
            }
            srcpath = normalizePath(srcpath);
            dstpath = normalizePath(dstpath);
            return fs.root.symlinkSync(srcpath, dstpath, type);
        };

        /**
        * Asynchronous readlink.
        * @param [String] path
        * @param [Function(BrowserFS.ApiError, String)] callback
        */
        fs.readlink = function (path, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 2);
            try  {
                path = normalizePath(path);
                fs.root.readlink(path, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous readlink.
        * @param [String] path
        * @return [String]
        */
        fs.readlinkSync = function (path) {
            path = normalizePath(path);
            return fs.root.readlinkSync(path);
        };

        // PROPERTY OPERATIONS
        /**
        * Asynchronous `chown`.
        * @param [String] path
        * @param [Number] uid
        * @param [Number] gid
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.chown = function (path, uid, gid, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                path = normalizePath(path);
                fs.root.chown(path, false, uid, gid, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `chown`.
        * @param [String] path
        * @param [Number] uid
        * @param [Number] gid
        */
        fs.chownSync = function (path, uid, gid) {
            path = normalizePath(path);
            fs.root.chownSync(path, false, uid, gid);
        };

        /**
        * Asynchronous `lchown`.
        * @param [String] path
        * @param [Number] uid
        * @param [Number] gid
        * @param [Function(BrowserFS.ApiError)] callback
        */
        fs.lchown = function (path, uid, gid, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                path = normalizePath(path);
                fs.root.chown(path, true, uid, gid, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `lchown`.
        * @param [String] path
        * @param [Number] uid
        * @param [Number] gid
        */
        fs.lchownSync = function (path, uid, gid) {
            path = normalizePath(path);
            return fs.root.chownSync(path, true, uid, gid);
        };

        fs.chmod = function (path, mode, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
                path = normalizePath(path);
                fs.root.chmod(path, false, mode, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.chmodSync = function (path, mode) {
            mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
            path = normalizePath(path);
            return fs.root.chmodSync(path, false, mode);
        };

        fs.lchmod = function (path, mode, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
                path = normalizePath(path);
                fs.root.chmod(path, true, mode, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.lchmodSync = function (path, mode) {
            path = normalizePath(path);
            mode = typeof mode === 'string' ? parseInt(mode, 8) : mode;
            return fs.root.chmodSync(path, true, mode);
        };

        fs.utimes = function (path, atime, mtime, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var newCb = wrapCb(cb, 1);
            try  {
                path = normalizePath(path);
                if (typeof atime === 'number') {
                    atime = new Date(atime * 1000);
                }
                if (typeof mtime === 'number') {
                    mtime = new Date(mtime * 1000);
                }
                fs.root.utimes(path, atime, mtime, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        fs.utimesSync = function (path, atime, mtime) {
            path = normalizePath(path);
            if (typeof atime === 'number') {
                atime = new Date(atime * 1000);
            }
            if (typeof mtime === 'number') {
                mtime = new Date(mtime * 1000);
            }
            return fs.root.utimesSync(path, atime, mtime);
        };

        fs.realpath = function (path, arg2, cb) {
            if (typeof cb === "undefined") { cb = nopCb; }
            var cache = typeof arg2 === 'object' ? arg2 : {};
            cb = typeof arg2 === 'function' ? arg2 : nopCb;
            var newCb = wrapCb(cb, 2);
            try  {
                path = normalizePath(path);
                fs.root.realpath(path, cache, newCb);
            } catch (e) {
                newCb(e);
            }
        };

        /**
        * Synchronous `realpath`.
        * @param [String] path
        * @param [Object?] cache An object literal of mapped paths that can be used to
        *   force a specific path resolution or avoid additional `fs.stat` calls for
        *   known real paths.
        * @return [String]
        */
        fs.realpathSync = function (path, cache) {
            if (typeof cache === "undefined") { cache = {}; }
            path = normalizePath(path);
            return fs.root.realpathSync(path, cache);
        };
        fs.root = null;
        return fs;
    })();
    exports.fs = fs;
});
//# sourceMappingURL=node_fs.js.map
;
define('core/browserfs',["require", "exports", './buffer', './node_fs', './node_path', './node_process'], function(require, exports, buffer, node_fs, node_path, node_process) {
    /**
    * Installs BrowserFS onto the given object.
    * We recommend that you run install with the 'window' object to make things
    * global, as in Node.
    *
    * Properties installed:
    *
    * * Buffer
    * * process
    * * require (we monkey-patch it)
    *
    * This allows you to write code as if you were running inside Node.
    * @param {object} obj - The object to install things onto (e.g. window)
    */
    function install(obj) {
        obj.Buffer = buffer.Buffer;
        obj.process = node_process.process;
        var oldRequire = obj.require != null ? obj.require : null;

        // Monkey-patch require for Node-style code.
        obj.require = function (arg) {
            var rv = exports.BFSRequire(arg);
            if (rv == null) {
                return oldRequire.apply(null, Array.prototype.slice.call(arguments, 0));
            } else {
                return rv;
            }
        };
    }
    exports.install = install;

    exports.FileSystem = {};
    function registerFileSystem(name, fs) {
        exports.FileSystem[name] = fs;
    }
    exports.registerFileSystem = registerFileSystem;

    function BFSRequire(module) {
        switch (module) {
            case 'fs':
                return node_fs.fs;
            case 'path':
                return node_path.path;
            case 'buffer':
                // The 'buffer' module has 'Buffer' as a property.
                return buffer;
            case 'process':
                return node_process.process;
            default:
                return exports.FileSystem[module];
        }
    }
    exports.BFSRequire = BFSRequire;

    /**
    * You must call this function with a properly-instantiated root file system
    * before using any file system API method.
    * @param {BrowserFS.FileSystem} rootFS - The root filesystem to use for the
    *   entire BrowserFS file system.
    */
    function initialize(rootfs) {
        return node_fs.fs._initialize(rootfs);
    }
    exports.initialize = initialize;
});
//# sourceMappingURL=browserfs.js.map
;
define('generic/emscripten_fs',["require", "exports", '../core/browserfs', '../core/node_fs', '../core/buffer', '../core/buffer_core_arraybuffer'], function(require, exports, BrowserFS, node_fs, buffer, buffer_core_arraybuffer) {
    var Buffer = buffer.Buffer;
    var BufferCoreArrayBuffer = buffer_core_arraybuffer.BufferCoreArrayBuffer;
    var fs = node_fs.fs;

    var BFSEmscriptenStreamOps = (function () {
        function BFSEmscriptenStreamOps(fs) {
            this.fs = fs;
        }
        BFSEmscriptenStreamOps.prototype.open = function (stream) {
            var path = this.fs.realPath(stream.node);
            try  {
                if (FS.isFile(stream.node.mode)) {
                    stream.nfd = fs.openSync(path, this.fs.flagsToPermissionString(stream.flags));
                }
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenStreamOps.prototype.close = function (stream) {
            try  {
                if (FS.isFile(stream.node.mode) && stream.nfd) {
                    fs.closeSync(stream.nfd);
                }
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenStreamOps.prototype.read = function (stream, buffer, offset, length, position) {
            // Avoid copying overhead by reading directly into buffer.
            var bcore = new BufferCoreArrayBuffer(buffer.buffer);
            var nbuffer = new Buffer(bcore, buffer.byteOffset + offset, buffer.byteOffset + offset + length);
            var res;
            try  {
                res = fs.readSync(stream.nfd, nbuffer, 0, length, position);
            } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }

            // No copying needed, since we wrote directly into UintArray.
            return res;
        };

        BFSEmscriptenStreamOps.prototype.write = function (stream, buffer, offset, length, position) {
            // Avoid copying overhead; plug the buffer directly into a BufferCore.
            var bcore = new BufferCoreArrayBuffer(buffer.buffer);
            var nbuffer = new Buffer(bcore, buffer.byteOffset + offset, buffer.byteOffset + offset + length);
            var res;
            try  {
                res = fs.writeSync(stream.nfd, nbuffer, 0, length, position);
            } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return res;
        };

        BFSEmscriptenStreamOps.prototype.llseek = function (stream, offset, whence) {
            var position = offset;
            if (whence === 1) {
                position += stream.position;
            } else if (whence === 2) {
                if (FS.isFile(stream.node.mode)) {
                    try  {
                        var stat = fs.fstatSync(stream.nfd);
                        position += stat.size;
                    } catch (e) {
                        throw new FS.ErrnoError(ERRNO_CODES[e.code]);
                    }
                }
            }

            if (position < 0) {
                throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
            }

            stream.position = position;
            return position;
        };
        return BFSEmscriptenStreamOps;
    })();

    var BFSEmscriptenNodeOps = (function () {
        function BFSEmscriptenNodeOps(fs) {
            this.fs = fs;
        }
        BFSEmscriptenNodeOps.prototype.getattr = function (node) {
            var path = this.fs.realPath(node);
            var stat;
            try  {
                stat = fs.lstatSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return {
                dev: stat.dev,
                ino: stat.ino,
                mode: stat.mode,
                nlink: stat.nlink,
                uid: stat.uid,
                gid: stat.gid,
                rdev: stat.rdev,
                size: stat.size,
                atime: stat.atime,
                mtime: stat.mtime,
                ctime: stat.ctime,
                blksize: stat.blksize,
                blocks: stat.blocks
            };
        };

        BFSEmscriptenNodeOps.prototype.setattr = function (node, attr) {
            var path = this.fs.realPath(node);
            try  {
                if (attr.mode !== undefined) {
                    fs.chmodSync(path, attr.mode);

                    // update the common node structure mode as well
                    node.mode = attr.mode;
                }
                if (attr.timestamp !== undefined) {
                    var date = new Date(attr.timestamp);
                    fs.utimesSync(path, date, date);
                }
                if (attr.size !== undefined) {
                    fs.truncateSync(path, attr.size);
                }
            } catch (e) {
                if (!e.code)
                    throw e;
                if (e.code === "ENOTSUP") {
                    // Ignore not supported errors. Emscripten does utimesSync when it
                    // writes files, but never really requires the value to be set.
                    return;
                }
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.lookup = function (parent, name) {
            var path = PATH.join2(this.fs.realPath(parent), name);
            var mode = this.fs.getMode(path);
            return this.fs.createNode(parent, name, mode);
        };

        BFSEmscriptenNodeOps.prototype.mknod = function (parent, name, mode, dev) {
            var node = this.fs.createNode(parent, name, mode, dev);

            // create the backing node for this in the fs root as well
            var path = this.fs.realPath(node);
            try  {
                if (FS.isDir(node.mode)) {
                    fs.mkdirSync(path, node.mode);
                } else {
                    fs.writeFileSync(path, '', { mode: node.mode });
                }
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return node;
        };

        BFSEmscriptenNodeOps.prototype.rename = function (oldNode, newDir, newName) {
            var oldPath = this.fs.realPath(oldNode);
            var newPath = PATH.join2(this.fs.realPath(newDir), newName);
            try  {
                fs.renameSync(oldPath, newPath);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.unlink = function (parent, name) {
            var path = PATH.join2(this.fs.realPath(parent), name);
            try  {
                fs.unlinkSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.rmdir = function (parent, name) {
            var path = PATH.join2(this.fs.realPath(parent), name);
            try  {
                fs.rmdirSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.readdir = function (node) {
            var path = this.fs.realPath(node);
            try  {
                return fs.readdirSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.symlink = function (parent, newName, oldPath) {
            var newPath = PATH.join2(this.fs.realPath(parent), newName);
            try  {
                fs.symlinkSync(oldPath, newPath);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };

        BFSEmscriptenNodeOps.prototype.readlink = function (node) {
            var path = this.fs.realPath(node);
            try  {
                return fs.readlinkSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        };
        return BFSEmscriptenNodeOps;
    })();

    var BFSEmscriptenFS = (function () {
        function BFSEmscriptenFS() {
            // This maps the integer permission modes from http://linux.die.net/man/3/open
            // to node.js-specific file open permission strings at http://nodejs.org/api/fs.html#fs_fs_open_path_flags_mode_callback
            this.flagsToPermissionStringMap = {
                0: 'r',
                1: 'r+',
                2: 'r+',
                64: 'r',
                65: 'r+',
                66: 'r+',
                129: 'rx+',
                193: 'rx+',
                514: 'w+',
                577: 'w',
                578: 'w+',
                705: 'wx',
                706: 'wx+',
                1024: 'a',
                1025: 'a',
                1026: 'a+',
                1089: 'a',
                1090: 'a+',
                1153: 'ax',
                1154: 'ax+',
                1217: 'ax',
                1218: 'ax+',
                4096: 'rs',
                4098: 'rs+'
            };
            this.node_ops = new BFSEmscriptenNodeOps(this);
            this.stream_ops = new BFSEmscriptenStreamOps(this);
            if (typeof BrowserFS === 'undefined') {
                throw new Error("BrowserFS is not loaded. Please load it before this library.");
            }
        }
        BFSEmscriptenFS.prototype.mount = function (mount) {
            return this.createNode(null, '/', this.getMode(mount.opts.root), 0);
        };

        BFSEmscriptenFS.prototype.createNode = function (parent, name, mode, dev) {
            if (!FS.isDir(mode) && !FS.isFile(mode) && !FS.isLink(mode)) {
                throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
            }
            var node = FS.createNode(parent, name, mode);
            node.node_ops = this.node_ops;
            node.stream_ops = this.stream_ops;
            return node;
        };

        BFSEmscriptenFS.prototype.getMode = function (path) {
            var stat;
            try  {
                stat = fs.lstatSync(path);
            } catch (e) {
                if (!e.code)
                    throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return stat.mode;
        };

        BFSEmscriptenFS.prototype.realPath = function (node) {
            var parts = [];
            while (node.parent !== node) {
                parts.push(node.name);
                node = node.parent;
            }
            parts.push(node.mount.opts.root);
            parts.reverse();
            return PATH.join.apply(null, parts);
        };

        BFSEmscriptenFS.prototype.flagsToPermissionString = function (flags) {
            if (flags in this.flagsToPermissionStringMap) {
                return this.flagsToPermissionStringMap[flags];
            } else {
                return flags;
            }
        };
        return BFSEmscriptenFS;
    })();
    exports.BFSEmscriptenFS = BFSEmscriptenFS;

    // Make it available on the global BrowserFS object.
    BrowserFS['EmscriptenFS'] = BFSEmscriptenFS;
});
//# sourceMappingURL=emscripten_fs.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('core/file_system',["require", "exports", './api_error', './file_flag', './node_path', './buffer'], function(require, exports, api_error, file_flag, node_path, buffer) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var path = node_path.path;
    var Buffer = buffer.Buffer;
    var ActionType = file_flag.ActionType;

    

    

    /**
    * Basic filesystem class. Most filesystems should extend this class, as it
    * provides default implementations for a handful of methods.
    */
    var BaseFileSystem = (function () {
        function BaseFileSystem() {
        }
        BaseFileSystem.prototype.supportsLinks = function () {
            return false;
        };
        BaseFileSystem.prototype.diskSpace = function (p, cb) {
            cb(0, 0);
        };

        /**
        * Opens the file at path p with the given flag. The file must exist.
        * @param p The path to open.
        * @param flag The flag to use when opening the file.
        */
        BaseFileSystem.prototype.openFile = function (p, flag, cb) {
            throw new ApiError(14 /* ENOTSUP */);
        };

        /**
        * Create the file at path p with the given mode. Then, open it with the given
        * flag.
        */
        BaseFileSystem.prototype.createFile = function (p, flag, mode, cb) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.open = function (p, flag, mode, cb) {
            var _this = this;
            var must_be_file = function (e, stats) {
                if (e) {
                    switch (flag.pathNotExistsAction()) {
                        case 3 /* CREATE_FILE */:
                            // Ensure parent exists.
                            return _this.stat(path.dirname(p), false, function (e, parentStats) {
                                if (e) {
                                    cb(e);
                                } else if (!parentStats.isDirectory()) {
                                    cb(new ApiError(7 /* ENOTDIR */, path.dirname(p) + " is not a directory."));
                                } else {
                                    _this.createFile(p, flag, mode, cb);
                                }
                            });
                        case 1 /* THROW_EXCEPTION */:
                            return cb(new ApiError(1 /* ENOENT */, "" + p + " doesn't exist."));
                        default:
                            return cb(new ApiError(9 /* EINVAL */, 'Invalid FileFlag object.'));
                    }
                } else {
                    // File exists.
                    if (stats.isDirectory()) {
                        return cb(new ApiError(8 /* EISDIR */, p + " is a directory."));
                    }
                    switch (flag.pathExistsAction()) {
                        case 1 /* THROW_EXCEPTION */:
                            return cb(new ApiError(6 /* EEXIST */, p + " already exists."));
                        case 2 /* TRUNCATE_FILE */:
                            // NOTE: In a previous implementation, we deleted the file and
                            // re-created it. However, this created a race condition if another
                            // asynchronous request was trying to read the file, as the file
                            // would not exist for a small period of time.
                            return _this.openFile(p, flag, function (e, fd) {
                                if (e) {
                                    cb(e);
                                } else {
                                    fd.truncate(0, function () {
                                        fd.sync(function () {
                                            cb(null, fd);
                                        });
                                    });
                                }
                            });
                        case 0 /* NOP */:
                            return _this.openFile(p, flag, cb);
                        default:
                            return cb(new ApiError(9 /* EINVAL */, 'Invalid FileFlag object.'));
                    }
                }
            };
            this.stat(p, false, must_be_file);
        };
        BaseFileSystem.prototype.rename = function (oldPath, newPath, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.renameSync = function (oldPath, newPath) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.stat = function (p, isLstat, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.statSync = function (p, isLstat) {
            throw new ApiError(14 /* ENOTSUP */);
        };

        /**
        * Opens the file at path p with the given flag. The file must exist.
        * @param p The path to open.
        * @param flag The flag to use when opening the file.
        * @return A File object corresponding to the opened file.
        */
        BaseFileSystem.prototype.openFileSync = function (p, flag) {
            throw new ApiError(14 /* ENOTSUP */);
        };

        /**
        * Create the file at path p with the given mode. Then, open it with the given
        * flag.
        */
        BaseFileSystem.prototype.createFileSync = function (p, flag, mode) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.openSync = function (p, flag, mode) {
            // Check if the path exists, and is a file.
            var stats;
            try  {
                stats = this.statSync(p, false);
            } catch (e) {
                switch (flag.pathNotExistsAction()) {
                    case 3 /* CREATE_FILE */:
                        // Ensure parent exists.
                        var parentStats = this.statSync(path.dirname(p), false);
                        if (!parentStats.isDirectory()) {
                            throw new ApiError(7 /* ENOTDIR */, path.dirname(p) + " is not a directory.");
                        }
                        return this.createFileSync(p, flag, mode);
                    case 1 /* THROW_EXCEPTION */:
                        throw new ApiError(1 /* ENOENT */, "" + p + " doesn't exist.");
                    default:
                        throw new ApiError(9 /* EINVAL */, 'Invalid FileFlag object.');
                }
            }

            // File exists.
            if (stats.isDirectory()) {
                throw new ApiError(8 /* EISDIR */, p + " is a directory.");
            }
            switch (flag.pathExistsAction()) {
                case 1 /* THROW_EXCEPTION */:
                    throw new ApiError(6 /* EEXIST */, p + " already exists.");
                case 2 /* TRUNCATE_FILE */:
                    // Delete file.
                    this.unlinkSync(p);

                    // Create file. Use the same mode as the old file.
                    // Node itself modifies the ctime when this occurs, so this action
                    // will preserve that behavior if the underlying file system
                    // supports those properties.
                    return this.createFileSync(p, flag, stats.mode);
                case 0 /* NOP */:
                    return this.openFileSync(p, flag);
                default:
                    throw new ApiError(9 /* EINVAL */, 'Invalid FileFlag object.');
            }
        };
        BaseFileSystem.prototype.unlink = function (p, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.unlinkSync = function (p) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.rmdir = function (p, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.rmdirSync = function (p) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.mkdir = function (p, mode, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.mkdirSync = function (p, mode) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.readdir = function (p, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.readdirSync = function (p) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.exists = function (p, cb) {
            this.stat(p, null, function (err) {
                cb(err == null);
            });
        };
        BaseFileSystem.prototype.existsSync = function (p) {
            try  {
                this.statSync(p, true);
                return true;
            } catch (e) {
                return false;
            }
        };
        BaseFileSystem.prototype.realpath = function (p, cache, cb) {
            if (this.supportsLinks()) {
                // The path could contain symlinks. Split up the path,
                // resolve any symlinks, return the resolved string.
                var splitPath = p.split(path.sep);

                for (var i = 0; i < splitPath.length; i++) {
                    var addPaths = splitPath.slice(0, i + 1);
                    splitPath[i] = path.join.apply(null, addPaths);
                }
            } else {
                // No symlinks. We just need to verify that it exists.
                this.exists(p, function (doesExist) {
                    if (doesExist) {
                        cb(null, p);
                    } else {
                        cb(new ApiError(1 /* ENOENT */, "File " + p + " not found."));
                    }
                });
            }
        };
        BaseFileSystem.prototype.realpathSync = function (p, cache) {
            if (this.supportsLinks()) {
                // The path could contain symlinks. Split up the path,
                // resolve any symlinks, return the resolved string.
                var splitPath = p.split(path.sep);

                for (var i = 0; i < splitPath.length; i++) {
                    var addPaths = splitPath.slice(0, i + 1);
                    splitPath[i] = path.join.apply(null, addPaths);
                }
            } else {
                // No symlinks. We just need to verify that it exists.
                if (this.existsSync(p)) {
                    return p;
                } else {
                    throw new ApiError(1 /* ENOENT */, "File " + p + " not found.");
                }
            }
        };
        BaseFileSystem.prototype.truncate = function (p, len, cb) {
            this.open(p, file_flag.FileFlag.getFileFlag('r+'), 0x1a4, (function (er, fd) {
                if (er) {
                    return cb(er);
                }
                fd.truncate(len, (function (er) {
                    fd.close((function (er2) {
                        cb(er || er2);
                    }));
                }));
            }));
        };
        BaseFileSystem.prototype.truncateSync = function (p, len) {
            var fd = this.openSync(p, file_flag.FileFlag.getFileFlag('r+'), 0x1a4);

            try  {
                fd.truncateSync(len);
            } catch (e) {
                throw e;
            } finally {
                fd.closeSync();
            }
        };
        BaseFileSystem.prototype.readFile = function (fname, encoding, flag, cb) {
            // Wrap cb in file closing code.
            var oldCb = cb;

            // Get file.
            this.open(fname, flag, 0x1a4, function (err, fd) {
                if (err) {
                    return cb(err);
                }
                cb = function (err, arg) {
                    fd.close(function (err2) {
                        if (err == null) {
                            err = err2;
                        }
                        return oldCb(err, arg);
                    });
                };
                fd.stat(function (err, stat) {
                    if (err != null) {
                        return cb(err);
                    }

                    // Allocate buffer.
                    var buf = new Buffer(stat.size);
                    fd.read(buf, 0, stat.size, 0, function (err) {
                        if (err != null) {
                            return cb(err);
                        } else if (encoding === null) {
                            return cb(err, buf);
                        }
                        try  {
                            cb(null, buf.toString(encoding));
                        } catch (e) {
                            cb(e);
                        }
                    });
                });
            });
        };
        BaseFileSystem.prototype.readFileSync = function (fname, encoding, flag) {
            // Get file.
            var fd = this.openSync(fname, flag, 0x1a4);
            try  {
                var stat = fd.statSync();

                // Allocate buffer.
                var buf = new Buffer(stat.size);
                fd.readSync(buf, 0, stat.size, 0);
                fd.closeSync();
                if (encoding === null) {
                    return buf;
                }
                return buf.toString(encoding);
            } finally {
                fd.closeSync();
            }
        };
        BaseFileSystem.prototype.writeFile = function (fname, data, encoding, flag, mode, cb) {
            // Wrap cb in file closing code.
            var oldCb = cb;

            // Get file.
            this.open(fname, flag, 0x1a4, function (err, fd) {
                if (err != null) {
                    return cb(err);
                }
                cb = function (err) {
                    fd.close(function (err2) {
                        oldCb(err != null ? err : err2);
                    });
                };

                try  {
                    if (typeof data === 'string') {
                        data = new Buffer(data, encoding);
                    }
                } catch (e) {
                    return cb(e);
                }

                // Write into file.
                fd.write(data, 0, data.length, 0, cb);
            });
        };
        BaseFileSystem.prototype.writeFileSync = function (fname, data, encoding, flag, mode) {
            // Get file.
            var fd = this.openSync(fname, flag, mode);
            try  {
                if (typeof data === 'string') {
                    data = new Buffer(data, encoding);
                }

                // Write into file.
                fd.writeSync(data, 0, data.length, 0);
            } finally {
                fd.closeSync();
            }
        };
        BaseFileSystem.prototype.appendFile = function (fname, data, encoding, flag, mode, cb) {
            // Wrap cb in file closing code.
            var oldCb = cb;
            this.open(fname, flag, mode, function (err, fd) {
                if (err != null) {
                    return cb(err);
                }
                cb = function (err) {
                    fd.close(function (err2) {
                        oldCb(err != null ? err : err2);
                    });
                };
                if (typeof data === 'string') {
                    data = new Buffer(data, encoding);
                }
                fd.write(data, 0, data.length, null, cb);
            });
        };
        BaseFileSystem.prototype.appendFileSync = function (fname, data, encoding, flag, mode) {
            var fd = this.openSync(fname, flag, mode);
            try  {
                if (typeof data === 'string') {
                    data = new Buffer(data, encoding);
                }
                fd.writeSync(data, 0, data.length, null);
            } finally {
                fd.closeSync();
            }
        };
        BaseFileSystem.prototype.chmod = function (p, isLchmod, mode, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.chmodSync = function (p, isLchmod, mode) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.chown = function (p, isLchown, uid, gid, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.chownSync = function (p, isLchown, uid, gid) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.utimes = function (p, atime, mtime, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.utimesSync = function (p, atime, mtime) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.link = function (srcpath, dstpath, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.linkSync = function (srcpath, dstpath) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.symlink = function (srcpath, dstpath, type, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.symlinkSync = function (srcpath, dstpath, type) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFileSystem.prototype.readlink = function (p, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFileSystem.prototype.readlinkSync = function (p) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        return BaseFileSystem;
    })();
    exports.BaseFileSystem = BaseFileSystem;

    /**
    * Implements the asynchronous API in terms of the synchronous API.
    * @class SynchronousFileSystem
    */
    var SynchronousFileSystem = (function (_super) {
        __extends(SynchronousFileSystem, _super);
        function SynchronousFileSystem() {
            _super.apply(this, arguments);
        }
        SynchronousFileSystem.prototype.supportsSynch = function () {
            return true;
        };

        SynchronousFileSystem.prototype.rename = function (oldPath, newPath, cb) {
            try  {
                this.renameSync(oldPath, newPath);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.stat = function (p, isLstat, cb) {
            try  {
                cb(null, this.statSync(p, isLstat));
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.open = function (p, flags, mode, cb) {
            try  {
                cb(null, this.openSync(p, flags, mode));
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.unlink = function (p, cb) {
            try  {
                this.unlinkSync(p);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.rmdir = function (p, cb) {
            try  {
                this.rmdirSync(p);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.mkdir = function (p, mode, cb) {
            try  {
                this.mkdirSync(p, mode);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.readdir = function (p, cb) {
            try  {
                cb(null, this.readdirSync(p));
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.chmod = function (p, isLchmod, mode, cb) {
            try  {
                this.chmodSync(p, isLchmod, mode);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.chown = function (p, isLchown, uid, gid, cb) {
            try  {
                this.chownSync(p, isLchown, uid, gid);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.utimes = function (p, atime, mtime, cb) {
            try  {
                this.utimesSync(p, atime, mtime);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.link = function (srcpath, dstpath, cb) {
            try  {
                this.linkSync(srcpath, dstpath);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.symlink = function (srcpath, dstpath, type, cb) {
            try  {
                this.symlinkSync(srcpath, dstpath, type);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        SynchronousFileSystem.prototype.readlink = function (p, cb) {
            try  {
                cb(null, this.readlinkSync(p));
            } catch (e) {
                cb(e);
            }
        };
        return SynchronousFileSystem;
    })(BaseFileSystem);
    exports.SynchronousFileSystem = SynchronousFileSystem;
});
//# sourceMappingURL=file_system.js.map
;
define('core/node_fs_stats',["require", "exports"], function(require, exports) {
    /**
    * Indicates the type of the given file. Applied to 'mode'.
    */
    (function (FileType) {
        FileType[FileType["FILE"] = 0x8000] = "FILE";
        FileType[FileType["DIRECTORY"] = 0x4000] = "DIRECTORY";
        FileType[FileType["SYMLINK"] = 0xA000] = "SYMLINK";
    })(exports.FileType || (exports.FileType = {}));
    var FileType = exports.FileType;

    /**
    * Emulation of Node's `fs.Stats` object.
    *
    * Attribute descriptions are from `man 2 stat'
    * @see http://nodejs.org/api/fs.html#fs_class_fs_stats
    * @see http://man7.org/linux/man-pages/man2/stat.2.html
    * @class
    */
    var Stats = (function () {
        /**
        * Provides information about a particular entry in the file system.
        * @param [Number] item_type type of the item (FILE, DIRECTORY, SYMLINK, or SOCKET)
        * @param [Number] size Size of the item in bytes. For directories/symlinks,
        *   this is normally the size of the struct that represents the item.
        * @param [Number] mode Unix-style file mode (e.g. 0o644)
        * @param [Date?] atime time of last access
        * @param [Date?] mtime time of last modification
        * @param [Date?] ctime time of creation
        */
        function Stats(item_type, size, mode, atime, mtime, ctime) {
            if (typeof atime === "undefined") { atime = new Date(); }
            if (typeof mtime === "undefined") { mtime = new Date(); }
            if (typeof ctime === "undefined") { ctime = new Date(); }
            this.size = size;
            this.mode = mode;
            this.atime = atime;
            this.mtime = mtime;
            this.ctime = ctime;
            /**
            * UNSUPPORTED ATTRIBUTES
            * I assume no one is going to need these details, although we could fake
            * appropriate values if need be.
            */
            // ID of device containing file
            this.dev = 0;
            // inode number
            this.ino = 0;
            // device ID (if special file)
            this.rdev = 0;
            // number of hard links
            this.nlink = 1;
            // blocksize for file system I/O
            this.blksize = 4096;
            // @todo Maybe support these? atm, it's a one-user filesystem.
            // user ID of owner
            this.uid = 0;
            // group ID of owner
            this.gid = 0;
            if (this.mode == null) {
                switch (item_type) {
                    case 32768 /* FILE */:
                        this.mode = 0x1a4;
                        break;
                    case 16384 /* DIRECTORY */:
                    default:
                        this.mode = 0x1ff;
                }
            }

            // number of 512B blocks allocated
            this.blocks = Math.ceil(size / 512);

            // Check if mode also includes top-most bits, which indicate the file's
            // type.
            if (this.mode < 0x1000) {
                this.mode |= item_type;
            }
        }
        /**
        * **Nonstandard**: Clone the stats object.
        * @return [BrowserFS.node.fs.Stats]
        */
        Stats.prototype.clone = function () {
            return new Stats(this.mode & 0xF000, this.size, this.mode & 0xFFF, this.atime, this.mtime, this.ctime);
        };

        /**
        * @return [Boolean] True if this item is a file.
        */
        Stats.prototype.isFile = function () {
            return (this.mode & 0xF000) === 32768 /* FILE */;
        };

        /**
        * @return [Boolean] True if this item is a directory.
        */
        Stats.prototype.isDirectory = function () {
            return (this.mode & 0xF000) === 16384 /* DIRECTORY */;
        };

        /**
        * @return [Boolean] True if this item is a symbolic link (only valid through lstat)
        */
        Stats.prototype.isSymbolicLink = function () {
            return (this.mode & 0xF000) === 40960 /* SYMLINK */;
        };

        /**
        * Change the mode of the file. We use this helper function to prevent messing
        * up the type of the file, which is encoded in mode.
        */
        Stats.prototype.chmod = function (mode) {
            this.mode = (this.mode & 0xF000) | mode;
        };

        // We don't support the following types of files.
        Stats.prototype.isSocket = function () {
            return false;
        };

        Stats.prototype.isBlockDevice = function () {
            return false;
        };

        Stats.prototype.isCharacterDevice = function () {
            return false;
        };

        Stats.prototype.isFIFO = function () {
            return false;
        };
        return Stats;
    })();
    exports.Stats = Stats;
});
//# sourceMappingURL=node_fs_stats.js.map
;
define('generic/inode',["require", "exports", '../core/node_fs_stats', '../core/buffer'], function(require, exports, node_fs_stats, buffer) {
    /**
    * Generic inode definition that can easily be serialized.
    */
    var Inode = (function () {
        function Inode(id, size, mode, atime, mtime, ctime) {
            this.id = id;
            this.size = size;
            this.mode = mode;
            this.atime = atime;
            this.mtime = mtime;
            this.ctime = ctime;
        }
        /**
        * Handy function that converts the Inode to a Node Stats object.
        */
        Inode.prototype.toStats = function () {
            return new node_fs_stats.Stats((this.mode & 0xF000) === 16384 /* DIRECTORY */ ? 16384 /* DIRECTORY */ : 32768 /* FILE */, this.size, this.mode, new Date(this.atime), new Date(this.mtime), new Date(this.ctime));
        };

        /**
        * Get the size of this Inode, in bytes.
        */
        Inode.prototype.getSize = function () {
            // ASSUMPTION: ID is ASCII (1 byte per char).
            return 30 + this.id.length;
        };

        /**
        * Writes the inode into the start of the buffer.
        */
        Inode.prototype.toBuffer = function (buff) {
            if (typeof buff === "undefined") { buff = new buffer.Buffer(this.getSize()); }
            buff.writeUInt32LE(this.size, 0);
            buff.writeUInt16LE(this.mode, 4);
            buff.writeDoubleLE(this.atime, 6);
            buff.writeDoubleLE(this.mtime, 14);
            buff.writeDoubleLE(this.ctime, 22);
            buff.write(this.id, 30, this.id.length, 'ascii');
            return buff;
        };

        /**
        * Updates the Inode using information from the stats object. Used by file
        * systems at sync time, e.g.:
        * - Program opens file and gets a File object.
        * - Program mutates file. File object is responsible for maintaining
        *   metadata changes locally -- typically in a Stats object.
        * - Program closes file. File object's metadata changes are synced with the
        *   file system.
        * @return True if any changes have occurred.
        */
        Inode.prototype.update = function (stats) {
            var hasChanged = false;
            if (this.size !== stats.size) {
                this.size = stats.size;
                hasChanged = true;
            }

            if (this.mode !== stats.mode) {
                this.mode = stats.mode;
                hasChanged = true;
            }

            var atimeMs = stats.atime.getTime();
            if (this.atime !== atimeMs) {
                this.atime = atimeMs;
                hasChanged = true;
            }

            var mtimeMs = stats.mtime.getTime();
            if (this.mtime !== mtimeMs) {
                this.mtime = mtimeMs;
                hasChanged = true;
            }

            var ctimeMs = stats.ctime.getTime();
            if (this.ctime !== ctimeMs) {
                this.ctime = ctimeMs;
                hasChanged = true;
            }

            return hasChanged;
        };

        /**
        * Converts the buffer into an Inode.
        */
        Inode.fromBuffer = function (buffer) {
            if (buffer === undefined) {
                throw new Error("NO");
            }
            return new Inode(buffer.toString('ascii', 30), buffer.readUInt32LE(0), buffer.readUInt16LE(4), buffer.readDoubleLE(6), buffer.readDoubleLE(14), buffer.readDoubleLE(22));
        };

        // XXX: Copied from Stats. Should reconcile these two into something more
        //      compact.
        /**
        * @return [Boolean] True if this item is a file.
        */
        Inode.prototype.isFile = function () {
            return (this.mode & 0xF000) === 32768 /* FILE */;
        };

        /**
        * @return [Boolean] True if this item is a directory.
        */
        Inode.prototype.isDirectory = function () {
            return (this.mode & 0xF000) === 16384 /* DIRECTORY */;
        };
        return Inode;
    })();

    
    return Inode;
});
//# sourceMappingURL=inode.js.map
;
define('core/file',["require", "exports", './api_error'], function(require, exports, api_error) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;

    /**
    * Base class that contains shared implementations of functions for the file
    * object.
    * @class
    */
    var BaseFile = (function () {
        function BaseFile() {
        }
        BaseFile.prototype.sync = function (cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFile.prototype.syncSync = function () {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFile.prototype.datasync = function (cb) {
            this.sync(cb);
        };
        BaseFile.prototype.datasyncSync = function () {
            return this.syncSync();
        };
        BaseFile.prototype.chown = function (uid, gid, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFile.prototype.chownSync = function (uid, gid) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFile.prototype.chmod = function (mode, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFile.prototype.chmodSync = function (mode) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        BaseFile.prototype.utimes = function (atime, mtime, cb) {
            cb(new ApiError(14 /* ENOTSUP */));
        };
        BaseFile.prototype.utimesSync = function (atime, mtime) {
            throw new ApiError(14 /* ENOTSUP */);
        };
        return BaseFile;
    })();
    exports.BaseFile = BaseFile;
});
//# sourceMappingURL=file.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('generic/preload_file',["require", "exports", '../core/file', '../core/buffer', '../core/api_error', '../core/node_fs'], function(require, exports, file, buffer, api_error, node_fs) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var fs = node_fs.fs;
    var Buffer = buffer.Buffer;

    /**
    * An implementation of the File interface that operates on a file that is
    * completely in-memory. PreloadFiles are backed by a Buffer.
    *
    * This is also an abstract class, as it lacks an implementation of 'sync' and
    * 'close'. Each filesystem that wishes to use this file representation must
    * extend this class and implement those two methods.
    * @todo 'close' lever that disables functionality once closed.
    */
    var PreloadFile = (function (_super) {
        __extends(PreloadFile, _super);
        /**
        * Creates a file with the given path and, optionally, the given contents. Note
        * that, if contents is specified, it will be mutated by the file!
        * @param [BrowserFS.FileSystem] _fs The file system that created the file.
        * @param [String] _path
        * @param [BrowserFS.FileMode] _mode The mode that the file was opened using.
        *   Dictates permissions and where the file pointer starts.
        * @param [BrowserFS.node.fs.Stats] _stat The stats object for the given file.
        *   PreloadFile will mutate this object. Note that this object must contain
        *   the appropriate mode that the file was opened as.
        * @param [BrowserFS.node.Buffer?] contents A buffer containing the entire
        *   contents of the file. PreloadFile will mutate this buffer. If not
        *   specified, we assume it is a new file.
        */
        function PreloadFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this);
            this._pos = 0;
            this._fs = _fs;
            this._path = _path;
            this._flag = _flag;
            this._stat = _stat;
            if (contents != null) {
                this._buffer = contents;
            } else {
                // Empty buffer. It'll expand once we write stuff to it.
                this._buffer = new Buffer(0);
            }

            // Note: This invariant is *not* maintained once the file starts getting
            // modified.
            if (this._stat.size !== this._buffer.length) {
                throw new Error("Invalid buffer: Buffer is " + this._buffer.length + " long, yet Stats object specifies that file is " + this._stat.size + " long.");
            }
        }
        /**
        * Get the path to this file.
        * @return [String] The path to the file.
        */
        PreloadFile.prototype.getPath = function () {
            return this._path;
        };

        /**
        * Get the current file position.
        *
        * We emulate the following bug mentioned in the Node documentation:
        * > On Linux, positional writes don't work when the file is opened in append
        *   mode. The kernel ignores the position argument and always appends the data
        *   to the end of the file.
        * @return [Number] The current file position.
        */
        PreloadFile.prototype.getPos = function () {
            if (this._flag.isAppendable()) {
                return this._stat.size;
            }
            return this._pos;
        };

        /**
        * Advance the current file position by the indicated number of positions.
        * @param [Number] delta
        */
        PreloadFile.prototype.advancePos = function (delta) {
            return this._pos += delta;
        };

        /**
        * Set the file position.
        * @param [Number] newPos
        */
        PreloadFile.prototype.setPos = function (newPos) {
            return this._pos = newPos;
        };

        /**
        * **Core**: Asynchronous sync. Must be implemented by subclasses of this
        * class.
        * @param [Function(BrowserFS.ApiError)] cb
        */
        PreloadFile.prototype.sync = function (cb) {
            try  {
                this.syncSync();
                cb();
            } catch (e) {
                cb(e);
            }
        };

        /**
        * **Core**: Synchronous sync.
        */
        PreloadFile.prototype.syncSync = function () {
            throw new ApiError(14 /* ENOTSUP */);
        };

        /**
        * **Core**: Asynchronous close. Must be implemented by subclasses of this
        * class.
        * @param [Function(BrowserFS.ApiError)] cb
        */
        PreloadFile.prototype.close = function (cb) {
            try  {
                this.closeSync();
                cb();
            } catch (e) {
                cb(e);
            }
        };

        /**
        * **Core**: Synchronous close.
        */
        PreloadFile.prototype.closeSync = function () {
            throw new ApiError(14 /* ENOTSUP */);
        };

        /**
        * Asynchronous `stat`.
        * @param [Function(BrowserFS.ApiError, BrowserFS.node.fs.Stats)] cb
        */
        PreloadFile.prototype.stat = function (cb) {
            try  {
                cb(null, this._stat.clone());
            } catch (e) {
                cb(e);
            }
        };

        /**
        * Synchronous `stat`.
        */
        PreloadFile.prototype.statSync = function () {
            return this._stat.clone();
        };

        /**
        * Asynchronous truncate.
        * @param [Number] len
        * @param [Function(BrowserFS.ApiError)] cb
        */
        PreloadFile.prototype.truncate = function (len, cb) {
            try  {
                this.truncateSync(len);
                if (this._flag.isSynchronous() && !fs.getRootFS().supportsSynch()) {
                    this.sync(cb);
                }
                cb();
            } catch (e) {
                return cb(e);
            }
        };

        /**
        * Synchronous truncate.
        * @param [Number] len
        */
        PreloadFile.prototype.truncateSync = function (len) {
            if (!this._flag.isWriteable()) {
                throw new ApiError(0 /* EPERM */, 'File not opened with a writeable mode.');
            }
            this._stat.mtime = new Date();
            if (len > this._buffer.length) {
                var buf = new Buffer(len - this._buffer.length);
                buf.fill(0);

                // Write will set @_stat.size for us.
                this.writeSync(buf, 0, buf.length, this._buffer.length);
                if (this._flag.isSynchronous() && fs.getRootFS().supportsSynch()) {
                    this.syncSync();
                }
                return;
            }
            this._stat.size = len;

            // Truncate buffer to 'len'.
            var newBuff = new Buffer(len);
            this._buffer.copy(newBuff, 0, 0, len);
            this._buffer = newBuff;
            if (this._flag.isSynchronous() && fs.getRootFS().supportsSynch()) {
                this.syncSync();
            }
        };

        /**
        * Write buffer to the file.
        * Note that it is unsafe to use fs.write multiple times on the same file
        * without waiting for the callback.
        * @param [BrowserFS.node.Buffer] buffer Buffer containing the data to write to
        *  the file.
        * @param [Number] offset Offset in the buffer to start reading data from.
        * @param [Number] length The amount of bytes to write to the file.
        * @param [Number] position Offset from the beginning of the file where this
        *   data should be written. If position is null, the data will be written at
        *   the current position.
        * @param [Function(BrowserFS.ApiError, Number, BrowserFS.node.Buffer)]
        *   cb The number specifies the number of bytes written into the file.
        */
        PreloadFile.prototype.write = function (buffer, offset, length, position, cb) {
            try  {
                cb(null, this.writeSync(buffer, offset, length, position), buffer);
            } catch (e) {
                cb(e);
            }
        };

        /**
        * Write buffer to the file.
        * Note that it is unsafe to use fs.writeSync multiple times on the same file
        * without waiting for the callback.
        * @param [BrowserFS.node.Buffer] buffer Buffer containing the data to write to
        *  the file.
        * @param [Number] offset Offset in the buffer to start reading data from.
        * @param [Number] length The amount of bytes to write to the file.
        * @param [Number] position Offset from the beginning of the file where this
        *   data should be written. If position is null, the data will be written at
        *   the current position.
        * @return [Number]
        */
        PreloadFile.prototype.writeSync = function (buffer, offset, length, position) {
            if (position == null) {
                position = this.getPos();
            }
            if (!this._flag.isWriteable()) {
                throw new ApiError(0 /* EPERM */, 'File not opened with a writeable mode.');
            }
            var endFp = position + length;
            if (endFp > this._stat.size) {
                this._stat.size = endFp;
                if (endFp > this._buffer.length) {
                    // Extend the buffer!
                    var newBuff = new Buffer(endFp);
                    this._buffer.copy(newBuff);
                    this._buffer = newBuff;
                }
            }
            var len = buffer.copy(this._buffer, position, offset, offset + length);
            this._stat.mtime = new Date();
            if (this._flag.isSynchronous()) {
                this.syncSync();
                return len;
            }
            this.setPos(position + len);
            return len;
        };

        /**
        * Read data from the file.
        * @param [BrowserFS.node.Buffer] buffer The buffer that the data will be
        *   written to.
        * @param [Number] offset The offset within the buffer where writing will
        *   start.
        * @param [Number] length An integer specifying the number of bytes to read.
        * @param [Number] position An integer specifying where to begin reading from
        *   in the file. If position is null, data will be read from the current file
        *   position.
        * @param [Function(BrowserFS.ApiError, Number, BrowserFS.node.Buffer)] cb The
        *   number is the number of bytes read
        */
        PreloadFile.prototype.read = function (buffer, offset, length, position, cb) {
            try  {
                cb(null, this.readSync(buffer, offset, length, position), buffer);
            } catch (e) {
                cb(e);
            }
        };

        /**
        * Read data from the file.
        * @param [BrowserFS.node.Buffer] buffer The buffer that the data will be
        *   written to.
        * @param [Number] offset The offset within the buffer where writing will
        *   start.
        * @param [Number] length An integer specifying the number of bytes to read.
        * @param [Number] position An integer specifying where to begin reading from
        *   in the file. If position is null, data will be read from the current file
        *   position.
        * @return [Number]
        */
        PreloadFile.prototype.readSync = function (buffer, offset, length, position) {
            if (!this._flag.isReadable()) {
                throw new ApiError(0 /* EPERM */, 'File not opened with a readable mode.');
            }
            if (position == null) {
                position = this.getPos();
            }
            var endRead = position + length;
            if (endRead > this._stat.size) {
                length = this._stat.size - position;
            }
            var rv = this._buffer.copy(buffer, offset, position, position + length);
            this._stat.atime = new Date();
            this._pos = position + length;
            return rv;
        };

        /**
        * Asynchronous `fchmod`.
        * @param [Number|String] mode
        * @param [Function(BrowserFS.ApiError)] cb
        */
        PreloadFile.prototype.chmod = function (mode, cb) {
            try  {
                this.chmodSync(mode);
                cb();
            } catch (e) {
                cb(e);
            }
        };

        /**
        * Asynchronous `fchmod`.
        * @param [Number] mode
        */
        PreloadFile.prototype.chmodSync = function (mode) {
            if (!this._fs.supportsProps()) {
                throw new ApiError(14 /* ENOTSUP */);
            }
            this._stat.chmod(mode);
            this.syncSync();
        };
        return PreloadFile;
    })(file.BaseFile);
    exports.PreloadFile = PreloadFile;

    /**
    * File class for the InMemory and XHR file systems.
    * Doesn't sync to anything, so it works nicely for memory-only files.
    */
    var NoSyncFile = (function (_super) {
        __extends(NoSyncFile, _super);
        function NoSyncFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this, _fs, _path, _flag, _stat, contents);
        }
        /**
        * Asynchronous sync. Doesn't do anything, simply calls the cb.
        * @param [Function(BrowserFS.ApiError)] cb
        */
        NoSyncFile.prototype.sync = function (cb) {
            cb();
        };

        /**
        * Synchronous sync. Doesn't do anything.
        */
        NoSyncFile.prototype.syncSync = function () {
        };

        /**
        * Asynchronous close. Doesn't do anything, simply calls the cb.
        * @param [Function(BrowserFS.ApiError)] cb
        */
        NoSyncFile.prototype.close = function (cb) {
            cb();
        };

        /**
        * Synchronous close. Doesn't do anything.
        */
        NoSyncFile.prototype.closeSync = function () {
        };
        return NoSyncFile;
    })(PreloadFile);
    exports.NoSyncFile = NoSyncFile;
});
//# sourceMappingURL=preload_file.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('generic/key_value_filesystem',["require", "exports", '../core/file_system', '../core/api_error', '../core/node_fs_stats', '../core/node_path', '../generic/inode', '../core/buffer', '../generic/preload_file'], function(require, exports, file_system, api_error, node_fs_stats, node_path, Inode, buffer, preload_file) {
    var ROOT_NODE_ID = "/", path = node_path.path, ApiError = api_error.ApiError, Buffer = buffer.Buffer;

    /**
    * Generates a random ID.
    */
    function GenerateRandomID() {
        // From http://stackoverflow.com/questions/105034/how-to-create-a-guid-uuid-in-javascript
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
            var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }

    /**
    * Helper function. Checks if 'e' is defined. If so, it triggers the callback
    * with 'e' and returns false. Otherwise, returns true.
    */
    function noError(e, cb) {
        if (e) {
            cb(e);
            return false;
        }
        return true;
    }

    /**
    * Helper function. Checks if 'e' is defined. If so, it aborts the transaction,
    * triggers the callback with 'e', and returns false. Otherwise, returns true.
    */
    function noErrorTx(e, tx, cb) {
        if (e) {
            tx.abort(function () {
                cb(e);
            });
            return false;
        }
        return true;
    }

    

    

    

    

    /**
    * A simple RW transaction for simple synchronous key-value stores.
    */
    var SimpleSyncRWTransaction = (function () {
        function SimpleSyncRWTransaction(store) {
            this.store = store;
            /**
            * Stores data in the keys we modify prior to modifying them.
            * Allows us to roll back commits.
            */
            this.originalData = {};
            /**
            * List of keys modified in this transaction, if any.
            */
            this.modifiedKeys = [];
        }
        /**
        * Stashes given key value pair into `originalData` if it doesn't already
        * exist. Allows us to stash values the program is requesting anyway to
        * prevent needless `get` requests if the program modifies the data later
        * on during the transaction.
        */
        SimpleSyncRWTransaction.prototype.stashOldValue = function (key, value) {
            // Keep only the earliest value in the transaction.
            if (!this.originalData.hasOwnProperty(key)) {
                this.originalData[key] = value;
            }
        };

        /**
        * Marks the given key as modified, and stashes its value if it has not been
        * stashed already.
        */
        SimpleSyncRWTransaction.prototype.markModified = function (key) {
            if (this.modifiedKeys.indexOf(key) === -1) {
                this.modifiedKeys.push(key);
                if (!this.originalData.hasOwnProperty(key)) {
                    this.originalData[key] = this.store.get(key);
                }
            }
        };

        SimpleSyncRWTransaction.prototype.get = function (key) {
            var val = this.store.get(key);
            this.stashOldValue(key, val);
            return val;
        };

        SimpleSyncRWTransaction.prototype.put = function (key, data, overwrite) {
            this.markModified(key);
            return this.store.put(key, data, overwrite);
        };

        SimpleSyncRWTransaction.prototype.delete = function (key) {
            this.markModified(key);
            this.store.delete(key);
        };

        SimpleSyncRWTransaction.prototype.commit = function () {
        };
        SimpleSyncRWTransaction.prototype.abort = function () {
            // Rollback old values.
            var i, key, value;
            for (i = 0; i < this.modifiedKeys.length; i++) {
                key = this.modifiedKeys[i];
                value = this.originalData[key];
                if (value === null) {
                    // Key didn't exist.
                    this.store.delete(key);
                } else {
                    // Key existed. Store old value.
                    this.store.put(key, value, true);
                }
            }
        };
        return SimpleSyncRWTransaction;
    })();
    exports.SimpleSyncRWTransaction = SimpleSyncRWTransaction;

    var SyncKeyValueFile = (function (_super) {
        __extends(SyncKeyValueFile, _super);
        function SyncKeyValueFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this, _fs, _path, _flag, _stat, contents);
        }
        SyncKeyValueFile.prototype.syncSync = function () {
            this._fs._syncSync(this._path, this._buffer, this._stat);
        };

        SyncKeyValueFile.prototype.closeSync = function () {
            this.syncSync();
        };
        return SyncKeyValueFile;
    })(preload_file.PreloadFile);
    exports.SyncKeyValueFile = SyncKeyValueFile;

    /**
    * A "Synchronous key-value file system". Stores data to/retrieves data from an
    * underlying key-value store.
    *
    * We use a unique ID for each node in the file system. The root node has a
    * fixed ID.
    * @todo Introduce Node ID caching.
    * @todo Check modes.
    */
    var SyncKeyValueFileSystem = (function (_super) {
        __extends(SyncKeyValueFileSystem, _super);
        function SyncKeyValueFileSystem(options) {
            _super.call(this);
            this.store = options.store;

            // INVARIANT: Ensure that the root exists.
            this.makeRootDirectory();
        }
        SyncKeyValueFileSystem.isAvailable = function () {
            return true;
        };
        SyncKeyValueFileSystem.prototype.getName = function () {
            return this.store.name();
        };
        SyncKeyValueFileSystem.prototype.isReadOnly = function () {
            return false;
        };
        SyncKeyValueFileSystem.prototype.supportsSymlinks = function () {
            return false;
        };
        SyncKeyValueFileSystem.prototype.supportsProps = function () {
            return false;
        };
        SyncKeyValueFileSystem.prototype.supportsSynch = function () {
            return true;
        };

        /**
        * Checks if the root directory exists. Creates it if it doesn't.
        */
        SyncKeyValueFileSystem.prototype.makeRootDirectory = function () {
            var tx = this.store.beginTransaction('readwrite');
            if (tx.get(ROOT_NODE_ID) === undefined) {
                // Create new inode.
                var currTime = (new Date()).getTime(), dirInode = new Inode(GenerateRandomID(), 4096, 511 | 16384 /* DIRECTORY */, currTime, currTime, currTime);

                // If the root doesn't exist, the first random ID shouldn't exist,
                // either.
                tx.put(dirInode.id, new Buffer("{}"), false);
                tx.put(ROOT_NODE_ID, dirInode.toBuffer(), false);
                tx.commit();
            }
        };

        /**
        * Helper function for findINode.
        * @param parent The parent directory of the file we are attempting to find.
        * @param filename The filename of the inode we are attempting to find, minus
        *   the parent.
        * @return string The ID of the file's inode in the file system.
        */
        SyncKeyValueFileSystem.prototype._findINode = function (tx, parent, filename) {
            var _this = this;
            var read_directory = function (inode) {
                // Get the root's directory listing.
                var dirList = _this.getDirListing(tx, parent, inode);

                // Get the file's ID.
                if (dirList[filename]) {
                    return dirList[filename];
                } else {
                    throw ApiError.ENOENT(path.resolve(parent, filename));
                }
            };
            if (parent === '/') {
                if (filename === '') {
                    // BASE CASE #1: Return the root's ID.
                    return ROOT_NODE_ID;
                } else {
                    // BASE CASE #2: Find the item in the root ndoe.
                    return read_directory(this.getINode(tx, parent, ROOT_NODE_ID));
                }
            } else {
                return read_directory(this.getINode(tx, parent + path.sep + filename, this._findINode(tx, path.dirname(parent), path.basename(parent))));
            }
        };

        /**
        * Finds the Inode of the given path.
        * @param p The path to look up.
        * @return The Inode of the path p.
        * @todo memoize/cache
        */
        SyncKeyValueFileSystem.prototype.findINode = function (tx, p) {
            return this.getINode(tx, p, this._findINode(tx, path.dirname(p), path.basename(p)));
        };

        /**
        * Given the ID of a node, retrieves the corresponding Inode.
        * @param tx The transaction to use.
        * @param p The corresponding path to the file (used for error messages).
        * @param id The ID to look up.
        */
        SyncKeyValueFileSystem.prototype.getINode = function (tx, p, id) {
            var inode = tx.get(id);
            if (inode === undefined) {
                throw ApiError.ENOENT(p);
            }
            return Inode.fromBuffer(inode);
        };

        /**
        * Given the Inode of a directory, retrieves the corresponding directory
        * listing.
        */
        SyncKeyValueFileSystem.prototype.getDirListing = function (tx, p, inode) {
            if (!inode.isDirectory()) {
                throw ApiError.ENOTDIR(p);
            }
            var data = tx.get(inode.id);
            if (data === undefined) {
                throw ApiError.ENOENT(p);
            }
            return JSON.parse(data.toString());
        };

        /**
        * Creates a new node under a random ID. Retries 5 times before giving up in
        * the exceedingly unlikely chance that we try to reuse a random GUID.
        * @return The GUID that the data was stored under.
        */
        SyncKeyValueFileSystem.prototype.addNewNode = function (tx, data) {
            var retries = 0, currId;
            while (retries < 5) {
                try  {
                    currId = GenerateRandomID();
                    tx.put(currId, data, false);
                    return currId;
                } catch (e) {
                    // Ignore and reroll.
                }
            }
            throw new ApiError(2 /* EIO */, 'Unable to commit data to key-value store.');
        };

        /**
        * Commits a new file (well, a FILE or a DIRECTORY) to the file system with
        * the given mode.
        * Note: This will commit the transaction.
        * @param p The path to the new file.
        * @param type The type of the new file.
        * @param mode The mode to create the new file with.
        * @param data The data to store at the file's data node.
        * @return The Inode for the new file.
        */
        SyncKeyValueFileSystem.prototype.commitNewFile = function (tx, p, type, mode, data) {
            var parentDir = path.dirname(p), fname = path.basename(p), parentNode = this.findINode(tx, parentDir), dirListing = this.getDirListing(tx, parentDir, parentNode), currTime = (new Date()).getTime();

            // Invariant: The root always exists.
            // If we don't check this prior to taking steps below, we will create a
            // file with name '' in root should p == '/'.
            if (p === '/') {
                throw ApiError.EEXIST(p);
            }

            // Check if file already exists.
            if (dirListing[fname]) {
                throw ApiError.EEXIST(p);
            }

            try  {
                // Commit data.
                var dataId = this.addNewNode(tx, data), fileNode = new Inode(dataId, data.length, mode | type, currTime, currTime, currTime), fileNodeId = this.addNewNode(tx, fileNode.toBuffer());

                // Update and commit parent directory listing.
                dirListing[fname] = fileNodeId;
                tx.put(parentNode.id, new Buffer(JSON.stringify(dirListing)), true);
            } catch (e) {
                tx.abort();
                throw e;
            }
            tx.commit();
            return fileNode;
        };

        /**
        * Delete all contents stored in the file system.
        */
        SyncKeyValueFileSystem.prototype.empty = function () {
            this.store.clear();

            // INVARIANT: Root always exists.
            this.makeRootDirectory();
        };

        SyncKeyValueFileSystem.prototype.renameSync = function (oldPath, newPath) {
            var tx = this.store.beginTransaction('readwrite'), oldParent = path.dirname(oldPath), oldName = path.basename(oldPath), newParent = path.dirname(newPath), newName = path.basename(newPath), oldDirNode = this.findINode(tx, oldParent), oldDirList = this.getDirListing(tx, oldParent, oldDirNode);
            if (!oldDirList[oldName]) {
                throw ApiError.ENOENT(oldPath);
            }
            var nodeId = oldDirList[oldName];
            delete oldDirList[oldName];

            // Invariant: Can't move a folder inside itself.
            // This funny little hack ensures that the check passes only if oldPath
            // is a subpath of newParent. We append '/' to avoid matching folders that
            // are a substring of the bottom-most folder in the path.
            if ((newParent + '/').indexOf(oldPath + '/') === 0) {
                throw new ApiError(5 /* EBUSY */, oldParent);
            }

            // Add newPath to parent's directory listing.
            var newDirNode, newDirList;
            if (newParent === oldParent) {
                // Prevent us from re-grabbing the same directory listing, which still
                // contains oldName.
                newDirNode = oldDirNode;
                newDirList = oldDirList;
            } else {
                newDirNode = this.findINode(tx, newParent);
                newDirList = this.getDirListing(tx, newParent, newDirNode);
            }

            if (newDirList[newName]) {
                // If it's a file, delete it.
                var newNameNode = this.getINode(tx, newPath, newDirList[newName]);
                if (newNameNode.isFile()) {
                    try  {
                        tx.delete(newNameNode.id);
                        tx.delete(newDirList[newName]);
                    } catch (e) {
                        tx.abort();
                        throw e;
                    }
                } else {
                    throw ApiError.EPERM(newPath);
                }
            }
            newDirList[newName] = nodeId;

            try  {
                tx.put(oldDirNode.id, new Buffer(JSON.stringify(oldDirList)), true);
                tx.put(newDirNode.id, new Buffer(JSON.stringify(newDirList)), true);
            } catch (e) {
                tx.abort();
                throw e;
            }

            tx.commit();
        };

        SyncKeyValueFileSystem.prototype.statSync = function (p, isLstat) {
            // Get the inode to the item, convert it into a Stats object.
            return this.findINode(this.store.beginTransaction('readonly'), p).toStats();
        };

        SyncKeyValueFileSystem.prototype.createFileSync = function (p, flag, mode) {
            var tx = this.store.beginTransaction('readwrite'), data = new Buffer(0), newFile = this.commitNewFile(tx, p, 32768 /* FILE */, mode, data);

            // Open the file.
            return new SyncKeyValueFile(this, p, flag, newFile.toStats(), data);
        };

        SyncKeyValueFileSystem.prototype.openFileSync = function (p, flag) {
            var tx = this.store.beginTransaction('readonly'), node = this.findINode(tx, p), data = tx.get(node.id);
            if (data === undefined) {
                throw ApiError.ENOENT(p);
            }
            return new SyncKeyValueFile(this, p, flag, node.toStats(), data);
        };

        /**
        * Remove all traces of the given path from the file system.
        * @param p The path to remove from the file system.
        * @param isDir Does the path belong to a directory, or a file?
        * @todo Update mtime.
        */
        SyncKeyValueFileSystem.prototype.removeEntry = function (p, isDir) {
            var tx = this.store.beginTransaction('readwrite'), parent = path.dirname(p), parentNode = this.findINode(tx, parent), parentListing = this.getDirListing(tx, parent, parentNode), fileName = path.basename(p);

            if (!parentListing[fileName]) {
                throw ApiError.ENOENT(p);
            }

            // Remove from directory listing of parent.
            var fileNodeId = parentListing[fileName];
            delete parentListing[fileName];

            // Get file inode.
            var fileNode = this.getINode(tx, p, fileNodeId);
            if (!isDir && fileNode.isDirectory()) {
                throw ApiError.EISDIR(p);
            } else if (isDir && !fileNode.isDirectory()) {
                throw ApiError.ENOTDIR(p);
            }

            try  {
                // Delete data.
                tx.delete(fileNode.id);

                // Delete node.
                tx.delete(fileNodeId);

                // Update directory listing.
                tx.put(parentNode.id, new Buffer(JSON.stringify(parentListing)), true);
            } catch (e) {
                tx.abort();
                throw e;
            }

            // Success.
            tx.commit();
        };

        SyncKeyValueFileSystem.prototype.unlinkSync = function (p) {
            this.removeEntry(p, false);
        };

        SyncKeyValueFileSystem.prototype.rmdirSync = function (p) {
            this.removeEntry(p, true);
        };

        SyncKeyValueFileSystem.prototype.mkdirSync = function (p, mode) {
            var tx = this.store.beginTransaction('readwrite'), data = new Buffer('{}');
            this.commitNewFile(tx, p, 16384 /* DIRECTORY */, mode, data);
        };

        SyncKeyValueFileSystem.prototype.readdirSync = function (p) {
            var tx = this.store.beginTransaction('readonly');
            return Object.keys(this.getDirListing(tx, p, this.findINode(tx, p)));
        };

        SyncKeyValueFileSystem.prototype._syncSync = function (p, data, stats) {
            // @todo Ensure mtime updates properly, and use that to determine if a data
            //       update is required.
            var tx = this.store.beginTransaction('readwrite'), fileInodeId = this._findINode(tx, path.dirname(p), path.basename(p)), fileInode = this.getINode(tx, p, fileInodeId), inodeChanged = fileInode.update(stats);

            try  {
                // Sync data.
                tx.put(fileInode.id, data, true);

                // Sync metadata.
                if (inodeChanged) {
                    tx.put(fileInodeId, fileInode.toBuffer(), true);
                }
            } catch (e) {
                tx.abort();
                throw e;
            }
            tx.commit();
        };
        return SyncKeyValueFileSystem;
    })(file_system.SynchronousFileSystem);
    exports.SyncKeyValueFileSystem = SyncKeyValueFileSystem;

    

    

    

    var AsyncKeyValueFile = (function (_super) {
        __extends(AsyncKeyValueFile, _super);
        function AsyncKeyValueFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this, _fs, _path, _flag, _stat, contents);
        }
        AsyncKeyValueFile.prototype.sync = function (cb) {
            this._fs._sync(this._path, this._buffer, this._stat, cb);
        };

        AsyncKeyValueFile.prototype.close = function (cb) {
            this.sync(cb);
        };
        return AsyncKeyValueFile;
    })(preload_file.PreloadFile);
    exports.AsyncKeyValueFile = AsyncKeyValueFile;

    /**
    * An "Asynchronous key-value file system". Stores data to/retrieves data from
    * an underlying asynchronous key-value store.
    */
    var AsyncKeyValueFileSystem = (function (_super) {
        __extends(AsyncKeyValueFileSystem, _super);
        function AsyncKeyValueFileSystem() {
            _super.apply(this, arguments);
        }
        /**
        * Initializes the file system. Typically called by subclasses' async
        * constructors.
        */
        AsyncKeyValueFileSystem.prototype.init = function (store, cb) {
            this.store = store;

            // INVARIANT: Ensure that the root exists.
            this.makeRootDirectory(cb);
        };

        AsyncKeyValueFileSystem.isAvailable = function () {
            return true;
        };
        AsyncKeyValueFileSystem.prototype.getName = function () {
            return this.store.name();
        };
        AsyncKeyValueFileSystem.prototype.isReadOnly = function () {
            return false;
        };
        AsyncKeyValueFileSystem.prototype.supportsSymlinks = function () {
            return false;
        };
        AsyncKeyValueFileSystem.prototype.supportsProps = function () {
            return false;
        };
        AsyncKeyValueFileSystem.prototype.supportsSynch = function () {
            return false;
        };

        /**
        * Checks if the root directory exists. Creates it if it doesn't.
        */
        AsyncKeyValueFileSystem.prototype.makeRootDirectory = function (cb) {
            var tx = this.store.beginTransaction('readwrite');
            tx.get(ROOT_NODE_ID, function (e, data) {
                if (e || data === undefined) {
                    // Create new inode.
                    var currTime = (new Date()).getTime(), dirInode = new Inode(GenerateRandomID(), 4096, 511 | 16384 /* DIRECTORY */, currTime, currTime, currTime);

                    // If the root doesn't exist, the first random ID shouldn't exist,
                    // either.
                    tx.put(dirInode.id, new Buffer("{}"), false, function (e) {
                        if (noErrorTx(e, tx, cb)) {
                            tx.put(ROOT_NODE_ID, dirInode.toBuffer(), false, function (e) {
                                if (e) {
                                    tx.abort(function () {
                                        cb(e);
                                    });
                                } else {
                                    tx.commit(cb);
                                }
                            });
                        }
                    });
                } else {
                    // We're good.
                    tx.commit(cb);
                }
            });
        };

        /**
        * Helper function for findINode.
        * @param parent The parent directory of the file we are attempting to find.
        * @param filename The filename of the inode we are attempting to find, minus
        *   the parent.
        * @param cb Passed an error or the ID of the file's inode in the file system.
        */
        AsyncKeyValueFileSystem.prototype._findINode = function (tx, parent, filename, cb) {
            var _this = this;
            var handle_directory_listings = function (e, inode, dirList) {
                if (e) {
                    cb(e);
                } else if (dirList[filename]) {
                    cb(null, dirList[filename]);
                } else {
                    cb(ApiError.ENOENT(path.resolve(parent, filename)));
                }
            };

            if (parent === '/') {
                if (filename === '') {
                    // BASE CASE #1: Return the root's ID.
                    cb(null, ROOT_NODE_ID);
                } else {
                    // BASE CASE #2: Find the item in the root node.
                    this.getINode(tx, parent, ROOT_NODE_ID, function (e, inode) {
                        if (noError(e, cb)) {
                            _this.getDirListing(tx, parent, inode, function (e, dirList) {
                                // handle_directory_listings will handle e for us.
                                handle_directory_listings(e, inode, dirList);
                            });
                        }
                    });
                }
            } else {
                // Get the parent directory's INode, and find the file in its directory
                // listing.
                this.findINodeAndDirListing(tx, parent, handle_directory_listings);
            }
        };

        /**
        * Finds the Inode of the given path.
        * @param p The path to look up.
        * @param cb Passed an error or the Inode of the path p.
        * @todo memoize/cache
        */
        AsyncKeyValueFileSystem.prototype.findINode = function (tx, p, cb) {
            var _this = this;
            this._findINode(tx, path.dirname(p), path.basename(p), function (e, id) {
                if (noError(e, cb)) {
                    _this.getINode(tx, p, id, cb);
                }
            });
        };

        /**
        * Given the ID of a node, retrieves the corresponding Inode.
        * @param tx The transaction to use.
        * @param p The corresponding path to the file (used for error messages).
        * @param id The ID to look up.
        * @param cb Passed an error or the inode under the given id.
        */
        AsyncKeyValueFileSystem.prototype.getINode = function (tx, p, id, cb) {
            tx.get(id, function (e, data) {
                if (noError(e, cb)) {
                    if (data === undefined) {
                        cb(ApiError.ENOENT(p));
                    } else {
                        cb(null, Inode.fromBuffer(data));
                    }
                }
            });
        };

        /**
        * Given the Inode of a directory, retrieves the corresponding directory
        * listing.
        */
        AsyncKeyValueFileSystem.prototype.getDirListing = function (tx, p, inode, cb) {
            if (!inode.isDirectory()) {
                cb(ApiError.ENOTDIR(p));
            } else {
                tx.get(inode.id, function (e, data) {
                    if (noError(e, cb)) {
                        try  {
                            cb(null, JSON.parse(data.toString()));
                        } catch (e) {
                            // Occurs when data is undefined, or corresponds to something other
                            // than a directory listing. The latter should never occur unless
                            // the file system is corrupted.
                            cb(ApiError.ENOENT(p));
                        }
                    }
                });
            }
        };

        /**
        * Given a path to a directory, retrieves the corresponding INode and
        * directory listing.
        */
        AsyncKeyValueFileSystem.prototype.findINodeAndDirListing = function (tx, p, cb) {
            var _this = this;
            this.findINode(tx, p, function (e, inode) {
                if (noError(e, cb)) {
                    _this.getDirListing(tx, p, inode, function (e, listing) {
                        if (noError(e, cb)) {
                            cb(null, inode, listing);
                        }
                    });
                }
            });
        };

        /**
        * Adds a new node under a random ID. Retries 5 times before giving up in
        * the exceedingly unlikely chance that we try to reuse a random GUID.
        * @param cb Passed an error or the GUID that the data was stored under.
        */
        AsyncKeyValueFileSystem.prototype.addNewNode = function (tx, data, cb) {
            var retries = 0, currId, reroll = function () {
                if (++retries === 5) {
                    // Max retries hit. Return with an error.
                    cb(new ApiError(2 /* EIO */, 'Unable to commit data to key-value store.'));
                } else {
                    // Try again.
                    currId = GenerateRandomID();
                    tx.put(currId, data, false, function (e, committed) {
                        if (e || !committed) {
                            reroll();
                        } else {
                            // Successfully stored under 'currId'.
                            cb(null, currId);
                        }
                    });
                }
            };
            reroll();
        };

        /**
        * Commits a new file (well, a FILE or a DIRECTORY) to the file system with
        * the given mode.
        * Note: This will commit the transaction.
        * @param p The path to the new file.
        * @param type The type of the new file.
        * @param mode The mode to create the new file with.
        * @param data The data to store at the file's data node.
        * @param cb Passed an error or the Inode for the new file.
        */
        AsyncKeyValueFileSystem.prototype.commitNewFile = function (tx, p, type, mode, data, cb) {
            var _this = this;
            var parentDir = path.dirname(p), fname = path.basename(p), currTime = (new Date()).getTime();

            // Invariant: The root always exists.
            // If we don't check this prior to taking steps below, we will create a
            // file with name '' in root should p == '/'.
            if (p === '/') {
                return cb(ApiError.EEXIST(p));
            }

            // Let's build a pyramid of code!
            // Step 1: Get the parent directory's inode and directory listing
            this.findINodeAndDirListing(tx, parentDir, function (e, parentNode, dirListing) {
                if (noErrorTx(e, tx, cb)) {
                    if (dirListing[fname]) {
                        // File already exists.
                        tx.abort(function () {
                            cb(ApiError.EEXIST(p));
                        });
                    } else {
                        // Step 2: Commit data to store.
                        _this.addNewNode(tx, data, function (e, dataId) {
                            if (noErrorTx(e, tx, cb)) {
                                // Step 3: Commit the file's inode to the store.
                                var fileInode = new Inode(dataId, data.length, mode | type, currTime, currTime, currTime);
                                _this.addNewNode(tx, fileInode.toBuffer(), function (e, fileInodeId) {
                                    if (noErrorTx(e, tx, cb)) {
                                        // Step 4: Update parent directory's listing.
                                        dirListing[fname] = fileInodeId;
                                        tx.put(parentNode.id, new Buffer(JSON.stringify(dirListing)), true, function (e) {
                                            if (noErrorTx(e, tx, cb)) {
                                                // Step 5: Commit and return the new inode.
                                                tx.commit(function (e) {
                                                    if (noErrorTx(e, tx, cb)) {
                                                        cb(null, fileInode);
                                                    }
                                                });
                                            }
                                        });
                                    }
                                });
                            }
                        });
                    }
                }
            });
        };

        /**
        * Delete all contents stored in the file system.
        */
        AsyncKeyValueFileSystem.prototype.empty = function (cb) {
            var _this = this;
            this.store.clear(function (e) {
                if (noError(e, cb)) {
                    // INVARIANT: Root always exists.
                    _this.makeRootDirectory(cb);
                }
            });
        };

        AsyncKeyValueFileSystem.prototype.rename = function (oldPath, newPath, cb) {
            var _this = this;
            var tx = this.store.beginTransaction('readwrite'), oldParent = path.dirname(oldPath), oldName = path.basename(oldPath), newParent = path.dirname(newPath), newName = path.basename(newPath), inodes = {}, lists = {}, errorOccurred = false;

            // Invariant: Can't move a folder inside itself.
            // This funny little hack ensures that the check passes only if oldPath
            // is a subpath of newParent. We append '/' to avoid matching folders that
            // are a substring of the bottom-most folder in the path.
            if ((newParent + '/').indexOf(oldPath + '/') === 0) {
                return cb(new ApiError(5 /* EBUSY */, oldParent));
            }

            /**
            * Responsible for Phase 2 of the rename operation: Modifying and
            * committing the directory listings. Called once we have successfully
            * retrieved both the old and new parent's inodes and listings.
            */
            var theOleSwitcharoo = function () {
                // Sanity check: Ensure both paths are present, and no error has occurred.
                if (errorOccurred || !lists.hasOwnProperty(oldParent) || !lists.hasOwnProperty(newParent)) {
                    return;
                }
                var oldParentList = lists[oldParent], oldParentINode = inodes[oldParent], newParentList = lists[newParent], newParentINode = inodes[newParent];

                // Delete file from old parent.
                if (!oldParentList[oldName]) {
                    cb(ApiError.ENOENT(oldPath));
                } else {
                    var fileId = oldParentList[oldName];
                    delete oldParentList[oldName];

                    // Finishes off the renaming process by adding the file to the new
                    // parent.
                    var completeRename = function () {
                        newParentList[newName] = fileId;

                        // Commit old parent's list.
                        tx.put(oldParentINode.id, new Buffer(JSON.stringify(oldParentList)), true, function (e) {
                            if (noErrorTx(e, tx, cb)) {
                                if (oldParent === newParent) {
                                    // DONE!
                                    tx.commit(cb);
                                } else {
                                    // Commit new parent's list.
                                    tx.put(newParentINode.id, new Buffer(JSON.stringify(newParentList)), true, function (e) {
                                        if (noErrorTx(e, tx, cb)) {
                                            tx.commit(cb);
                                        }
                                    });
                                }
                            }
                        });
                    };

                    if (newParentList[newName]) {
                        // 'newPath' already exists. Check if it's a file or a directory, and
                        // act accordingly.
                        _this.getINode(tx, newPath, newParentList[newName], function (e, inode) {
                            if (noErrorTx(e, tx, cb)) {
                                if (inode.isFile()) {
                                    // Delete the file and continue.
                                    tx.delete(inode.id, function (e) {
                                        if (noErrorTx(e, tx, cb)) {
                                            tx.delete(newParentList[newName], function (e) {
                                                if (noErrorTx(e, tx, cb)) {
                                                    completeRename();
                                                }
                                            });
                                        }
                                    });
                                } else {
                                    // Can't overwrite a directory using rename.
                                    tx.abort(function (e) {
                                        cb(ApiError.EPERM(newPath));
                                    });
                                }
                            }
                        });
                    } else {
                        completeRename();
                    }
                }
            };

            /**
            * Grabs a path's inode and directory listing, and shoves it into the
            * inodes and lists hashes.
            */
            var processInodeAndListings = function (p) {
                _this.findINodeAndDirListing(tx, p, function (e, node, dirList) {
                    if (e) {
                        if (!errorOccurred) {
                            errorOccurred = true;
                            tx.abort(function () {
                                cb(e);
                            });
                        }
                        // If error has occurred already, just stop here.
                    } else {
                        inodes[p] = node;
                        lists[p] = dirList;
                        theOleSwitcharoo();
                    }
                });
            };

            processInodeAndListings(oldParent);
            if (oldParent !== newParent) {
                processInodeAndListings(newParent);
            }
        };

        AsyncKeyValueFileSystem.prototype.stat = function (p, isLstat, cb) {
            var tx = this.store.beginTransaction('readonly');
            this.findINode(tx, p, function (e, inode) {
                if (noError(e, cb)) {
                    cb(null, inode.toStats());
                }
            });
        };

        AsyncKeyValueFileSystem.prototype.createFile = function (p, flag, mode, cb) {
            var _this = this;
            var tx = this.store.beginTransaction('readwrite'), data = new Buffer(0);

            this.commitNewFile(tx, p, 32768 /* FILE */, mode, data, function (e, newFile) {
                if (noError(e, cb)) {
                    cb(null, new AsyncKeyValueFile(_this, p, flag, newFile.toStats(), data));
                }
            });
        };

        AsyncKeyValueFileSystem.prototype.openFile = function (p, flag, cb) {
            var _this = this;
            var tx = this.store.beginTransaction('readonly');

            // Step 1: Grab the file's inode.
            this.findINode(tx, p, function (e, inode) {
                if (noError(e, cb)) {
                    // Step 2: Grab the file's data.
                    tx.get(inode.id, function (e, data) {
                        if (noError(e, cb)) {
                            if (data === undefined) {
                                cb(ApiError.ENOENT(p));
                            } else {
                                cb(null, new AsyncKeyValueFile(_this, p, flag, inode.toStats(), data));
                            }
                        }
                    });
                }
            });
        };

        /**
        * Remove all traces of the given path from the file system.
        * @param p The path to remove from the file system.
        * @param isDir Does the path belong to a directory, or a file?
        * @todo Update mtime.
        */
        AsyncKeyValueFileSystem.prototype.removeEntry = function (p, isDir, cb) {
            var _this = this;
            var tx = this.store.beginTransaction('readwrite'), parent = path.dirname(p), fileName = path.basename(p);

            // Step 1: Get parent directory's node and directory listing.
            this.findINodeAndDirListing(tx, parent, function (e, parentNode, parentListing) {
                if (noErrorTx(e, tx, cb)) {
                    if (!parentListing[fileName]) {
                        tx.abort(function () {
                            cb(ApiError.ENOENT(p));
                        });
                    } else {
                        // Remove from directory listing of parent.
                        var fileNodeId = parentListing[fileName];
                        delete parentListing[fileName];

                        // Step 2: Get file inode.
                        _this.getINode(tx, p, fileNodeId, function (e, fileNode) {
                            if (noErrorTx(e, tx, cb)) {
                                if (!isDir && fileNode.isDirectory()) {
                                    tx.abort(function () {
                                        cb(ApiError.EISDIR(p));
                                    });
                                } else if (isDir && !fileNode.isDirectory()) {
                                    tx.abort(function () {
                                        cb(ApiError.ENOTDIR(p));
                                    });
                                } else {
                                    // Step 3: Delete data.
                                    tx.delete(fileNode.id, function (e) {
                                        if (noErrorTx(e, tx, cb)) {
                                            // Step 4: Delete node.
                                            tx.delete(fileNodeId, function (e) {
                                                if (noErrorTx(e, tx, cb)) {
                                                    // Step 5: Update directory listing.
                                                    tx.put(parentNode.id, new Buffer(JSON.stringify(parentListing)), true, function (e) {
                                                        if (noErrorTx(e, tx, cb)) {
                                                            tx.commit(cb);
                                                        }
                                                    });
                                                }
                                            });
                                        }
                                    });
                                }
                            }
                        });
                    }
                }
            });
        };

        AsyncKeyValueFileSystem.prototype.unlink = function (p, cb) {
            this.removeEntry(p, false, cb);
        };

        AsyncKeyValueFileSystem.prototype.rmdir = function (p, cb) {
            this.removeEntry(p, true, cb);
        };

        AsyncKeyValueFileSystem.prototype.mkdir = function (p, mode, cb) {
            var tx = this.store.beginTransaction('readwrite'), data = new Buffer('{}');
            this.commitNewFile(tx, p, 16384 /* DIRECTORY */, mode, data, cb);
        };

        AsyncKeyValueFileSystem.prototype.readdir = function (p, cb) {
            var _this = this;
            var tx = this.store.beginTransaction('readonly');
            this.findINode(tx, p, function (e, inode) {
                if (noError(e, cb)) {
                    _this.getDirListing(tx, p, inode, function (e, dirListing) {
                        if (noError(e, cb)) {
                            cb(null, Object.keys(dirListing));
                        }
                    });
                }
            });
        };

        AsyncKeyValueFileSystem.prototype._sync = function (p, data, stats, cb) {
            var _this = this;
            // @todo Ensure mtime updates properly, and use that to determine if a data
            //       update is required.
            var tx = this.store.beginTransaction('readwrite');

            // Step 1: Get the file node's ID.
            this._findINode(tx, path.dirname(p), path.basename(p), function (e, fileInodeId) {
                if (noErrorTx(e, tx, cb)) {
                    // Step 2: Get the file inode.
                    _this.getINode(tx, p, fileInodeId, function (e, fileInode) {
                        if (noErrorTx(e, tx, cb)) {
                            var inodeChanged = fileInode.update(stats);

                            // Step 3: Sync the data.
                            tx.put(fileInode.id, data, true, function (e) {
                                if (noErrorTx(e, tx, cb)) {
                                    // Step 4: Sync the metadata (if it changed)!
                                    if (inodeChanged) {
                                        tx.put(fileInodeId, fileInode.toBuffer(), true, function (e) {
                                            if (noErrorTx(e, tx, cb)) {
                                                tx.commit(cb);
                                            }
                                        });
                                    } else {
                                        // No need to sync metadata; return.
                                        tx.commit(cb);
                                    }
                                }
                            });
                        }
                    });
                }
            });
        };
        return AsyncKeyValueFileSystem;
    })(file_system.BaseFileSystem);
    exports.AsyncKeyValueFileSystem = AsyncKeyValueFileSystem;
});
//# sourceMappingURL=key_value_filesystem.js.map
;
define('core/global',["require", "exports"], function(require, exports) {
    /**
    * Exports the global scope variable.
    * In the main browser thread, this is "window".
    * In a WebWorker, this is "self".
    * In Node, this is "global".
    */
    var toExport;
    if (typeof (window) !== 'undefined') {
        toExport = window;
    } else if (typeof (self) !== 'undefined') {
        toExport = self;
    } else {
        toExport = global;
    }
    
    return toExport;
});
//# sourceMappingURL=global.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/IndexedDB',["require", "exports", '../core/buffer', '../core/browserfs', '../generic/key_value_filesystem', '../core/api_error', '../core/buffer_core_arraybuffer', '../core/global'], function(require, exports, buffer, browserfs, kvfs, api_error, buffer_core_arraybuffer, global) {
    var Buffer = buffer.Buffer, ApiError = api_error.ApiError, ErrorCode = api_error.ErrorCode, indexedDB = global.indexedDB || global.mozIndexedDB || global.webkitIndexedDB || global.msIndexedDB;

    /**
    * Converts a DOMException or a DOMError from an IndexedDB event into a
    * standardized BrowserFS API error.
    */
    function convertError(e, message) {
        if (typeof message === "undefined") { message = e.toString(); }
        switch (e.name) {
            case "NotFoundError":
                return new ApiError(1 /* ENOENT */, message);
            case "QuotaExceededError":
                return new ApiError(11 /* ENOSPC */, message);
            default:
                // The rest do not seem to map cleanly to standard error codes.
                return new ApiError(2 /* EIO */, message);
        }
    }

    /**
    * Produces a new onerror handler for IDB. Our errors are always fatal, so we
    * handle them generically: Call the user-supplied callback with a translated
    * version of the error, and let the error bubble up.
    */
    function onErrorHandler(cb, code, message) {
        if (typeof code === "undefined") { code = 2 /* EIO */; }
        if (typeof message === "undefined") { message = null; }
        return function (e) {
            // Prevent the error from canceling the transaction.
            e.preventDefault();
            cb(new ApiError(code, message));
        };
    }

    /**
    * Converts a NodeBuffer into an ArrayBuffer.
    */
    function buffer2arraybuffer(buffer) {
        // XXX: Typing hack.
        var backing_mem = buffer.getBufferCore();
        if (!(backing_mem instanceof buffer_core_arraybuffer.BufferCoreArrayBuffer)) {
            // Copy into an ArrayBuffer-backed Buffer.
            buffer = new Buffer(this._buffer.length);
            this._buffer.copy(buffer);
            backing_mem = buffer.getBufferCore();
        }

        // Reach into the BC, grab the DV.
        var dv = backing_mem.getDataView();
        return dv.buffer;
    }

    var IndexedDBROTransaction = (function () {
        function IndexedDBROTransaction(tx, store) {
            this.tx = tx;
            this.store = store;
        }
        IndexedDBROTransaction.prototype.get = function (key, cb) {
            try  {
                var r = this.store.get(key);
                r.onerror = onErrorHandler(cb);
                r.onsuccess = function (event) {
                    // IDB returns the value 'undefined' when you try to get keys that
                    // don't exist. The caller expects this behavior.
                    var result = event.target.result;
                    if (result === undefined) {
                        cb(null, result);
                    } else {
                        // IDB data is stored as an ArrayBuffer
                        cb(null, new Buffer(result));
                    }
                };
            } catch (e) {
                cb(convertError(e));
            }
        };
        return IndexedDBROTransaction;
    })();
    exports.IndexedDBROTransaction = IndexedDBROTransaction;

    var IndexedDBRWTransaction = (function (_super) {
        __extends(IndexedDBRWTransaction, _super);
        function IndexedDBRWTransaction(tx, store) {
            _super.call(this, tx, store);
        }
        IndexedDBRWTransaction.prototype.put = function (key, data, overwrite, cb) {
            try  {
                var arraybuffer = buffer2arraybuffer(data), r;
                if (overwrite) {
                    r = this.store.put(arraybuffer, key);
                } else {
                    // 'add' will never overwrite an existing key.
                    r = this.store.add(arraybuffer, key);
                }

                // XXX: NEED TO RETURN FALSE WHEN ADD HAS A KEY CONFLICT. NO ERROR.
                r.onerror = onErrorHandler(cb);
                r.onsuccess = function (event) {
                    cb(null, true);
                };
            } catch (e) {
                cb(convertError(e));
            }
        };

        IndexedDBRWTransaction.prototype.delete = function (key, cb) {
            try  {
                var r = this.store.delete(key);
                r.onerror = onErrorHandler(cb);
                r.onsuccess = function (event) {
                    cb();
                };
            } catch (e) {
                cb(convertError(e));
            }
        };

        IndexedDBRWTransaction.prototype.commit = function (cb) {
            // Return to the event loop to commit the transaction.
            setTimeout(cb, 0);
        };

        IndexedDBRWTransaction.prototype.abort = function (cb) {
            var _e;
            try  {
                this.tx.abort();
            } catch (e) {
                _e = convertError(e);
            } finally {
                cb(_e);
            }
        };
        return IndexedDBRWTransaction;
    })(IndexedDBROTransaction);
    exports.IndexedDBRWTransaction = IndexedDBRWTransaction;

    var IndexedDBStore = (function () {
        /**
        * Constructs an IndexedDB file system.
        * @param cb Called once the database is instantiated and ready for use.
        *   Passes an error if there was an issue instantiating the database.
        * @param objectStoreName The name of this file system. You can have
        *   multiple IndexedDB file systems operating at once, but each must have
        *   a different name.
        */
        function IndexedDBStore(cb, storeName) {
            if (typeof storeName === "undefined") { storeName = 'browserfs'; }
            var _this = this;
            this.storeName = storeName;
            var openReq = indexedDB.open(this.storeName, 1);

            openReq.onupgradeneeded = function (event) {
                var db = event.target.result;

                // Huh. This should never happen; we're at version 1. Why does another
                // database exist?
                if (db.objectStoreNames.contains(_this.storeName)) {
                    db.deleteObjectStore(_this.storeName);
                }
                db.createObjectStore(_this.storeName);
            };

            openReq.onsuccess = function (event) {
                _this.db = event.target.result;
                cb(null, _this);
            };

            openReq.onerror = onErrorHandler(cb, 4 /* EACCES */);
        }
        IndexedDBStore.prototype.name = function () {
            return "IndexedDB - " + this.storeName;
        };

        IndexedDBStore.prototype.clear = function (cb) {
            try  {
                var tx = this.db.transaction(this.storeName, 'readwrite'), objectStore = tx.objectStore(this.storeName), r = objectStore.clear();
                r.onsuccess = function (event) {
                    // Use setTimeout to commit transaction.
                    setTimeout(cb, 0);
                };
                r.onerror = onErrorHandler(cb);
            } catch (e) {
                cb(convertError(e));
            }
        };

        IndexedDBStore.prototype.beginTransaction = function (type) {
            if (typeof type === "undefined") { type = 'readonly'; }
            var tx = this.db.transaction(this.storeName, type), objectStore = tx.objectStore(this.storeName);
            if (type === 'readwrite') {
                return new IndexedDBRWTransaction(tx, objectStore);
            } else if (type === 'readonly') {
                return new IndexedDBROTransaction(tx, objectStore);
            } else {
                throw new ApiError(9 /* EINVAL */, 'Invalid transaction type.');
            }
        };
        return IndexedDBStore;
    })();
    exports.IndexedDBStore = IndexedDBStore;

    /**
    * A file system that uses the IndexedDB key value file system.
    */
    var IndexedDBFileSystem = (function (_super) {
        __extends(IndexedDBFileSystem, _super);
        function IndexedDBFileSystem(cb, storeName) {
            var _this = this;
            _super.call(this);
            new IndexedDBStore(function (e, store) {
                if (e) {
                    cb(e);
                } else {
                    _this.init(store, function (e) {
                        cb(e, _this);
                    });
                }
            }, storeName);
        }
        IndexedDBFileSystem.isAvailable = function () {
            return typeof indexedDB !== 'undefined';
        };
        return IndexedDBFileSystem;
    })(kvfs.AsyncKeyValueFileSystem);
    exports.IndexedDBFileSystem = IndexedDBFileSystem;

    browserfs.registerFileSystem('IndexedDB', IndexedDBFileSystem);
});
//# sourceMappingURL=IndexedDB.js.map
;
define('generic/file_index',["require", "exports", '../core/node_fs_stats', '../core/node_path'], function(require, exports, node_fs_stats, node_path) {
    var Stats = node_fs_stats.Stats;
    var path = node_path.path;

    /**
    * A simple class for storing a filesystem index. Assumes that all paths passed
    * to it are *absolute* paths.
    *
    * Can be used as a partial or a full index, although care must be taken if used
    * for the former purpose, especially when directories are concerned.
    */
    var FileIndex = (function () {
        /**
        * Constructs a new FileIndex.
        */
        function FileIndex() {
            // _index is a single-level key,value store that maps *directory* paths to
            // DirInodes. File information is only contained in DirInodes themselves.
            this._index = {};

            // Create the root directory.
            this.addPath('/', new DirInode());
        }
        /**
        * Split into a (directory path, item name) pair
        */
        FileIndex.prototype._split_path = function (p) {
            var dirpath = path.dirname(p);
            var itemname = p.substr(dirpath.length + (dirpath === "/" ? 0 : 1));
            return [dirpath, itemname];
        };

        /**
        * Runs the given function over all files in the index.
        */
        FileIndex.prototype.fileIterator = function (cb) {
            for (var path in this._index) {
                var dir = this._index[path];
                var files = dir.getListing();
                for (var i = 0; i < files.length; i++) {
                    var item = dir.getItem(files[i]);
                    if (item.isFile()) {
                        cb(item.getData());
                    }
                }
            }
        };

        /**
        * Adds the given absolute path to the index if it is not already in the index.
        * Creates any needed parent directories.
        * @param [String] path The path to add to the index.
        * @param [BrowserFS.FileInode | BrowserFS.DirInode] inode The inode for the
        *   path to add.
        * @return [Boolean] 'True' if it was added or already exists, 'false' if there
        *   was an issue adding it (e.g. item in path is a file, item exists but is
        *   different).
        * @todo If adding fails and implicitly creates directories, we do not clean up
        *   the new empty directories.
        */
        FileIndex.prototype.addPath = function (path, inode) {
            if (inode == null) {
                throw new Error('Inode must be specified');
            }
            if (path[0] !== '/') {
                throw new Error('Path must be absolute, got: ' + path);
            }

            // Check if it already exists.
            if (this._index.hasOwnProperty(path)) {
                return this._index[path] === inode;
            }

            var splitPath = this._split_path(path);
            var dirpath = splitPath[0];
            var itemname = splitPath[1];

            // Try to add to its parent directory first.
            var parent = this._index[dirpath];
            if (parent === undefined && path !== '/') {
                // Create parent.
                parent = new DirInode();
                if (!this.addPath(dirpath, parent)) {
                    return false;
                }
            }

            // Add myself to my parent.
            if (path !== '/') {
                if (!parent.addItem(itemname, inode)) {
                    return false;
                }
            }

            // If I'm a directory, add myself to the index.
            if (!inode.isFile()) {
                this._index[path] = inode;
            }
            return true;
        };

        /**
        * Removes the given path. Can be a file or a directory.
        * @return [BrowserFS.FileInode | BrowserFS.DirInode | null] The removed item,
        *   or null if it did not exist.
        */
        FileIndex.prototype.removePath = function (path) {
            var splitPath = this._split_path(path);
            var dirpath = splitPath[0];
            var itemname = splitPath[1];

            // Try to remove it from its parent directory first.
            var parent = this._index[dirpath];
            if (parent === undefined) {
                return null;
            }

            // Remove myself from my parent.
            var inode = parent.remItem(itemname);
            if (inode === null) {
                return null;
            }

            // If I'm a directory, remove myself from the index, and remove my children.
            if (!inode.isFile()) {
                var dirInode = inode;
                var children = dirInode.getListing();
                for (var i = 0; i < children.length; i++) {
                    this.removePath(path + '/' + children[i]);
                }

                // Remove the directory from the index, unless it's the root.
                if (path !== '/') {
                    delete this._index[path];
                }
            }
            return inode;
        };

        /**
        * Retrieves the directory listing of the given path.
        * @return [String[]] An array of files in the given path, or 'null' if it does
        *   not exist.
        */
        FileIndex.prototype.ls = function (path) {
            var item = this._index[path];
            if (item === undefined) {
                return null;
            }
            return item.getListing();
        };

        /**
        * Returns the inode of the given item.
        * @param [String] path
        * @return [BrowserFS.FileInode | BrowserFS.DirInode | null] Returns null if
        *   the item does not exist.
        */
        FileIndex.prototype.getInode = function (path) {
            var splitPath = this._split_path(path);
            var dirpath = splitPath[0];
            var itemname = splitPath[1];

            // Retrieve from its parent directory.
            var parent = this._index[dirpath];
            if (parent === undefined) {
                return null;
            }

            // Root case
            if (dirpath === path) {
                return parent;
            }
            return parent.getItem(itemname);
        };

        /**
        * Static method for constructing indices from a JSON listing.
        * @param [Object] listing Directory listing generated by tools/XHRIndexer.coffee
        * @return [BrowserFS.FileIndex] A new FileIndex object.
        */
        FileIndex.from_listing = function (listing) {
            var idx = new FileIndex();

            // Add a root DirNode.
            var rootInode = new DirInode();
            idx._index['/'] = rootInode;
            var queue = [['', listing, rootInode]];
            while (queue.length > 0) {
                var inode;
                var next = queue.pop();
                var pwd = next[0];
                var tree = next[1];
                var parent = next[2];
                for (var node in tree) {
                    var children = tree[node];
                    var name = "" + pwd + "/" + node;
                    if (children != null) {
                        idx._index[name] = inode = new DirInode();
                        queue.push([name, children, inode]);
                    } else {
                        // This inode doesn't have correct size information, noted with -1.
                        inode = new FileInode(new Stats(32768 /* FILE */, -1, 0x16D));
                    }
                    if (parent != null) {
                        parent._ls[node] = inode;
                    }
                }
            }
            return idx;
        };
        return FileIndex;
    })();
    exports.FileIndex = FileIndex;

    

    /**
    * Inode for a file. Stores an arbitrary (filesystem-specific) data payload.
    */
    var FileInode = (function () {
        function FileInode(data) {
            this.data = data;
        }
        FileInode.prototype.isFile = function () {
            return true;
        };
        FileInode.prototype.isDir = function () {
            return false;
        };
        FileInode.prototype.getData = function () {
            return this.data;
        };
        FileInode.prototype.setData = function (data) {
            this.data = data;
        };
        return FileInode;
    })();
    exports.FileInode = FileInode;

    /**
    * Inode for a directory. Currently only contains the directory listing.
    */
    var DirInode = (function () {
        /**
        * Constructs an inode for a directory.
        */
        function DirInode() {
            this._ls = {};
        }
        DirInode.prototype.isFile = function () {
            return false;
        };
        DirInode.prototype.isDir = function () {
            return true;
        };

        /**
        * Return a Stats object for this inode.
        * @todo Should probably remove this at some point. This isn't the
        *       responsibility of the FileIndex.
        * @return [BrowserFS.node.fs.Stats]
        */
        DirInode.prototype.getStats = function () {
            return new Stats(16384 /* DIRECTORY */, 4096, 0x16D);
        };

        /**
        * Returns the directory listing for this directory. Paths in the directory are
        * relative to the directory's path.
        * @return [String[]] The directory listing for this directory.
        */
        DirInode.prototype.getListing = function () {
            return Object.keys(this._ls);
        };

        /**
        * Returns the inode for the indicated item, or null if it does not exist.
        * @param [String] p Name of item in this directory.
        * @return [BrowserFS.FileInode | BrowserFS.DirInode | null]
        */
        DirInode.prototype.getItem = function (p) {
            var _ref;
            return (_ref = this._ls[p]) != null ? _ref : null;
        };

        /**
        * Add the given item to the directory listing. Note that the given inode is
        * not copied, and will be mutated by the DirInode if it is a DirInode.
        * @param [String] p Item name to add to the directory listing.
        * @param [BrowserFS.FileInode | BrowserFS.DirInode] inode The inode for the
        *   item to add to the directory inode.
        * @return [Boolean] True if it was added, false if it already existed.
        */
        DirInode.prototype.addItem = function (p, inode) {
            if (p in this._ls) {
                return false;
            }
            this._ls[p] = inode;
            return true;
        };

        /**
        * Removes the given item from the directory listing.
        * @param [String] p Name of item to remove from the directory listing.
        * @return [BrowserFS.FileInode | BrowserFS.DirInode | null] Returns the item
        *   removed, or null if the item did not exist.
        */
        DirInode.prototype.remItem = function (p) {
            var item = this._ls[p];
            if (item === undefined) {
                return null;
            }
            delete this._ls[p];
            return item;
        };
        return DirInode;
    })();
    exports.DirInode = DirInode;
});
//# sourceMappingURL=file_index.js.map
;
/**
* Grab bag of utility functions used across the code.
*/
define('core/util',["require", "exports"], function(require, exports) {
    /**
    * Estimates the size of a JS object.
    * @param {Object} object - the object to measure.
    * @return {Number} estimated object size.
    * @see http://stackoverflow.com/a/11900218/10601
    */
    function roughSizeOfObject(object) {
        var bytes, key, objectList, prop, stack, value;
        objectList = [];
        stack = [object];
        bytes = 0;
        while (stack.length !== 0) {
            value = stack.pop();
            if (typeof value === 'boolean') {
                bytes += 4;
            } else if (typeof value === 'string') {
                bytes += value.length * 2;
            } else if (typeof value === 'number') {
                bytes += 8;
            } else if (typeof value === 'object' && objectList.indexOf(value) < 0) {
                objectList.push(value);
                bytes += 4;
                for (key in value) {
                    prop = value[key];
                    bytes += key.length * 2;
                    stack.push(prop);
                }
            }
        }
        return bytes;
    }
    exports.roughSizeOfObject = roughSizeOfObject;

    /**
    * Checks for any IE version, including IE11 which removed MSIE from the
    * userAgent string.
    */
    exports.isIE = (/(msie) ([\w.]+)/.exec(navigator.userAgent.toLowerCase()) != null || navigator.userAgent.indexOf('Trident') !== -1);
});
//# sourceMappingURL=util.js.map
;
/**
* Contains utility methods for performing a variety of tasks with
* XmlHttpRequest across browsers.
*/
define('generic/xhr',["require", "exports", '../core/util', '../core/buffer', '../core/api_error'], function(require, exports, util, buffer, api_error) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var Buffer = buffer.Buffer;

    

    // Converts 'responseBody' in IE into the equivalent 'responseText' that other
    // browsers would generate.
    function getIEByteArray(IEByteArray) {
        var rawBytes = IEBinaryToArray_ByteStr(IEByteArray);
        var lastChr = IEBinaryToArray_ByteStr_Last(IEByteArray);
        var data_str = rawBytes.replace(/[\s\S]/g, function (match) {
            var v = match.charCodeAt(0);
            return String.fromCharCode(v & 0xff, v >> 8);
        }) + lastChr;
        var data_array = new Array(data_str.length);
        for (var i = 0; i < data_str.length; i++) {
            data_array[i] = data_str.charCodeAt(i);
        }
        return data_array;
    }

    function downloadFileIE(async, p, type, cb) {
        switch (type) {
            case 'buffer':

            case 'json':
                break;
            default:
                return cb(new ApiError(9 /* EINVAL */, "Invalid download type: " + type));
        }

        var req = new XMLHttpRequest();
        req.open('GET', p, async);
        req.setRequestHeader("Accept-Charset", "x-user-defined");
        req.onreadystatechange = function (e) {
            var data_array;
            if (req.readyState === 4) {
                if (req.status === 200) {
                    switch (type) {
                        case 'buffer':
                            data_array = getIEByteArray(req.responseBody);
                            return cb(null, new Buffer(data_array));
                        case 'json':
                            return cb(null, JSON.parse(req.responseText));
                    }
                } else {
                    return cb(new ApiError(req.status, "XHR error."));
                }
            }
        };
        req.send();
    }

    function asyncDownloadFileIE(p, type, cb) {
        downloadFileIE(true, p, type, cb);
    }

    function syncDownloadFileIE(p, type) {
        var rv;
        downloadFileIE(false, p, type, function (err, data) {
            if (err)
                throw err;
            rv = data;
        });
        return rv;
    }

    function asyncDownloadFileModern(p, type, cb) {
        var req = new XMLHttpRequest();
        req.open('GET', p, true);
        var jsonSupported = true;
        switch (type) {
            case 'buffer':
                req.responseType = 'arraybuffer';
                break;
            case 'json':
                try  {
                    req.responseType = 'json';
                    jsonSupported = req.responseType === 'json';
                } catch (e) {
                    jsonSupported = false;
                }
                break;
            default:
                return cb(new ApiError(9 /* EINVAL */, "Invalid download type: " + type));
        }
        req.onreadystatechange = function (e) {
            if (req.readyState === 4) {
                if (req.status === 200) {
                    switch (type) {
                        case 'buffer':
                            // XXX: WebKit-based browsers return *null* when XHRing an empty file.
                            return cb(null, new Buffer(req.response ? req.response : 0));
                        case 'json':
                            if (jsonSupported) {
                                return cb(null, req.response);
                            } else {
                                return cb(null, JSON.parse(req.responseText));
                            }
                    }
                } else {
                    return cb(new ApiError(req.status, "XHR error."));
                }
            }
        };
        req.send();
    }

    function syncDownloadFileModern(p, type) {
        var req = new XMLHttpRequest();
        req.open('GET', p, false);

        // On most platforms, we cannot set the responseType of synchronous downloads.
        // @todo Test for this; IE10 allows this, as do older versions of Chrome/FF.
        var data = null;
        var err = null;

        // Classic hack to download binary data as a string.
        req.overrideMimeType('text/plain; charset=x-user-defined');
        req.onreadystatechange = function (e) {
            if (req.readyState === 4) {
                if (req.status === 200) {
                    switch (type) {
                        case 'buffer':
                            // Convert the text into a buffer.
                            var text = req.responseText;
                            data = new Buffer(text.length);

                            for (var i = 0; i < text.length; i++) {
                                // This will automatically throw away the upper bit of each
                                // character for us.
                                data.writeUInt8(text.charCodeAt(i), i);
                            }
                            return;
                        case 'json':
                            data = JSON.parse(req.responseText);
                            return;
                    }
                } else {
                    err = new ApiError(req.status, "XHR error.");
                    return;
                }
            }
        };
        req.send();
        if (err) {
            throw err;
        }
        return data;
    }

    

    function syncDownloadFileIE10(p, type) {
        var req = new XMLHttpRequest();
        req.open('GET', p, false);
        switch (type) {
            case 'buffer':
                req.responseType = 'arraybuffer';
                break;
            case 'json':
                break;
            default:
                throw new ApiError(9 /* EINVAL */, "Invalid download type: " + type);
        }
        var data;
        var err;
        req.onreadystatechange = function (e) {
            if (req.readyState === 4) {
                if (req.status === 200) {
                    switch (type) {
                        case 'buffer':
                            data = new Buffer(req.response);
                            break;
                        case 'json':
                            data = JSON.parse(req.response);
                            break;
                    }
                } else {
                    err = new ApiError(req.status, "XHR error.");
                }
            }
        };
        req.send();
        if (err) {
            throw err;
        }
        return data;
    }

    function getFileSize(async, p, cb) {
        var req = new XMLHttpRequest();
        req.open('HEAD', p, async);
        req.onreadystatechange = function (e) {
            if (req.readyState === 4) {
                if (req.status == 200) {
                    try  {
                        return cb(null, parseInt(req.getResponseHeader('Content-Length'), 10));
                    } catch (e) {
                        // In the event that the header isn't present or there is an error...
                        return cb(new ApiError(2 /* EIO */, "XHR HEAD error: Could not read content-length."));
                    }
                } else {
                    return cb(new ApiError(req.status, "XHR HEAD error."));
                }
            }
        };
        req.send();
    }

    /**
    * Asynchronously download a file as a buffer or a JSON object.
    * Note that the third function signature with a non-specialized type is
    * invalid, but TypeScript requires it when you specialize string arguments to
    * constants.
    */
    exports.asyncDownloadFile = (util.isIE && typeof Blob === 'undefined') ? asyncDownloadFileIE : asyncDownloadFileModern;

    /**
    * Synchronously download a file as a buffer or a JSON object.
    * Note that the third function signature with a non-specialized type is
    * invalid, but TypeScript requires it when you specialize string arguments to
    * constants.
    */
    exports.syncDownloadFile = (util.isIE && typeof Blob === 'undefined') ? syncDownloadFileIE : (util.isIE && typeof Blob !== 'undefined') ? syncDownloadFileIE10 : syncDownloadFileModern;

    /**
    * Synchronously retrieves the size of the given file in bytes.
    */
    function getFileSizeSync(p) {
        var rv;
        getFileSize(false, p, function (err, size) {
            if (err) {
                throw err;
            }
            rv = size;
        });
        return rv;
    }
    exports.getFileSizeSync = getFileSizeSync;

    /**
    * Asynchronously retrieves the size of the given file in bytes.
    */
    function getFileSizeAsync(p, cb) {
        getFileSize(true, p, cb);
    }
    exports.getFileSizeAsync = getFileSizeAsync;
});
//# sourceMappingURL=xhr.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/XmlHttpRequest',["require", "exports", '../core/file_system', '../generic/file_index', '../core/buffer', '../core/api_error', '../core/file_flag', '../generic/preload_file', '../core/browserfs', '../generic/xhr'], function(require, exports, file_system, file_index, buffer, api_error, file_flag, preload_file, browserfs, xhr) {
    var Buffer = buffer.Buffer;
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var FileFlag = file_flag.FileFlag;
    var ActionType = file_flag.ActionType;

    /**
    * A simple filesystem backed by XmlHttpRequests.
    */
    var XmlHttpRequest = (function (_super) {
        __extends(XmlHttpRequest, _super);
        /**
        * Constructs the file system.
        * @param [String] listing_url The path to the JSON file index generated by
        *   tools/XHRIndexer.coffee. This can be relative to the current webpage URL
        *   or absolutely specified.
        * @param [String] prefix_url The url prefix to use for all web-server requests.
        */
        function XmlHttpRequest(listing_url, prefix_url) {
            if (typeof prefix_url === "undefined") { prefix_url = ''; }
            _super.call(this);
            this.prefix_url = prefix_url;
            if (listing_url == null) {
                listing_url = 'index.json';
            }

            // prefix_url must end in a directory separator.
            if (prefix_url.length > 0 && prefix_url.charAt(prefix_url.length - 1) !== '/') {
                prefix_url = prefix_url + '/';
            }
            var listing = this._requestFileSync(listing_url, 'json');
            if (listing == null) {
                throw new Error("Unable to find listing at URL: " + listing_url);
            }
            this._index = file_index.FileIndex.from_listing(listing);
        }
        XmlHttpRequest.prototype.empty = function () {
            this._index.fileIterator(function (file) {
                file.file_data = null;
            });
        };

        XmlHttpRequest.prototype.getXhrPath = function (filePath) {
            if (filePath.charAt(0) === '/') {
                filePath = filePath.slice(1);
            }
            return this.prefix_url + filePath;
        };

        /**
        * Only requests the HEAD content, for the file size.
        */
        XmlHttpRequest.prototype._requestFileSizeAsync = function (path, cb) {
            xhr.getFileSizeAsync(this.getXhrPath(path), cb);
        };
        XmlHttpRequest.prototype._requestFileSizeSync = function (path) {
            return xhr.getFileSizeSync(this.getXhrPath(path));
        };

        XmlHttpRequest.prototype._requestFileAsync = function (p, type, cb) {
            xhr.asyncDownloadFile(this.getXhrPath(p), type, cb);
        };

        XmlHttpRequest.prototype._requestFileSync = function (p, type) {
            return xhr.syncDownloadFile(this.getXhrPath(p), type);
        };

        XmlHttpRequest.prototype.getName = function () {
            return 'XmlHttpRequest';
        };

        XmlHttpRequest.isAvailable = function () {
            // @todo Older browsers use a different name for XHR, iirc.
            return typeof XMLHttpRequest !== "undefined" && XMLHttpRequest !== null;
        };

        XmlHttpRequest.prototype.diskSpace = function (path, cb) {
            // Read-only file system. We could calculate the total space, but that's not
            // important right now.
            cb(0, 0);
        };

        XmlHttpRequest.prototype.isReadOnly = function () {
            return true;
        };

        XmlHttpRequest.prototype.supportsLinks = function () {
            return false;
        };

        XmlHttpRequest.prototype.supportsProps = function () {
            return false;
        };

        XmlHttpRequest.prototype.supportsSynch = function () {
            return true;
        };

        /**
        * Special XHR function: Preload the given file into the index.
        * @param [String] path
        * @param [BrowserFS.Buffer] buffer
        */
        XmlHttpRequest.prototype.preloadFile = function (path, buffer) {
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw ApiError.ENOENT(path);
            }
            var stats = inode.getData();
            stats.size = buffer.length;
            stats.file_data = buffer;
        };

        XmlHttpRequest.prototype.stat = function (path, isLstat, cb) {
            var inode = this._index.getInode(path);
            if (inode === null) {
                return cb(ApiError.ENOENT(path));
            }
            var stats;
            if (inode.isFile()) {
                stats = inode.getData();

                // At this point, a non-opened file will still have default stats from the listing.
                if (stats.size < 0) {
                    this._requestFileSizeAsync(path, function (e, size) {
                        if (e) {
                            return cb(e);
                        }
                        stats.size = size;
                        cb(null, stats.clone());
                    });
                } else {
                    cb(null, stats.clone());
                }
            } else {
                stats = inode.getStats();
                cb(null, stats);
            }
        };

        XmlHttpRequest.prototype.statSync = function (path, isLstat) {
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw ApiError.ENOENT(path);
            }
            var stats;
            if (inode.isFile()) {
                stats = inode.getData();

                // At this point, a non-opened file will still have default stats from the listing.
                if (stats.size < 0) {
                    stats.size = this._requestFileSizeSync(path);
                }
            } else {
                stats = inode.getStats();
            }
            return stats;
        };

        XmlHttpRequest.prototype.open = function (path, flags, mode, cb) {
            // INVARIANT: You can't write to files on this file system.
            if (flags.isWriteable()) {
                return cb(new ApiError(0 /* EPERM */, path));
            }
            var _this = this;

            // Check if the path exists, and is a file.
            var inode = this._index.getInode(path);
            if (inode === null) {
                return cb(ApiError.ENOENT(path));
            }
            if (inode.isDir()) {
                return cb(ApiError.EISDIR(path));
            }
            var stats = inode.getData();
            switch (flags.pathExistsAction()) {
                case 1 /* THROW_EXCEPTION */:
                case 2 /* TRUNCATE_FILE */:
                    return cb(ApiError.EEXIST(path));
                case 0 /* NOP */:
                    // Use existing file contents.
                    // XXX: Uh, this maintains the previously-used flag.
                    if (stats.file_data != null) {
                        return cb(null, new preload_file.NoSyncFile(_this, path, flags, stats.clone(), stats.file_data));
                    }

                    // @todo be lazier about actually requesting the file
                    this._requestFileAsync(path, 'buffer', function (err, buffer) {
                        if (err) {
                            return cb(err);
                        }

                        // we don't initially have file sizes
                        stats.size = buffer.length;
                        stats.file_data = buffer;
                        return cb(null, new preload_file.NoSyncFile(_this, path, flags, stats.clone(), buffer));
                    });
                    break;
                default:
                    return cb(new ApiError(9 /* EINVAL */, 'Invalid FileMode object.'));
            }
        };

        XmlHttpRequest.prototype.openSync = function (path, flags, mode) {
            // INVARIANT: You can't write to files on this file system.
            if (flags.isWriteable()) {
                throw new ApiError(0 /* EPERM */, path);
            }

            // Check if the path exists, and is a file.
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw ApiError.ENOENT(path);
            }
            if (inode.isDir()) {
                throw ApiError.EISDIR(path);
            }
            var stats = inode.getData();
            switch (flags.pathExistsAction()) {
                case 1 /* THROW_EXCEPTION */:
                case 2 /* TRUNCATE_FILE */:
                    throw ApiError.EEXIST(path);
                case 0 /* NOP */:
                    // Use existing file contents.
                    // XXX: Uh, this maintains the previously-used flag.
                    if (stats.file_data != null) {
                        return new preload_file.NoSyncFile(this, path, flags, stats.clone(), stats.file_data);
                    }

                    // @todo be lazier about actually requesting the file
                    var buffer = this._requestFileSync(path, 'buffer');

                    // we don't initially have file sizes
                    stats.size = buffer.length;
                    stats.file_data = buffer;
                    return new preload_file.NoSyncFile(this, path, flags, stats.clone(), buffer);
                default:
                    throw new ApiError(9 /* EINVAL */, 'Invalid FileMode object.');
            }
        };

        XmlHttpRequest.prototype.readdir = function (path, cb) {
            try  {
                cb(null, this.readdirSync(path));
            } catch (e) {
                cb(e);
            }
        };

        XmlHttpRequest.prototype.readdirSync = function (path) {
            // Check if it exists.
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw ApiError.ENOENT(path);
            } else if (inode.isFile()) {
                throw ApiError.ENOTDIR(path);
            }
            return inode.getListing();
        };

        /**
        * We have the entire file as a buffer; optimize readFile.
        */
        XmlHttpRequest.prototype.readFile = function (fname, encoding, flag, cb) {
            // Wrap cb in file closing code.
            var oldCb = cb;

            // Get file.
            this.open(fname, flag, 0x1a4, function (err, fd) {
                if (err) {
                    return cb(err);
                }
                cb = function (err, arg) {
                    fd.close(function (err2) {
                        if (err == null) {
                            err = err2;
                        }
                        return oldCb(err, arg);
                    });
                };
                var fdCast = fd;
                var fdBuff = fdCast._buffer;
                if (encoding === null) {
                    if (fdBuff.length > 0) {
                        return cb(err, fdBuff.sliceCopy());
                    } else {
                        return cb(err, new buffer.Buffer(0));
                    }
                }
                try  {
                    cb(null, fdBuff.toString(encoding));
                } catch (e) {
                    cb(e);
                }
            });
        };

        /**
        * Specially-optimized readfile.
        */
        XmlHttpRequest.prototype.readFileSync = function (fname, encoding, flag) {
            // Get file.
            var fd = this.openSync(fname, flag, 0x1a4);
            try  {
                var fdCast = fd;
                var fdBuff = fdCast._buffer;
                if (encoding === null) {
                    if (fdBuff.length > 0) {
                        return fdBuff.sliceCopy();
                    } else {
                        return new buffer.Buffer(0);
                    }
                }
                return fdBuff.toString(encoding);
            } finally {
                fd.closeSync();
            }
        };
        return XmlHttpRequest;
    })(file_system.BaseFileSystem);
    exports.XmlHttpRequest = XmlHttpRequest;

    browserfs.registerFileSystem('XmlHttpRequest', XmlHttpRequest);
});
//# sourceMappingURL=XmlHttpRequest.js.map
;
/*global setImmediate: false, setTimeout: false, console: false */
(function () {

    var async = {};

    // global on the server, window in the browser
    var root, previous_async;

    root = this;
    if (root != null) {
      previous_async = root.async;
    }

    async.noConflict = function () {
        root.async = previous_async;
        return async;
    };

    function only_once(fn) {
        var called = false;
        return function() {
            if (called) throw new Error("Callback was already called.");
            called = true;
            fn.apply(root, arguments);
        }
    }

    //// cross-browser compatiblity functions ////

    var _each = function (arr, iterator) {
        if (arr.forEach) {
            return arr.forEach(iterator);
        }
        for (var i = 0; i < arr.length; i += 1) {
            iterator(arr[i], i, arr);
        }
    };

    var _map = function (arr, iterator) {
        if (arr.map) {
            return arr.map(iterator);
        }
        var results = [];
        _each(arr, function (x, i, a) {
            results.push(iterator(x, i, a));
        });
        return results;
    };

    var _reduce = function (arr, iterator, memo) {
        if (arr.reduce) {
            return arr.reduce(iterator, memo);
        }
        _each(arr, function (x, i, a) {
            memo = iterator(memo, x, i, a);
        });
        return memo;
    };

    var _keys = function (obj) {
        if (Object.keys) {
            return Object.keys(obj);
        }
        var keys = [];
        for (var k in obj) {
            if (obj.hasOwnProperty(k)) {
                keys.push(k);
            }
        }
        return keys;
    };

    //// exported async module functions ////

    //// nextTick implementation with browser-compatible fallback ////
    if (typeof process === 'undefined' || !(process.nextTick)) {
        if (typeof setImmediate === 'function') {
            async.nextTick = function (fn) {
                // not a direct alias for IE10 compatibility
                setImmediate(fn);
            };
            async.setImmediate = async.nextTick;
        }
        else {
            async.nextTick = function (fn) {
                setTimeout(fn, 0);
            };
            async.setImmediate = async.nextTick;
        }
    }
    else {
        async.nextTick = process.nextTick;
        if (typeof setImmediate !== 'undefined') {
            async.setImmediate = function (fn) {
              // not a direct alias for IE10 compatibility
              setImmediate(fn);
            };
        }
        else {
            async.setImmediate = async.nextTick;
        }
    }

    async.each = function (arr, iterator, callback) {
        callback = callback || function () {};
        if (!arr.length) {
            return callback();
        }
        var completed = 0;
        _each(arr, function (x) {
            iterator(x, only_once(function (err) {
                if (err) {
                    callback(err);
                    callback = function () {};
                }
                else {
                    completed += 1;
                    if (completed >= arr.length) {
                        callback(null);
                    }
                }
            }));
        });
    };
    async.forEach = async.each;

    async.eachSeries = function (arr, iterator, callback) {
        callback = callback || function () {};
        if (!arr.length) {
            return callback();
        }
        var completed = 0;
        var iterate = function () {
            iterator(arr[completed], function (err) {
                if (err) {
                    callback(err);
                    callback = function () {};
                }
                else {
                    completed += 1;
                    if (completed >= arr.length) {
                        callback(null);
                    }
                    else {
                        iterate();
                    }
                }
            });
        };
        iterate();
    };
    async.forEachSeries = async.eachSeries;

    async.eachLimit = function (arr, limit, iterator, callback) {
        var fn = _eachLimit(limit);
        fn.apply(null, [arr, iterator, callback]);
    };
    async.forEachLimit = async.eachLimit;

    var _eachLimit = function (limit) {

        return function (arr, iterator, callback) {
            callback = callback || function () {};
            if (!arr.length || limit <= 0) {
                return callback();
            }
            var completed = 0;
            var started = 0;
            var running = 0;

            (function replenish () {
                if (completed >= arr.length) {
                    return callback();
                }

                while (running < limit && started < arr.length) {
                    started += 1;
                    running += 1;
                    iterator(arr[started - 1], function (err) {
                        if (err) {
                            callback(err);
                            callback = function () {};
                        }
                        else {
                            completed += 1;
                            running -= 1;
                            if (completed >= arr.length) {
                                callback();
                            }
                            else {
                                replenish();
                            }
                        }
                    });
                }
            })();
        };
    };


    var doParallel = function (fn) {
        return function () {
            var args = Array.prototype.slice.call(arguments);
            return fn.apply(null, [async.each].concat(args));
        };
    };
    var doParallelLimit = function(limit, fn) {
        return function () {
            var args = Array.prototype.slice.call(arguments);
            return fn.apply(null, [_eachLimit(limit)].concat(args));
        };
    };
    var doSeries = function (fn) {
        return function () {
            var args = Array.prototype.slice.call(arguments);
            return fn.apply(null, [async.eachSeries].concat(args));
        };
    };


    var _asyncMap = function (eachfn, arr, iterator, callback) {
        var results = [];
        arr = _map(arr, function (x, i) {
            return {index: i, value: x};
        });
        eachfn(arr, function (x, callback) {
            iterator(x.value, function (err, v) {
                results[x.index] = v;
                callback(err);
            });
        }, function (err) {
            callback(err, results);
        });
    };
    async.map = doParallel(_asyncMap);
    async.mapSeries = doSeries(_asyncMap);
    async.mapLimit = function (arr, limit, iterator, callback) {
        return _mapLimit(limit)(arr, iterator, callback);
    };

    var _mapLimit = function(limit) {
        return doParallelLimit(limit, _asyncMap);
    };

    // reduce only has a series version, as doing reduce in parallel won't
    // work in many situations.
    async.reduce = function (arr, memo, iterator, callback) {
        async.eachSeries(arr, function (x, callback) {
            iterator(memo, x, function (err, v) {
                memo = v;
                callback(err);
            });
        }, function (err) {
            callback(err, memo);
        });
    };
    // inject alias
    async.inject = async.reduce;
    // foldl alias
    async.foldl = async.reduce;

    async.reduceRight = function (arr, memo, iterator, callback) {
        var reversed = _map(arr, function (x) {
            return x;
        }).reverse();
        async.reduce(reversed, memo, iterator, callback);
    };
    // foldr alias
    async.foldr = async.reduceRight;

    var _filter = function (eachfn, arr, iterator, callback) {
        var results = [];
        arr = _map(arr, function (x, i) {
            return {index: i, value: x};
        });
        eachfn(arr, function (x, callback) {
            iterator(x.value, function (v) {
                if (v) {
                    results.push(x);
                }
                callback();
            });
        }, function (err) {
            callback(_map(results.sort(function (a, b) {
                return a.index - b.index;
            }), function (x) {
                return x.value;
            }));
        });
    };
    async.filter = doParallel(_filter);
    async.filterSeries = doSeries(_filter);
    // select alias
    async.select = async.filter;
    async.selectSeries = async.filterSeries;

    var _reject = function (eachfn, arr, iterator, callback) {
        var results = [];
        arr = _map(arr, function (x, i) {
            return {index: i, value: x};
        });
        eachfn(arr, function (x, callback) {
            iterator(x.value, function (v) {
                if (!v) {
                    results.push(x);
                }
                callback();
            });
        }, function (err) {
            callback(_map(results.sort(function (a, b) {
                return a.index - b.index;
            }), function (x) {
                return x.value;
            }));
        });
    };
    async.reject = doParallel(_reject);
    async.rejectSeries = doSeries(_reject);

    var _detect = function (eachfn, arr, iterator, main_callback) {
        eachfn(arr, function (x, callback) {
            iterator(x, function (result) {
                if (result) {
                    main_callback(x);
                    main_callback = function () {};
                }
                else {
                    callback();
                }
            });
        }, function (err) {
            main_callback();
        });
    };
    async.detect = doParallel(_detect);
    async.detectSeries = doSeries(_detect);

    async.some = function (arr, iterator, main_callback) {
        async.each(arr, function (x, callback) {
            iterator(x, function (v) {
                if (v) {
                    main_callback(true);
                    main_callback = function () {};
                }
                callback();
            });
        }, function (err) {
            main_callback(false);
        });
    };
    // any alias
    async.any = async.some;

    async.every = function (arr, iterator, main_callback) {
        async.each(arr, function (x, callback) {
            iterator(x, function (v) {
                if (!v) {
                    main_callback(false);
                    main_callback = function () {};
                }
                callback();
            });
        }, function (err) {
            main_callback(true);
        });
    };
    // all alias
    async.all = async.every;

    async.sortBy = function (arr, iterator, callback) {
        async.map(arr, function (x, callback) {
            iterator(x, function (err, criteria) {
                if (err) {
                    callback(err);
                }
                else {
                    callback(null, {value: x, criteria: criteria});
                }
            });
        }, function (err, results) {
            if (err) {
                return callback(err);
            }
            else {
                var fn = function (left, right) {
                    var a = left.criteria, b = right.criteria;
                    return a < b ? -1 : a > b ? 1 : 0;
                };
                callback(null, _map(results.sort(fn), function (x) {
                    return x.value;
                }));
            }
        });
    };

    async.auto = function (tasks, callback) {
        callback = callback || function () {};
        var keys = _keys(tasks);
        if (!keys.length) {
            return callback(null);
        }

        var results = {};

        var listeners = [];
        var addListener = function (fn) {
            listeners.unshift(fn);
        };
        var removeListener = function (fn) {
            for (var i = 0; i < listeners.length; i += 1) {
                if (listeners[i] === fn) {
                    listeners.splice(i, 1);
                    return;
                }
            }
        };
        var taskComplete = function () {
            _each(listeners.slice(0), function (fn) {
                fn();
            });
        };

        addListener(function () {
            if (_keys(results).length === keys.length) {
                callback(null, results);
                callback = function () {};
            }
        });

        _each(keys, function (k) {
            var task = (tasks[k] instanceof Function) ? [tasks[k]]: tasks[k];
            var taskCallback = function (err) {
                var args = Array.prototype.slice.call(arguments, 1);
                if (args.length <= 1) {
                    args = args[0];
                }
                if (err) {
                    var safeResults = {};
                    _each(_keys(results), function(rkey) {
                        safeResults[rkey] = results[rkey];
                    });
                    safeResults[k] = args;
                    callback(err, safeResults);
                    // stop subsequent errors hitting callback multiple times
                    callback = function () {};
                }
                else {
                    results[k] = args;
                    async.setImmediate(taskComplete);
                }
            };
            var requires = task.slice(0, Math.abs(task.length - 1)) || [];
            var ready = function () {
                return _reduce(requires, function (a, x) {
                    return (a && results.hasOwnProperty(x));
                }, true) && !results.hasOwnProperty(k);
            };
            if (ready()) {
                task[task.length - 1](taskCallback, results);
            }
            else {
                var listener = function () {
                    if (ready()) {
                        removeListener(listener);
                        task[task.length - 1](taskCallback, results);
                    }
                };
                addListener(listener);
            }
        });
    };

    async.waterfall = function (tasks, callback) {
        callback = callback || function () {};
        if (tasks.constructor !== Array) {
          var err = new Error('First argument to waterfall must be an array of functions');
          return callback(err);
        }
        if (!tasks.length) {
            return callback();
        }
        var wrapIterator = function (iterator) {
            return function (err) {
                if (err) {
                    callback.apply(null, arguments);
                    callback = function () {};
                }
                else {
                    var args = Array.prototype.slice.call(arguments, 1);
                    var next = iterator.next();
                    if (next) {
                        args.push(wrapIterator(next));
                    }
                    else {
                        args.push(callback);
                    }
                    async.setImmediate(function () {
                        iterator.apply(null, args);
                    });
                }
            };
        };
        wrapIterator(async.iterator(tasks))();
    };

    var _parallel = function(eachfn, tasks, callback) {
        callback = callback || function () {};
        if (tasks.constructor === Array) {
            eachfn.map(tasks, function (fn, callback) {
                if (fn) {
                    fn(function (err) {
                        var args = Array.prototype.slice.call(arguments, 1);
                        if (args.length <= 1) {
                            args = args[0];
                        }
                        callback.call(null, err, args);
                    });
                }
            }, callback);
        }
        else {
            var results = {};
            eachfn.each(_keys(tasks), function (k, callback) {
                tasks[k](function (err) {
                    var args = Array.prototype.slice.call(arguments, 1);
                    if (args.length <= 1) {
                        args = args[0];
                    }
                    results[k] = args;
                    callback(err);
                });
            }, function (err) {
                callback(err, results);
            });
        }
    };

    async.parallel = function (tasks, callback) {
        _parallel({ map: async.map, each: async.each }, tasks, callback);
    };

    async.parallelLimit = function(tasks, limit, callback) {
        _parallel({ map: _mapLimit(limit), each: _eachLimit(limit) }, tasks, callback);
    };

    async.series = function (tasks, callback) {
        callback = callback || function () {};
        if (tasks.constructor === Array) {
            async.mapSeries(tasks, function (fn, callback) {
                if (fn) {
                    fn(function (err) {
                        var args = Array.prototype.slice.call(arguments, 1);
                        if (args.length <= 1) {
                            args = args[0];
                        }
                        callback.call(null, err, args);
                    });
                }
            }, callback);
        }
        else {
            var results = {};
            async.eachSeries(_keys(tasks), function (k, callback) {
                tasks[k](function (err) {
                    var args = Array.prototype.slice.call(arguments, 1);
                    if (args.length <= 1) {
                        args = args[0];
                    }
                    results[k] = args;
                    callback(err);
                });
            }, function (err) {
                callback(err, results);
            });
        }
    };

    async.iterator = function (tasks) {
        var makeCallback = function (index) {
            var fn = function () {
                if (tasks.length) {
                    tasks[index].apply(null, arguments);
                }
                return fn.next();
            };
            fn.next = function () {
                return (index < tasks.length - 1) ? makeCallback(index + 1): null;
            };
            return fn;
        };
        return makeCallback(0);
    };

    async.apply = function (fn) {
        var args = Array.prototype.slice.call(arguments, 1);
        return function () {
            return fn.apply(
                null, args.concat(Array.prototype.slice.call(arguments))
            );
        };
    };

    var _concat = function (eachfn, arr, fn, callback) {
        var r = [];
        eachfn(arr, function (x, cb) {
            fn(x, function (err, y) {
                r = r.concat(y || []);
                cb(err);
            });
        }, function (err) {
            callback(err, r);
        });
    };
    async.concat = doParallel(_concat);
    async.concatSeries = doSeries(_concat);

    async.whilst = function (test, iterator, callback) {
        if (test()) {
            iterator(function (err) {
                if (err) {
                    return callback(err);
                }
                async.whilst(test, iterator, callback);
            });
        }
        else {
            callback();
        }
    };

    async.doWhilst = function (iterator, test, callback) {
        iterator(function (err) {
            if (err) {
                return callback(err);
            }
            if (test()) {
                async.doWhilst(iterator, test, callback);
            }
            else {
                callback();
            }
        });
    };

    async.until = function (test, iterator, callback) {
        if (!test()) {
            iterator(function (err) {
                if (err) {
                    return callback(err);
                }
                async.until(test, iterator, callback);
            });
        }
        else {
            callback();
        }
    };

    async.doUntil = function (iterator, test, callback) {
        iterator(function (err) {
            if (err) {
                return callback(err);
            }
            if (!test()) {
                async.doUntil(iterator, test, callback);
            }
            else {
                callback();
            }
        });
    };

    async.queue = function (worker, concurrency) {
        if (concurrency === undefined) {
            concurrency = 1;
        }
        function _insert(q, data, pos, callback) {
          if(data.constructor !== Array) {
              data = [data];
          }
          _each(data, function(task) {
              var item = {
                  data: task,
                  callback: typeof callback === 'function' ? callback : null
              };

              if (pos) {
                q.tasks.unshift(item);
              } else {
                q.tasks.push(item);
              }

              if (q.saturated && q.tasks.length === concurrency) {
                  q.saturated();
              }
              async.setImmediate(q.process);
          });
        }

        var workers = 0;
        var q = {
            tasks: [],
            concurrency: concurrency,
            saturated: null,
            empty: null,
            drain: null,
            push: function (data, callback) {
              _insert(q, data, false, callback);
            },
            unshift: function (data, callback) {
              _insert(q, data, true, callback);
            },
            process: function () {
                if (workers < q.concurrency && q.tasks.length) {
                    var task = q.tasks.shift();
                    if (q.empty && q.tasks.length === 0) {
                        q.empty();
                    }
                    workers += 1;
                    var next = function () {
                        workers -= 1;
                        if (task.callback) {
                            task.callback.apply(task, arguments);
                        }
                        if (q.drain && q.tasks.length + workers === 0) {
                            q.drain();
                        }
                        q.process();
                    };
                    var cb = only_once(next);
                    worker(task.data, cb);
                }
            },
            length: function () {
                return q.tasks.length;
            },
            running: function () {
                return workers;
            }
        };
        return q;
    };

    async.cargo = function (worker, payload) {
        var working     = false,
            tasks       = [];

        var cargo = {
            tasks: tasks,
            payload: payload,
            saturated: null,
            empty: null,
            drain: null,
            push: function (data, callback) {
                if(data.constructor !== Array) {
                    data = [data];
                }
                _each(data, function(task) {
                    tasks.push({
                        data: task,
                        callback: typeof callback === 'function' ? callback : null
                    });
                    if (cargo.saturated && tasks.length === payload) {
                        cargo.saturated();
                    }
                });
                async.setImmediate(cargo.process);
            },
            process: function process() {
                if (working) return;
                if (tasks.length === 0) {
                    if(cargo.drain) cargo.drain();
                    return;
                }

                var ts = typeof payload === 'number'
                            ? tasks.splice(0, payload)
                            : tasks.splice(0);

                var ds = _map(ts, function (task) {
                    return task.data;
                });

                if(cargo.empty) cargo.empty();
                working = true;
                worker(ds, function () {
                    working = false;

                    var args = arguments;
                    _each(ts, function (data) {
                        if (data.callback) {
                            data.callback.apply(null, args);
                        }
                    });

                    process();
                });
            },
            length: function () {
                return tasks.length;
            },
            running: function () {
                return working;
            }
        };
        return cargo;
    };

    var _console_fn = function (name) {
        return function (fn) {
            var args = Array.prototype.slice.call(arguments, 1);
            fn.apply(null, args.concat([function (err) {
                var args = Array.prototype.slice.call(arguments, 1);
                if (typeof console !== 'undefined') {
                    if (err) {
                        if (console.error) {
                            console.error(err);
                        }
                    }
                    else if (console[name]) {
                        _each(args, function (x) {
                            console[name](x);
                        });
                    }
                }
            }]));
        };
    };
    async.log = _console_fn('log');
    async.dir = _console_fn('dir');
    /*async.info = _console_fn('info');
    async.warn = _console_fn('warn');
    async.error = _console_fn('error');*/

    async.memoize = function (fn, hasher) {
        var memo = {};
        var queues = {};
        hasher = hasher || function (x) {
            return x;
        };
        var memoized = function () {
            var args = Array.prototype.slice.call(arguments);
            var callback = args.pop();
            var key = hasher.apply(null, args);
            if (key in memo) {
                callback.apply(null, memo[key]);
            }
            else if (key in queues) {
                queues[key].push(callback);
            }
            else {
                queues[key] = [callback];
                fn.apply(null, args.concat([function () {
                    memo[key] = arguments;
                    var q = queues[key];
                    delete queues[key];
                    for (var i = 0, l = q.length; i < l; i++) {
                      q[i].apply(null, arguments);
                    }
                }]));
            }
        };
        memoized.memo = memo;
        memoized.unmemoized = fn;
        return memoized;
    };

    async.unmemoize = function (fn) {
      return function () {
        return (fn.unmemoized || fn).apply(null, arguments);
      };
    };

    async.times = function (count, iterator, callback) {
        var counter = [];
        for (var i = 0; i < count; i++) {
            counter.push(i);
        }
        return async.map(counter, iterator, callback);
    };

    async.timesSeries = function (count, iterator, callback) {
        var counter = [];
        for (var i = 0; i < count; i++) {
            counter.push(i);
        }
        return async.mapSeries(counter, iterator, callback);
    };

    async.compose = function (/* functions... */) {
        var fns = Array.prototype.reverse.call(arguments);
        return function () {
            var that = this;
            var args = Array.prototype.slice.call(arguments);
            var callback = args.pop();
            async.reduce(fns, args, function (newargs, fn, cb) {
                fn.apply(that, newargs.concat([function () {
                    var err = arguments[0];
                    var nextargs = Array.prototype.slice.call(arguments, 1);
                    cb(err, nextargs);
                }]))
            },
            function (err, results) {
                callback.apply(that, [err].concat(results));
            });
        };
    };

    var _applyEach = function (eachfn, fns /*args...*/) {
        var go = function () {
            var that = this;
            var args = Array.prototype.slice.call(arguments);
            var callback = args.pop();
            return eachfn(fns, function (fn, cb) {
                fn.apply(that, args.concat([cb]));
            },
            callback);
        };
        if (arguments.length > 2) {
            var args = Array.prototype.slice.call(arguments, 2);
            return go.apply(this, args);
        }
        else {
            return go;
        }
    };
    async.applyEach = doParallel(_applyEach);
    async.applyEachSeries = doSeries(_applyEach);

    async.forever = function (fn, callback) {
        function next(err) {
            if (err) {
                if (callback) {
                    return callback(err);
                }
                throw err;
            }
            fn(next);
        }
        next();
    };

    // AMD / RequireJS
    if (typeof define !== 'undefined' && define.amd) {
        define('async',[], function () {
            return async;
        });
    }
    // Node.js
    else if (typeof module !== 'undefined' && module.exports) {
        module.exports = async;
    }
    // included directly via <script> tag
    else {
        root.async = async;
    }

}());

var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/dropbox',["require", "exports", '../generic/preload_file', '../core/file_system', '../core/node_fs_stats', '../core/buffer', '../core/api_error', '../core/node_path', '../core/browserfs', '../core/buffer_core_arraybuffer', "async"], function(require, exports, preload_file, file_system, node_fs_stats, buffer, api_error, node_path, browserfs, buffer_core_arraybuffer) {
    var Buffer = buffer.Buffer;
    var Stats = node_fs_stats.Stats;
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var path = node_path.path;
    var FileType = node_fs_stats.FileType;

    // XXX: No typings available for the Dropbox client. :(
    // XXX: The typings for async on DefinitelyTyped are out of date.
    var async = require('async');
    var Buffer = buffer.Buffer;

    var DropboxFile = (function (_super) {
        __extends(DropboxFile, _super);
        function DropboxFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this, _fs, _path, _flag, _stat, contents);
        }
        DropboxFile.prototype.sync = function (cb) {
            var buffer = this._buffer;

            // XXX: Typing hack.
            var backing_mem = this._buffer.getBufferCore();
            if (!(backing_mem instanceof buffer_core_arraybuffer.BufferCoreArrayBuffer)) {
                // Copy into an ArrayBuffer-backed Buffer.
                buffer = new Buffer(this._buffer.length);
                this._buffer.copy(buffer);
                backing_mem = buffer.getBufferCore();
            }

            // Reach into the BC, grab the DV.
            var dv = backing_mem.getDataView();

            // Create an appropriate view on the array buffer.
            var abv = new DataView(dv.buffer, dv.byteOffset + buffer.getOffset(), buffer.length);
            this._fs._writeFileStrict(this._path, abv, cb);
        };

        DropboxFile.prototype.close = function (cb) {
            this.sync(cb);
        };
        return DropboxFile;
    })(preload_file.PreloadFile);
    exports.DropboxFile = DropboxFile;

    var DropboxFileSystem = (function (_super) {
        __extends(DropboxFileSystem, _super);
        /**
        * Arguments: an authenticated Dropbox.js client
        */
        function DropboxFileSystem(client) {
            _super.call(this);
            this.client = client;
        }
        DropboxFileSystem.prototype.getName = function () {
            return 'Dropbox';
        };

        DropboxFileSystem.isAvailable = function () {
            // Checks if the Dropbox library is loaded.
            // @todo Check if the Dropbox library *can be used* in the current browser.
            return typeof Dropbox !== 'undefined';
        };

        DropboxFileSystem.prototype.isReadOnly = function () {
            return false;
        };

        // Dropbox doesn't support symlinks, properties, or synchronous calls
        DropboxFileSystem.prototype.supportsSymlinks = function () {
            return false;
        };

        DropboxFileSystem.prototype.supportsProps = function () {
            return false;
        };

        DropboxFileSystem.prototype.supportsSynch = function () {
            return false;
        };

        DropboxFileSystem.prototype.empty = function (main_cb) {
            var _this = this;
            this.client.readdir('/', function (error, paths, dir, files) {
                if (error) {
                    main_cb(_this.convert(error));
                } else {
                    var deleteFile = function (file, cb) {
                        _this.client.remove(file.path, function (err, stat) {
                            cb(err ? _this.convert(err) : err);
                        });
                    };
                    var finished = function (err) {
                        if (err) {
                            main_cb(_this.convert(err));
                        } else {
                            main_cb();
                        }
                    };
                    async.each(files, deleteFile, finished);
                }
            });
        };

        DropboxFileSystem.prototype.rename = function (oldPath, newPath, cb) {
            this.client.move(oldPath, newPath, function (error, stat) {
                if (error) {
                    // XXX: Assume 404 for now.
                    var missingPath = error.response.error.indexOf(oldPath) > -1 ? oldPath : newPath;
                    cb(new ApiError(1 /* ENOENT */, missingPath + " doesn't exist"));
                } else {
                    cb();
                }
            });
        };

        DropboxFileSystem.prototype.stat = function (path, isLstat, cb) {
            var _this = this;
            // Ignore lstat case -- Dropbox doesn't support symlinks
            // Stat the file
            this.client.stat(path, function (error, stat) {
                // Dropbox keeps track of deleted files, so if a file has existed in the
                // past but doesn't any longer, you wont get an error
                if (error || ((stat != null) && stat.isRemoved)) {
                    cb(new ApiError(1 /* ENOENT */, path + " doesn't exist"));
                } else {
                    var stats = new Stats(_this._statType(stat), stat.size);
                    return cb(null, stats);
                }
            });
        };

        DropboxFileSystem.prototype.open = function (path, flags, mode, cb) {
            var _this = this;
            // Try and get the file's contents
            this.client.readFile(path, {
                arrayBuffer: true
            }, function (error, content, db_stat, range) {
                if (error) {
                    // If the file's being opened for reading and doesn't exist, return an
                    // error
                    if (flags.isReadable()) {
                        cb(new ApiError(1 /* ENOENT */, path + " doesn't exist"));
                    } else {
                        switch (error.status) {
                            case 0:
                                return console.error('No connection');

                            case 404:
                                var ab = new ArrayBuffer(0);
                                return _this._writeFileStrict(path, ab, function (error2, stat) {
                                    if (error2) {
                                        cb(error2);
                                    } else {
                                        var file = _this._makeFile(path, flags, stat, new Buffer(ab));
                                        cb(null, file);
                                    }
                                });
                            default:
                                return console.log("Unhandled error: " + error);
                        }
                    }
                } else {
                    // No error
                    var buffer;

                    // Dropbox.js seems to set `content` to `null` rather than to an empty
                    // buffer when reading an empty file. Not sure why this is.
                    if (content === null) {
                        buffer = new Buffer(0);
                    } else {
                        buffer = new Buffer(content);
                    }
                    var file = _this._makeFile(path, flags, db_stat, buffer);
                    return cb(null, file);
                }
            });
        };

        DropboxFileSystem.prototype._writeFileStrict = function (p, data, cb) {
            var _this = this;
            var parent = path.dirname(p);
            this.stat(parent, false, function (error, stat) {
                if (error) {
                    cb(new ApiError(1 /* ENOENT */, "Can't create " + p + " because " + parent + " doesn't exist"));
                } else {
                    _this.client.writeFile(p, data, function (error2, stat) {
                        if (error2) {
                            cb(_this.convert(error2));
                        } else {
                            cb(null, stat);
                        }
                    });
                }
            });
        };

        /**
        * Private
        * Returns a BrowserFS object representing the type of a Dropbox.js stat object
        */
        DropboxFileSystem.prototype._statType = function (stat) {
            return stat.isFile ? 32768 /* FILE */ : 16384 /* DIRECTORY */;
        };

        /**
        * Private
        * Returns a BrowserFS object representing a File, created from the data
        * returned by calls to the Dropbox API.
        */
        DropboxFileSystem.prototype._makeFile = function (path, flag, stat, buffer) {
            var type = this._statType(stat);
            var stats = new Stats(type, stat.size);
            return new DropboxFile(this, path, flag, stats, buffer);
        };

        /**
        * Private
        * Delete a file or directory from Dropbox
        * isFile should reflect which call was made to remove the it (`unlink` or
        * `rmdir`). If this doesn't match what's actually at `path`, an error will be
        * returned
        */
        DropboxFileSystem.prototype._remove = function (path, cb, isFile) {
            var _this = this;
            this.client.stat(path, function (error, stat) {
                var message = null;
                if (error) {
                    cb(new ApiError(1 /* ENOENT */, path + " doesn't exist"));
                } else {
                    if (stat.isFile && !isFile) {
                        cb(new ApiError(7 /* ENOTDIR */, path + " is a file."));
                    } else if (!stat.isFile && isFile) {
                        cb(new ApiError(8 /* EISDIR */, path + " is a directory."));
                    } else {
                        _this.client.remove(path, function (error, stat) {
                            if (error) {
                                // @todo Make this more specific.
                                cb(new ApiError(2 /* EIO */, "Failed to remove " + path));
                            } else {
                                cb(null);
                            }
                        });
                    }
                }
            });
        };

        /**
        * Delete a file
        */
        DropboxFileSystem.prototype.unlink = function (path, cb) {
            this._remove(path, cb, true);
        };

        /**
        * Delete a directory
        */
        DropboxFileSystem.prototype.rmdir = function (path, cb) {
            this._remove(path, cb, false);
        };

        /**
        * Create a directory
        */
        DropboxFileSystem.prototype.mkdir = function (p, mode, cb) {
            var _this = this;
            // Dropbox.js' client.mkdir() behaves like `mkdir -p`, i.e. it creates a
            // directory and all its ancestors if they don't exist.
            // Node's fs.mkdir() behaves like `mkdir`, i.e. it throws an error if an attempt
            // is made to create a directory without a parent.
            // To handle this inconsistency, a check for the existence of `path`'s parent
            // must be performed before it is created, and an error thrown if it does
            // not exist
            var parent = path.dirname(p);
            this.client.stat(parent, function (error, stat) {
                if (error) {
                    cb(new ApiError(1 /* ENOENT */, "Can't create " + p + " because " + parent + " doesn't exist"));
                } else {
                    _this.client.mkdir(p, function (error, stat) {
                        if (error) {
                            cb(new ApiError(6 /* EEXIST */, p + " already exists"));
                        } else {
                            cb(null);
                        }
                    });
                }
            });
        };

        /**
        * Get the names of the files in a directory
        */
        DropboxFileSystem.prototype.readdir = function (path, cb) {
            var _this = this;
            this.client.readdir(path, function (error, files, dir_stat, content_stats) {
                if (error) {
                    return cb(_this.convert(error));
                } else {
                    return cb(null, files);
                }
            });
        };

        /**
        * Converts a Dropbox-JS error into a BFS error.
        */
        DropboxFileSystem.prototype.convert = function (err, message) {
            if (typeof message === "undefined") { message = ""; }
            switch (err.status) {
                case 400:
                    // INVALID_PARAM
                    return new ApiError(9 /* EINVAL */, message);
                case 401:

                case 403:
                    // OAUTH_ERROR
                    return new ApiError(2 /* EIO */, message);
                case 404:
                    // NOT_FOUND
                    return new ApiError(1 /* ENOENT */, message);
                case 405:
                    // INVALID_METHOD
                    return new ApiError(14 /* ENOTSUP */, message);

                case 0:

                case 304:

                case 406:

                case 409:

                default:
                    return new ApiError(2 /* EIO */, message);
            }
        };
        return DropboxFileSystem;
    })(file_system.BaseFileSystem);
    exports.DropboxFileSystem = DropboxFileSystem;

    browserfs.registerFileSystem('Dropbox', DropboxFileSystem);
});
//# sourceMappingURL=dropbox.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/html5fs',["require", "exports", '../generic/preload_file', '../core/file_system', '../core/api_error', '../core/file_flag', '../core/node_fs_stats', '../core/buffer', '../core/browserfs', '../core/buffer_core_arraybuffer', '../core/node_path', '../core/global', "async"], function(require, exports, preload_file, file_system, api_error, file_flag, node_fs_stats, buffer, browserfs, buffer_core_arraybuffer, node_path, global) {
    var Buffer = buffer.Buffer;
    var Stats = node_fs_stats.Stats;
    var FileType = node_fs_stats.FileType;
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var ActionType = file_flag.ActionType;

    // XXX: The typings for async on DefinitelyTyped are out of date.
    var async = require('async');

    var _getFS = global.webkitRequestFileSystem || global.requestFileSystem || null;

    function _requestQuota(type, size, success, errorCallback) {
        // We cast navigator and window to '<any>' because everything here is
        // nonstandard functionality, despite the fact that Chrome has the only
        // implementation of the HTML5FS and is likely driving the standardization
        // process. Thus, these objects defined off of navigator and window are not
        // present in the DefinitelyTyped TypeScript typings for FileSystem.
        if (typeof navigator['webkitPersistentStorage'] !== 'undefined') {
            switch (type) {
                case global.PERSISTENT:
                    navigator.webkitPersistentStorage.requestQuota(size, success, errorCallback);
                    break;
                case global.TEMPORARY:
                    navigator.webkitTemporaryStorage.requestQuota(size, success, errorCallback);
                    break;
                default:
                    // TODO: Figure out how to construct a DOMException/DOMError.
                    errorCallback(null);
                    break;
            }
        } else {
            global.webkitStorageInfo.requestQuota(type, size, success, errorCallback);
        }
    }

    function _toArray(list) {
        return Array.prototype.slice.call(list || [], 0);
    }

    // A note about getFile and getDirectory options:
    // These methods are called at numerous places in this file, and are passed
    // some combination of these two options:
    //   - create: If true, the entry will be created if it doesn't exist.
    //             If false, an error will be thrown if it doesn't exist.
    //   - exclusive: If true, only create the entry if it doesn't already exist,
    //                and throw an error if it does.
    var HTML5FSFile = (function (_super) {
        __extends(HTML5FSFile, _super);
        function HTML5FSFile(_fs, _path, _flag, _stat, contents) {
            _super.call(this, _fs, _path, _flag, _stat, contents);
        }
        HTML5FSFile.prototype.sync = function (cb) {
            var _this = this;
            // Don't create the file (it should already have been created by `open`)
            var opts = {
                create: false
            };
            var _fs = this._fs;
            var success = function (entry) {
                entry.createWriter(function (writer) {
                    // XXX: Typing hack.
                    var buffer = _this._buffer;
                    var backing_mem = _this._buffer.getBufferCore();
                    if (!(backing_mem instanceof buffer_core_arraybuffer.BufferCoreArrayBuffer)) {
                        // Copy into an ArrayBuffer-backed Buffer.
                        buffer = new Buffer(_this._buffer.length);
                        _this._buffer.copy(buffer);
                        backing_mem = buffer.getBufferCore();
                    }

                    // Reach into the BC, grab the DV.
                    var dv = backing_mem.getDataView();

                    // Create an appropriate view on the array buffer.
                    var abv = new DataView(dv.buffer, dv.byteOffset + buffer.getOffset(), buffer.length);
                    var blob = new Blob([abv]);
                    var length = blob.size;
                    writer.onwriteend = function (event) {
                        writer.onwriteend = null;
                        writer.truncate(length);
                        cb();
                    };
                    writer.onerror = function (err) {
                        cb(_fs.convert(err));
                    };
                    writer.write(blob);
                });
            };
            var error = function (err) {
                cb(_fs.convert(err));
            };
            _fs.fs.root.getFile(this._path, opts, success, error);
        };

        HTML5FSFile.prototype.close = function (cb) {
            this.sync(cb);
        };
        return HTML5FSFile;
    })(preload_file.PreloadFile);
    exports.HTML5FSFile = HTML5FSFile;

    var HTML5FS = (function (_super) {
        __extends(HTML5FS, _super);
        /**
        * Arguments:
        *   - type: PERSISTENT or TEMPORARY
        *   - size: storage quota to request, in megabytes. Allocated value may be less.
        */
        function HTML5FS(size, type) {
            _super.call(this);
            this.size = size != null ? size : 5;
            this.type = type != null ? type : global.PERSISTENT;
            var kb = 1024;
            var mb = kb * kb;
            this.size *= mb;
        }
        HTML5FS.prototype.getName = function () {
            return 'HTML5 FileSystem';
        };

        HTML5FS.isAvailable = function () {
            return _getFS != null;
        };

        HTML5FS.prototype.isReadOnly = function () {
            return false;
        };

        HTML5FS.prototype.supportsSymlinks = function () {
            return false;
        };

        HTML5FS.prototype.supportsProps = function () {
            return false;
        };

        HTML5FS.prototype.supportsSynch = function () {
            return false;
        };

        /**
        * Converts the given DOMError into an appropriate ApiError.
        * Full list of values here:
        * https://developer.mozilla.org/en-US/docs/Web/API/DOMError
        * I've only implemented the most obvious ones, but more can be added to
        * make errors more descriptive in the future.
        */
        HTML5FS.prototype.convert = function (err, message) {
            if (typeof message === "undefined") { message = ""; }
            switch (err.name) {
                case 'QuotaExceededError':
                    return new ApiError(11 /* ENOSPC */, message);
                case 'NotFoundError':
                    return new ApiError(1 /* ENOENT */, message);
                case 'SecurityError':
                    return new ApiError(4 /* EACCES */, message);
                case 'InvalidModificationError':
                    return new ApiError(0 /* EPERM */, message);
                case 'SyntaxError':
                case 'TypeMismatchError':
                    return new ApiError(9 /* EINVAL */, message);
                default:
                    return new ApiError(9 /* EINVAL */, message);
            }
        };

        /**
        * Converts the given ErrorEvent (from a FileReader) into an appropriate
        * APIError.
        */
        HTML5FS.prototype.convertErrorEvent = function (err, message) {
            if (typeof message === "undefined") { message = ""; }
            return new ApiError(1 /* ENOENT */, err.message + "; " + message);
        };

        /**
        * Nonstandard
        * Requests a storage quota from the browser to back this FS.
        */
        HTML5FS.prototype.allocate = function (cb) {
            var _this = this;
            if (typeof cb === "undefined") { cb = function () {
            }; }
            var success = function (fs) {
                _this.fs = fs;
                cb();
            };
            var error = function (err) {
                cb(_this.convert(err));
            };
            if (this.type === global.PERSISTENT) {
                _requestQuota(this.type, this.size, function (granted) {
                    _getFS(_this.type, granted, success, error);
                }, error);
            } else {
                _getFS(this.type, this.size, success, error);
            }
        };

        /**
        * Nonstandard
        * Deletes everything in the FS. Used for testing.
        * Karma clears the storage after you quit it but not between runs of the test
        * suite, and the tests expect an empty FS every time.
        */
        HTML5FS.prototype.empty = function (main_cb) {
            var _this = this;
            // Get a list of all entries in the root directory to delete them
            this._readdir('/', function (err, entries) {
                if (err) {
                    console.error('Failed to empty FS');
                    main_cb(err);
                } else {
                    // Called when every entry has been operated on
                    var finished = function (er) {
                        if (err) {
                            console.error("Failed to empty FS");
                            main_cb(err);
                        } else {
                            main_cb();
                        }
                    };

                    // Removes files and recursively removes directories
                    var deleteEntry = function (entry, cb) {
                        var succ = function () {
                            cb();
                        };
                        var error = function (err) {
                            cb(_this.convert(err, entry.fullPath));
                        };
                        if (entry.isFile) {
                            entry.remove(succ, error);
                        } else {
                            entry.removeRecursively(succ, error);
                        }
                    };

                    // Loop through the entries and remove them, then call the callback
                    // when they're all finished.
                    async.each(entries, deleteEntry, finished);
                }
            });
        };

        HTML5FS.prototype.rename = function (oldPath, newPath, cb) {
            var _this = this;
            var semaphore = 2, successCount = 0, root = this.fs.root, error = function (err) {
                if (--semaphore === 0) {
                    cb(_this.convert(err, "Failed to rename " + oldPath + " to " + newPath + "."));
                }
            }, success = function (file) {
                if (++successCount === 2) {
                    console.error("Something was identified as both a file and a directory. This should never happen.");
                    return;
                }

                // SPECIAL CASE: If newPath === oldPath, and the path exists, then
                // this operation trivially succeeds.
                if (oldPath === newPath) {
                    return cb();
                }

                // Get the new parent directory.
                root.getDirectory(node_path.path.dirname(newPath), {}, function (parentDir) {
                    file.moveTo(parentDir, node_path.path.basename(newPath), function (entry) {
                        cb();
                    }, function (err) {
                        // SPECIAL CASE: If oldPath is a directory, and newPath is a
                        // file, rename should delete the file and perform the move.
                        if (file.isDirectory) {
                            // Unlink only works on files. Try to delete newPath.
                            _this.unlink(newPath, function (e) {
                                if (e) {
                                    // newPath is probably a directory.
                                    error(err);
                                } else {
                                    // Recurse, now that newPath doesn't exist.
                                    _this.rename(oldPath, newPath, cb);
                                }
                            });
                        } else {
                            error(err);
                        }
                    });
                }, error);
            };

            // We don't know if oldPath is a *file* or a *directory*, and there's no
            // way to stat items. So launch both requests, see which one succeeds.
            root.getFile(oldPath, {}, success, error);
            root.getDirectory(oldPath, {}, success, error);
        };

        HTML5FS.prototype.stat = function (path, isLstat, cb) {
            var _this = this;
            // Throw an error if the entry doesn't exist, because then there's nothing
            // to stat.
            var opts = {
                create: false
            };

            // Called when the path has been successfully loaded as a file.
            var loadAsFile = function (entry) {
                var fileFromEntry = function (file) {
                    var stat = new Stats(32768 /* FILE */, file.size);
                    cb(null, stat);
                };
                entry.file(fileFromEntry, failedToLoad);
            };

            // Called when the path has been successfully loaded as a directory.
            var loadAsDir = function (dir) {
                // Directory entry size can't be determined from the HTML5 FS API, and is
                // implementation-dependant anyway, so a dummy value is used.
                var size = 4096;
                var stat = new Stats(16384 /* DIRECTORY */, size);
                cb(null, stat);
            };

            // Called when the path couldn't be opened as a directory or a file.
            var failedToLoad = function (err) {
                cb(_this.convert(err, path));
            };

            // Called when the path couldn't be opened as a file, but might still be a
            // directory.
            var failedToLoadAsFile = function () {
                _this.fs.root.getDirectory(path, opts, loadAsDir, failedToLoad);
            };

            // No method currently exists to determine whether a path refers to a
            // directory or a file, so this implementation tries both and uses the first
            // one that succeeds.
            this.fs.root.getFile(path, opts, loadAsFile, failedToLoadAsFile);
        };

        HTML5FS.prototype.open = function (path, flags, mode, cb) {
            var _this = this;
            var opts = {
                create: flags.pathNotExistsAction() === 3 /* CREATE_FILE */,
                exclusive: flags.isExclusive()
            };
            var error = function (err) {
                cb(_this.convertErrorEvent(err, path));
            };
            var error2 = function (err) {
                cb(_this.convert(err, path));
            };
            var success = function (entry) {
                var success2 = function (file) {
                    var reader = new FileReader();
                    reader.onloadend = function (event) {
                        var bfs_file = _this._makeFile(path, flags, file, reader.result);
                        cb(null, bfs_file);
                    };
                    reader.onerror = error;
                    reader.readAsArrayBuffer(file);
                };
                entry.file(success2, error2);
            };
            this.fs.root.getFile(path, opts, success, error);
        };

        /**
        * Returns a BrowserFS object representing the type of a Dropbox.js stat object
        */
        HTML5FS.prototype._statType = function (stat) {
            return stat.isFile ? 32768 /* FILE */ : 16384 /* DIRECTORY */;
        };

        /**
        * Returns a BrowserFS object representing a File, created from the data
        * returned by calls to the Dropbox API.
        */
        HTML5FS.prototype._makeFile = function (path, flag, stat, data) {
            if (typeof data === "undefined") { data = new ArrayBuffer(0); }
            var stats = new Stats(32768 /* FILE */, stat.size);
            var buffer = new Buffer(data);
            return new HTML5FSFile(this, path, flag, stats, buffer);
        };

        /**
        * Delete a file or directory from the file system
        * isFile should reflect which call was made to remove the it (`unlink` or
        * `rmdir`). If this doesn't match what's actually at `path`, an error will be
        * returned
        */
        HTML5FS.prototype._remove = function (path, cb, isFile) {
            var _this = this;
            var success = function (entry) {
                var succ = function () {
                    cb();
                };
                var err = function (err) {
                    cb(_this.convert(err, path));
                };
                entry.remove(succ, err);
            };
            var error = function (err) {
                cb(_this.convert(err, path));
            };

            // Deleting the entry, so don't create it
            var opts = {
                create: false
            };

            if (isFile) {
                this.fs.root.getFile(path, opts, success, error);
            } else {
                this.fs.root.getDirectory(path, opts, success, error);
            }
        };

        HTML5FS.prototype.unlink = function (path, cb) {
            this._remove(path, cb, true);
        };

        HTML5FS.prototype.rmdir = function (path, cb) {
            this._remove(path, cb, false);
        };

        HTML5FS.prototype.mkdir = function (path, mode, cb) {
            var _this = this;
            // Create the directory, but throw an error if it already exists, as per
            // mkdir(1)
            var opts = {
                create: true,
                exclusive: true
            };
            var success = function (dir) {
                cb();
            };
            var error = function (err) {
                cb(_this.convert(err, path));
            };
            this.fs.root.getDirectory(path, opts, success, error);
        };

        /**
        * Returns an array of `FileEntry`s. Used internally by empty and readdir.
        */
        HTML5FS.prototype._readdir = function (path, cb) {
            var _this = this;
            // Grab the requested directory.
            this.fs.root.getDirectory(path, { create: false }, function (dirEntry) {
                var reader = dirEntry.createReader();
                var entries = [];
                var error = function (err) {
                    cb(_this.convert(err, path));
                };

                // Call the reader.readEntries() until no more results are returned.
                var readEntries = function () {
                    reader.readEntries((function (results) {
                        if (results.length) {
                            entries = entries.concat(_toArray(results));
                            readEntries();
                        } else {
                            cb(null, entries);
                        }
                    }), error);
                };
                readEntries();
            });
        };

        /**
        * Map _readdir's list of `FileEntry`s to their names and return that.
        */
        HTML5FS.prototype.readdir = function (path, cb) {
            this._readdir(path, function (e, entries) {
                if (e != null) {
                    return cb(e);
                }
                var rv = [];
                for (var i = 0; i < entries.length; i++) {
                    rv.push(entries[i].name);
                }
                cb(null, rv);
            });
        };
        return HTML5FS;
    })(file_system.BaseFileSystem);
    exports.HTML5FS = HTML5FS;

    browserfs.registerFileSystem('HTML5FS', HTML5FS);
});
//# sourceMappingURL=html5fs.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/in_memory',["require", "exports", '../generic/key_value_filesystem', '../core/browserfs'], function(require, exports, kvfs, browserfs) {
    /**
    * A simple in-memory key-value store backed by a JavaScript object.
    */
    var InMemoryStore = (function () {
        function InMemoryStore() {
            this.store = {};
        }
        InMemoryStore.prototype.name = function () {
            return 'In-memory';
        };
        InMemoryStore.prototype.clear = function () {
            this.store = {};
        };

        InMemoryStore.prototype.beginTransaction = function (type) {
            return new kvfs.SimpleSyncRWTransaction(this);
        };

        InMemoryStore.prototype.get = function (key) {
            return this.store[key];
        };

        InMemoryStore.prototype.put = function (key, data, overwrite) {
            if (!overwrite && this.store.hasOwnProperty(key)) {
                return false;
            }
            this.store[key] = data;
            return true;
        };

        InMemoryStore.prototype.delete = function (key) {
            delete this.store[key];
        };
        return InMemoryStore;
    })();
    exports.InMemoryStore = InMemoryStore;

    /**
    * A simple in-memory file system backed by an InMemoryStore.
    */
    var InMemoryFileSystem = (function (_super) {
        __extends(InMemoryFileSystem, _super);
        function InMemoryFileSystem() {
            _super.call(this, { store: new InMemoryStore() });
        }
        return InMemoryFileSystem;
    })(kvfs.SyncKeyValueFileSystem);
    exports.InMemoryFileSystem = InMemoryFileSystem;

    browserfs.registerFileSystem('InMemory', InMemoryFileSystem);
});
//# sourceMappingURL=in_memory.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/localStorage',["require", "exports", '../core/buffer', '../core/browserfs', '../generic/key_value_filesystem', '../core/api_error', '../core/global'], function(require, exports, buffer, browserfs, kvfs, api_error, global) {
    var Buffer = buffer.Buffer, ApiError = api_error.ApiError, ErrorCode = api_error.ErrorCode;

    // Some versions of FF and all versions of IE do not support the full range of
    // 16-bit numbers encoded as characters, as they enforce UTF-16 restrictions.
    // http://stackoverflow.com/questions/11170716/are-there-any-characters-that-are-not-allowed-in-localstorage/11173673#11173673
    var supportsBinaryString = false, binaryEncoding;
    try  {
        global.localStorage.setItem("__test__", String.fromCharCode(0xD800));
        supportsBinaryString = global.localStorage.getItem("__test__") === String.fromCharCode(0xD800);
    } catch (e) {
        // IE throws an exception.
        supportsBinaryString = false;
    }
    binaryEncoding = supportsBinaryString ? 'binary_string' : 'binary_string_ie';

    /**
    * A synchronous key-value store backed by localStorage.
    */
    var LocalStorageStore = (function () {
        function LocalStorageStore() {
        }
        LocalStorageStore.prototype.name = function () {
            return 'LocalStorage';
        };

        LocalStorageStore.prototype.clear = function () {
            global.localStorage.clear();
        };

        LocalStorageStore.prototype.beginTransaction = function (type) {
            // No need to differentiate.
            return new kvfs.SimpleSyncRWTransaction(this);
        };

        LocalStorageStore.prototype.get = function (key) {
            try  {
                var data = global.localStorage.getItem(key);
                if (data !== null) {
                    return new Buffer(data, binaryEncoding);
                }
            } catch (e) {
            }

            // Key doesn't exist, or a failure occurred.
            return undefined;
        };

        LocalStorageStore.prototype.put = function (key, data, overwrite) {
            try  {
                if (!overwrite && global.localStorage.getItem(key) !== null) {
                    // Don't want to overwrite the key!
                    return false;
                }
                global.localStorage.setItem(key, data.toString(binaryEncoding));
                return true;
            } catch (e) {
                throw new ApiError(11 /* ENOSPC */, "LocalStorage is full.");
            }
        };

        LocalStorageStore.prototype.delete = function (key) {
            try  {
                global.localStorage.removeItem(key);
            } catch (e) {
                throw new ApiError(2 /* EIO */, "Unable to delete key " + key + ": " + e);
            }
        };
        return LocalStorageStore;
    })();
    exports.LocalStorageStore = LocalStorageStore;

    /**
    * A synchronous file system backed by localStorage. Connects our
    * LocalStorageStore to our SyncKeyValueFileSystem.
    */
    var LocalStorageFileSystem = (function (_super) {
        __extends(LocalStorageFileSystem, _super);
        function LocalStorageFileSystem() {
            _super.call(this, { store: new LocalStorageStore() });
        }
        LocalStorageFileSystem.isAvailable = function () {
            return typeof global.localStorage !== 'undefined';
        };
        return LocalStorageFileSystem;
    })(kvfs.SyncKeyValueFileSystem);
    exports.LocalStorageFileSystem = LocalStorageFileSystem;

    browserfs.registerFileSystem('LocalStorage', LocalStorageFileSystem);
});
//# sourceMappingURL=localStorage.js.map
;
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/mountable_file_system',["require", "exports", '../core/file_system', './in_memory', '../core/api_error', '../core/node_fs', '../core/browserfs'], function(require, exports, file_system, in_memory, api_error, node_fs, browserfs) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var fs = node_fs.fs;

    /**
    * The MountableFileSystem allows you to mount multiple backend types or
    * multiple instantiations of the same backend into a single file system tree.
    * The file systems do not need to know about each other; all interactions are
    * automatically facilitated through this interface.
    *
    * For example, if a file system is mounted at /mnt/blah, and a request came in
    * for /mnt/blah/foo.txt, the file system would see a request for /foo.txt.
    */
    var MountableFileSystem = (function (_super) {
        __extends(MountableFileSystem, _super);
        function MountableFileSystem() {
            _super.call(this);
            this.mntMap = {};

            // The InMemory file system serves purely to provide directory listings for
            // mounted file systems.
            this.rootFs = new in_memory.InMemoryFileSystem();
        }
        /**
        * Mounts the file system at the given mount point.
        */
        MountableFileSystem.prototype.mount = function (mnt_pt, fs) {
            if (this.mntMap[mnt_pt]) {
                throw new ApiError(9 /* EINVAL */, "Mount point " + mnt_pt + " is already taken.");
            }

            // @todo Ensure new mount path is not subsumed by active mount paths.
            this.rootFs.mkdirSync(mnt_pt, 0x1ff);
            this.mntMap[mnt_pt] = fs;
        };

        MountableFileSystem.prototype.umount = function (mnt_pt) {
            if (!this.mntMap[mnt_pt]) {
                throw new ApiError(9 /* EINVAL */, "Mount point " + mnt_pt + " is already unmounted.");
            }
            delete this.mntMap[mnt_pt];
            this.rootFs.rmdirSync(mnt_pt);
        };

        /**
        * Returns the file system that the path points to.
        */
        MountableFileSystem.prototype._get_fs = function (path) {
            for (var mnt_pt in this.mntMap) {
                var fs = this.mntMap[mnt_pt];
                if (path.indexOf(mnt_pt) === 0) {
                    path = path.substr(mnt_pt.length > 1 ? mnt_pt.length : 0);
                    if (path === '') {
                        path = '/';
                    }
                    return { fs: fs, path: path };
                }
            }

            // Query our root file system.
            return { fs: this.rootFs, path: path };
        };

        // Global information methods
        MountableFileSystem.prototype.getName = function () {
            return 'MountableFileSystem';
        };

        MountableFileSystem.isAvailable = function () {
            return true;
        };

        MountableFileSystem.prototype.diskSpace = function (path, cb) {
            cb(0, 0);
        };

        MountableFileSystem.prototype.isReadOnly = function () {
            return false;
        };

        MountableFileSystem.prototype.supportsLinks = function () {
            // I'm not ready for cross-FS links yet.
            return false;
        };

        MountableFileSystem.prototype.supportsProps = function () {
            return false;
        };

        MountableFileSystem.prototype.supportsSynch = function () {
            return true;
        };

        /**
        * Fixes up error messages so they mention the mounted file location relative
        * to the MFS root, not to the particular FS's root.
        * Mutates the input error, and returns it.
        */
        MountableFileSystem.prototype.standardizeError = function (err, path, realPath) {
            var index;
            if (-1 !== (index = err.message.indexOf(path))) {
                err.message = err.message.substr(0, index) + realPath + err.message.substr(index + path.length);
            }
            return err;
        };

        // The following methods involve multiple file systems, and thus have custom
        // logic.
        // Note that we go through the Node API to use its robust default argument
        // processing.
        MountableFileSystem.prototype.rename = function (oldPath, newPath, cb) {
            // Scenario 1: old and new are on same FS.
            var fs1_rv = this._get_fs(oldPath);
            var fs2_rv = this._get_fs(newPath);
            if (fs1_rv.fs === fs2_rv.fs) {
                var _this = this;
                return fs1_rv.fs.rename(fs1_rv.path, fs2_rv.path, function (e) {
                    if (e)
                        _this.standardizeError(_this.standardizeError(e, fs1_rv.path, oldPath), fs2_rv.path, newPath);
                    cb(e);
                });
            }

            // Scenario 2: Different file systems.
            // Read old file, write new file, delete old file.
            return fs.readFile(oldPath, function (err, data) {
                if (err) {
                    return cb(err);
                }
                fs.writeFile(newPath, data, function (err) {
                    if (err) {
                        return cb(err);
                    }
                    fs.unlink(oldPath, cb);
                });
            });
        };

        MountableFileSystem.prototype.renameSync = function (oldPath, newPath) {
            // Scenario 1: old and new are on same FS.
            var fs1_rv = this._get_fs(oldPath);
            var fs2_rv = this._get_fs(newPath);
            if (fs1_rv.fs === fs2_rv.fs) {
                try  {
                    return fs1_rv.fs.renameSync(fs1_rv.path, fs2_rv.path);
                } catch (e) {
                    this.standardizeError(this.standardizeError(e, fs1_rv.path, oldPath), fs2_rv.path, newPath);
                    throw e;
                }
            }

            // Scenario 2: Different file systems.
            var data = fs.readFileSync(oldPath);
            fs.writeFileSync(newPath, data);
            return fs.unlinkSync(oldPath);
        };
        return MountableFileSystem;
    })(file_system.BaseFileSystem);
    exports.MountableFileSystem = MountableFileSystem;

    /**
    * Tricky: Define all of the functions that merely forward arguments to the
    * relevant file system, or return/throw an error.
    * Take advantage of the fact that the *first* argument is always the path, and
    * the *last* is the callback function (if async).
    */
    function defineFcn(name, isSync, numArgs) {
        if (isSync) {
            return function () {
                var args = [];
                for (var _i = 0; _i < (arguments.length - 0); _i++) {
                    args[_i] = arguments[_i + 0];
                }
                var path = args[0];
                var rv = this._get_fs(path);
                args[0] = rv.path;
                try  {
                    return rv.fs[name].apply(rv.fs, args);
                } catch (e) {
                    this.standardizeError(e, rv.path, path);
                    throw e;
                }
            };
        } else {
            return function () {
                var args = [];
                for (var _i = 0; _i < (arguments.length - 0); _i++) {
                    args[_i] = arguments[_i + 0];
                }
                var path = args[0];
                var rv = this._get_fs(path);
                args[0] = rv.path;
                if (typeof args[args.length - 1] === 'function') {
                    var cb = args[args.length - 1];
                    var _this = this;
                    args[args.length - 1] = function () {
                        var args = [];
                        for (var _i = 0; _i < (arguments.length - 0); _i++) {
                            args[_i] = arguments[_i + 0];
                        }
                        if (args.length > 0 && args[0] instanceof api_error.ApiError) {
                            _this.standardizeError(args[0], rv.path, path);
                        }
                        cb.apply(null, args);
                    };
                }
                return rv.fs[name].apply(rv.fs, args);
            };
        }
    }

    var fsCmdMap = [
        ['readdir', 'exists', 'unlink', 'rmdir', 'readlink'],
        ['stat', 'mkdir', 'realpath', 'truncate'],
        ['open', 'readFile', 'chmod', 'utimes'],
        ['chown'],
        ['writeFile', 'appendFile']];

    for (var i = 0; i < fsCmdMap.length; i++) {
        var cmds = fsCmdMap[i];
        for (var j = 0; j < cmds.length; j++) {
            var fnName = cmds[j];
            MountableFileSystem.prototype[fnName] = defineFcn(fnName, false, i + 1);
            MountableFileSystem.prototype[fnName + 'Sync'] = defineFcn(fnName + 'Sync', true, i + 1);
        }
    }

    browserfs.registerFileSystem('MountableFileSystem', MountableFileSystem);
});
//# sourceMappingURL=mountable_file_system.js.map
;
/** @license zlib.js 2012 - imaya [ https://github.com/imaya/zlib.js ] The MIT License */(function() {var l=void 0,p=this;function q(c,d){var a=c.split("."),b=p;!(a[0]in b)&&b.execScript&&b.execScript("var "+a[0]);for(var e;a.length&&(e=a.shift());)!a.length&&d!==l?b[e]=d:b=b[e]?b[e]:b[e]={}};var r="undefined"!==typeof Uint8Array&&"undefined"!==typeof Uint16Array&&"undefined"!==typeof Uint32Array;function u(c){var d=c.length,a=0,b=Number.POSITIVE_INFINITY,e,f,g,h,k,m,s,n,t;for(n=0;n<d;++n)c[n]>a&&(a=c[n]),c[n]<b&&(b=c[n]);e=1<<a;f=new (r?Uint32Array:Array)(e);g=1;h=0;for(k=2;g<=a;){for(n=0;n<d;++n)if(c[n]===g){m=0;s=h;for(t=0;t<g;++t)m=m<<1|s&1,s>>=1;for(t=m;t<e;t+=k)f[t]=g<<16|n;++h}++g;h<<=1;k<<=1}return[f,a,b]};function v(c,d){this.g=[];this.h=32768;this.c=this.f=this.d=this.k=0;this.input=r?new Uint8Array(c):c;this.l=!1;this.i=w;this.p=!1;if(d||!(d={}))d.index&&(this.d=d.index),d.bufferSize&&(this.h=d.bufferSize),d.bufferType&&(this.i=d.bufferType),d.resize&&(this.p=d.resize);switch(this.i){case x:this.a=32768;this.b=new (r?Uint8Array:Array)(32768+this.h+258);break;case w:this.a=0;this.b=new (r?Uint8Array:Array)(this.h);this.e=this.u;this.m=this.r;this.j=this.s;break;default:throw Error("invalid inflate mode");
}}var x=0,w=1;
v.prototype.t=function(){for(;!this.l;){var c=y(this,3);c&1&&(this.l=!0);c>>>=1;switch(c){case 0:var d=this.input,a=this.d,b=this.b,e=this.a,f=l,g=l,h=l,k=b.length,m=l;this.c=this.f=0;f=d[a++];if(f===l)throw Error("invalid uncompressed block header: LEN (first byte)");g=f;f=d[a++];if(f===l)throw Error("invalid uncompressed block header: LEN (second byte)");g|=f<<8;f=d[a++];if(f===l)throw Error("invalid uncompressed block header: NLEN (first byte)");h=f;f=d[a++];if(f===l)throw Error("invalid uncompressed block header: NLEN (second byte)");h|=
f<<8;if(g===~h)throw Error("invalid uncompressed block header: length verify");if(a+g>d.length)throw Error("input buffer is broken");switch(this.i){case x:for(;e+g>b.length;){m=k-e;g-=m;if(r)b.set(d.subarray(a,a+m),e),e+=m,a+=m;else for(;m--;)b[e++]=d[a++];this.a=e;b=this.e();e=this.a}break;case w:for(;e+g>b.length;)b=this.e({o:2});break;default:throw Error("invalid inflate mode");}if(r)b.set(d.subarray(a,a+g),e),e+=g,a+=g;else for(;g--;)b[e++]=d[a++];this.d=a;this.a=e;this.b=b;break;case 1:this.j(z,
A);break;case 2:B(this);break;default:throw Error("unknown BTYPE: "+c);}}return this.m()};
var C=[16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15],D=r?new Uint16Array(C):C,E=[3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,258,258],F=r?new Uint16Array(E):E,G=[0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0],H=r?new Uint8Array(G):G,I=[1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577],J=r?new Uint16Array(I):I,K=[0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,
13],L=r?new Uint8Array(K):K,M=new (r?Uint8Array:Array)(288),N,O;N=0;for(O=M.length;N<O;++N)M[N]=143>=N?8:255>=N?9:279>=N?7:8;var z=u(M),P=new (r?Uint8Array:Array)(30),Q,R;Q=0;for(R=P.length;Q<R;++Q)P[Q]=5;var A=u(P);function y(c,d){for(var a=c.f,b=c.c,e=c.input,f=c.d,g;b<d;){g=e[f++];if(g===l)throw Error("input buffer is broken");a|=g<<b;b+=8}g=a&(1<<d)-1;c.f=a>>>d;c.c=b-d;c.d=f;return g}
function S(c,d){for(var a=c.f,b=c.c,e=c.input,f=c.d,g=d[0],h=d[1],k,m,s;b<h;){k=e[f++];if(k===l)break;a|=k<<b;b+=8}m=g[a&(1<<h)-1];s=m>>>16;c.f=a>>s;c.c=b-s;c.d=f;return m&65535}
function B(c){function d(a,c,b){var d,f,e,g;for(g=0;g<a;)switch(d=S(this,c),d){case 16:for(e=3+y(this,2);e--;)b[g++]=f;break;case 17:for(e=3+y(this,3);e--;)b[g++]=0;f=0;break;case 18:for(e=11+y(this,7);e--;)b[g++]=0;f=0;break;default:f=b[g++]=d}return b}var a=y(c,5)+257,b=y(c,5)+1,e=y(c,4)+4,f=new (r?Uint8Array:Array)(D.length),g,h,k,m;for(m=0;m<e;++m)f[D[m]]=y(c,3);g=u(f);h=new (r?Uint8Array:Array)(a);k=new (r?Uint8Array:Array)(b);c.j(u(d.call(c,a,g,h)),u(d.call(c,b,g,k)))}
v.prototype.j=function(c,d){var a=this.b,b=this.a;this.n=c;for(var e=a.length-258,f,g,h,k;256!==(f=S(this,c));)if(256>f)b>=e&&(this.a=b,a=this.e(),b=this.a),a[b++]=f;else{g=f-257;k=F[g];0<H[g]&&(k+=y(this,H[g]));f=S(this,d);h=J[f];0<L[f]&&(h+=y(this,L[f]));b>=e&&(this.a=b,a=this.e(),b=this.a);for(;k--;)a[b]=a[b++-h]}for(;8<=this.c;)this.c-=8,this.d--;this.a=b};
v.prototype.s=function(c,d){var a=this.b,b=this.a;this.n=c;for(var e=a.length,f,g,h,k;256!==(f=S(this,c));)if(256>f)b>=e&&(a=this.e(),e=a.length),a[b++]=f;else{g=f-257;k=F[g];0<H[g]&&(k+=y(this,H[g]));f=S(this,d);h=J[f];0<L[f]&&(h+=y(this,L[f]));b+k>e&&(a=this.e(),e=a.length);for(;k--;)a[b]=a[b++-h]}for(;8<=this.c;)this.c-=8,this.d--;this.a=b};
v.prototype.e=function(){var c=new (r?Uint8Array:Array)(this.a-32768),d=this.a-32768,a,b,e=this.b;if(r)c.set(e.subarray(32768,c.length));else{a=0;for(b=c.length;a<b;++a)c[a]=e[a+32768]}this.g.push(c);this.k+=c.length;if(r)e.set(e.subarray(d,d+32768));else for(a=0;32768>a;++a)e[a]=e[d+a];this.a=32768;return e};
v.prototype.u=function(c){var d,a=this.input.length/this.d+1|0,b,e,f,g=this.input,h=this.b;c&&("number"===typeof c.o&&(a=c.o),"number"===typeof c.q&&(a+=c.q));2>a?(b=(g.length-this.d)/this.n[2],f=258*(b/2)|0,e=f<h.length?h.length+f:h.length<<1):e=h.length*a;r?(d=new Uint8Array(e),d.set(h)):d=h;return this.b=d};
v.prototype.m=function(){var c=0,d=this.b,a=this.g,b,e=new (r?Uint8Array:Array)(this.k+(this.a-32768)),f,g,h,k;if(0===a.length)return r?this.b.subarray(32768,this.a):this.b.slice(32768,this.a);f=0;for(g=a.length;f<g;++f){b=a[f];h=0;for(k=b.length;h<k;++h)e[c++]=b[h]}f=32768;for(g=this.a;f<g;++f)e[c++]=d[f];this.g=[];return this.buffer=e};
v.prototype.r=function(){var c,d=this.a;r?this.p?(c=new Uint8Array(d),c.set(this.b.subarray(0,d))):c=this.b.subarray(0,d):(this.b.length>d&&(this.b.length=d),c=this.b);return this.buffer=c};q("Zlib.RawInflate",v);q("Zlib.RawInflate.prototype.decompress",v.prototype.t);var T={ADAPTIVE:w,BLOCK:x},U,V,W,X;if(Object.keys)U=Object.keys(T);else for(V in U=[],W=0,T)U[W++]=V;W=0;for(X=U.length;W<X;++W)V=U[W],q("Zlib.RawInflate.BufferType."+V,T[V]);}).call(this); //@ sourceMappingURL=rawinflate.min.js.map
;
define("zlib", (function (global) {
    return function () {
        var ret, fn;
        return ret || global.Zlib.RawInflate;
    };
}(this)));

/// <amd-dependency path="zlib" />
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
define('backend/zipfs',["require", "exports", '../core/buffer', '../core/api_error', '../generic/file_index', '../core/browserfs', '../core/node_fs_stats', '../core/file_system', '../core/file_flag', '../core/buffer_core_arraybuffer', '../generic/preload_file', "zlib"], function(require, exports, buffer, api_error, file_index, browserfs, node_fs_stats, file_system, file_flag, buffer_core_arraybuffer, preload_file) {
    var ApiError = api_error.ApiError;
    var ErrorCode = api_error.ErrorCode;
    var ActionType = file_flag.ActionType;

    

    var RawInflate = Zlib.RawInflate;

    /**
    * 4.4.2.2: Indicates the compatibiltiy of a file's external attributes.
    */
    (function (ExternalFileAttributeType) {
        ExternalFileAttributeType[ExternalFileAttributeType["MSDOS"] = 0] = "MSDOS";
        ExternalFileAttributeType[ExternalFileAttributeType["AMIGA"] = 1] = "AMIGA";
        ExternalFileAttributeType[ExternalFileAttributeType["OPENVMS"] = 2] = "OPENVMS";
        ExternalFileAttributeType[ExternalFileAttributeType["UNIX"] = 3] = "UNIX";
        ExternalFileAttributeType[ExternalFileAttributeType["VM_CMS"] = 4] = "VM_CMS";
        ExternalFileAttributeType[ExternalFileAttributeType["ATARI_ST"] = 5] = "ATARI_ST";
        ExternalFileAttributeType[ExternalFileAttributeType["OS2_HPFS"] = 6] = "OS2_HPFS";
        ExternalFileAttributeType[ExternalFileAttributeType["MAC"] = 7] = "MAC";
        ExternalFileAttributeType[ExternalFileAttributeType["Z_SYSTEM"] = 8] = "Z_SYSTEM";
        ExternalFileAttributeType[ExternalFileAttributeType["CP_M"] = 9] = "CP_M";
        ExternalFileAttributeType[ExternalFileAttributeType["NTFS"] = 10] = "NTFS";
        ExternalFileAttributeType[ExternalFileAttributeType["MVS"] = 11] = "MVS";
        ExternalFileAttributeType[ExternalFileAttributeType["VSE"] = 12] = "VSE";
        ExternalFileAttributeType[ExternalFileAttributeType["ACORN_RISC"] = 13] = "ACORN_RISC";
        ExternalFileAttributeType[ExternalFileAttributeType["VFAT"] = 14] = "VFAT";
        ExternalFileAttributeType[ExternalFileAttributeType["ALT_MVS"] = 15] = "ALT_MVS";
        ExternalFileAttributeType[ExternalFileAttributeType["BEOS"] = 16] = "BEOS";
        ExternalFileAttributeType[ExternalFileAttributeType["TANDEM"] = 17] = "TANDEM";
        ExternalFileAttributeType[ExternalFileAttributeType["OS_400"] = 18] = "OS_400";
        ExternalFileAttributeType[ExternalFileAttributeType["OSX"] = 19] = "OSX";
    })(exports.ExternalFileAttributeType || (exports.ExternalFileAttributeType = {}));
    var ExternalFileAttributeType = exports.ExternalFileAttributeType;

    /**
    * 4.4.5
    */
    (function (CompressionMethod) {
        CompressionMethod[CompressionMethod["STORED"] = 0] = "STORED";
        CompressionMethod[CompressionMethod["SHRUNK"] = 1] = "SHRUNK";
        CompressionMethod[CompressionMethod["REDUCED_1"] = 2] = "REDUCED_1";
        CompressionMethod[CompressionMethod["REDUCED_2"] = 3] = "REDUCED_2";
        CompressionMethod[CompressionMethod["REDUCED_3"] = 4] = "REDUCED_3";
        CompressionMethod[CompressionMethod["REDUCED_4"] = 5] = "REDUCED_4";
        CompressionMethod[CompressionMethod["IMPLODE"] = 6] = "IMPLODE";
        CompressionMethod[CompressionMethod["DEFLATE"] = 8] = "DEFLATE";
        CompressionMethod[CompressionMethod["DEFLATE64"] = 9] = "DEFLATE64";
        CompressionMethod[CompressionMethod["TERSE_OLD"] = 10] = "TERSE_OLD";
        CompressionMethod[CompressionMethod["BZIP2"] = 12] = "BZIP2";
        CompressionMethod[CompressionMethod["LZMA"] = 14] = "LZMA";
        CompressionMethod[CompressionMethod["TERSE_NEW"] = 18] = "TERSE_NEW";
        CompressionMethod[CompressionMethod["LZ77"] = 19] = "LZ77";
        CompressionMethod[CompressionMethod["WAVPACK"] = 97] = "WAVPACK";
        CompressionMethod[CompressionMethod["PPMD"] = 98] = "PPMD";
    })(exports.CompressionMethod || (exports.CompressionMethod = {}));
    var CompressionMethod = exports.CompressionMethod;

    /**
    * Converts the input time and date in MS-DOS format into a JavaScript Date
    * object.
    */
    function msdos2date(time, date) {
        // MS-DOS Date
        //|0 0 0 0  0|0 0 0  0|0 0 0  0 0 0 0
        //  D (1-31)  M (1-23)  Y (from 1980)
        var day = date & 0x1F;

        // JS date is 0-indexed, DOS is 1-indexed.
        var month = ((date >> 5) & 0xF) - 1;
        var year = (date >> 9) + 1980;

        // MS DOS Time
        //|0 0 0 0  0|0 0 0  0 0 0|0  0 0 0 0
        //   Second      Minute       Hour
        var second = time & 0x1F;
        var minute = (time >> 5) & 0x3F;
        var hour = time >> 11;
        return new Date(year, month, day, hour, minute, second);
    }

    /**
    * Safely returns the string from the buffer, even if it is 0 bytes long.
    * (Normally, calling toString() on a buffer with start === end causes an
    * exception).
    */
    function safeToString(buff, useUTF8, start, length) {
        return length === 0 ? "" : buff.toString(useUTF8 ? 'utf8' : 'extended_ascii', start, start + length);
    }

    /*
    4.3.6 Overall .ZIP file format:
    
    [local file header 1]
    [encryption header 1]
    [file data 1]
    [data descriptor 1]
    .
    .
    .
    [local file header n]
    [encryption header n]
    [file data n]
    [data descriptor n]
    [archive decryption header]
    [archive extra data record]
    [central directory header 1]
    .
    .
    .
    [central directory header n]
    [zip64 end of central directory record]
    [zip64 end of central directory locator]
    [end of central directory record]
    */
    /*
    4.3.7  Local file header:
    
    local file header signature     4 bytes  (0x04034b50)
    version needed to extract       2 bytes
    general purpose bit flag        2 bytes
    compression method              2 bytes
    last mod file time              2 bytes
    last mod file date              2 bytes
    crc-32                          4 bytes
    compressed size                 4 bytes
    uncompressed size               4 bytes
    file name length                2 bytes
    extra field length              2 bytes
    
    file name (variable size)
    extra field (variable size)
    */
    var FileHeader = (function () {
        function FileHeader(data) {
            this.data = data;
            if (data.readUInt32LE(0) !== 0x04034b50) {
                throw new ApiError(9 /* EINVAL */, "Invalid Zip file: Local file header has invalid signature: " + this.data.readUInt32LE(0));
            }
        }
        FileHeader.prototype.versionNeeded = function () {
            return this.data.readUInt16LE(4);
        };
        FileHeader.prototype.flags = function () {
            return this.data.readUInt16LE(6);
        };
        FileHeader.prototype.compressionMethod = function () {
            return this.data.readUInt16LE(8);
        };
        FileHeader.prototype.lastModFileTime = function () {
            // Time and date is in MS-DOS format.
            return msdos2date(this.data.readUInt16LE(10), this.data.readUInt16LE(12));
        };
        FileHeader.prototype.crc32 = function () {
            return this.data.readUInt32LE(14);
        };

        /**
        * These two values are COMPLETELY USELESS.
        *
        * Section 4.4.9:
        *   If bit 3 of the general purpose bit flag is set,
        *   these fields are set to zero in the local header and the
        *   correct values are put in the data descriptor and
        *   in the central directory.
        *
        * So we'll just use the central directory's values.
        */
        // public compressedSize(): number { return this.data.readUInt32LE(18); }
        // public uncompressedSize(): number { return this.data.readUInt32LE(22); }
        FileHeader.prototype.fileNameLength = function () {
            return this.data.readUInt16LE(26);
        };
        FileHeader.prototype.extraFieldLength = function () {
            return this.data.readUInt16LE(28);
        };
        FileHeader.prototype.fileName = function () {
            return safeToString(this.data, this.useUTF8(), 30, this.fileNameLength());
        };
        FileHeader.prototype.extraField = function () {
            var start = 30 + this.fileNameLength();
            return this.data.slice(start, start + this.extraFieldLength());
        };
        FileHeader.prototype.totalSize = function () {
            return 30 + this.fileNameLength() + this.extraFieldLength();
        };
        FileHeader.prototype.useUTF8 = function () {
            return (this.flags() & 0x800) === 0x800;
        };
        return FileHeader;
    })();
    exports.FileHeader = FileHeader;

    /**
    4.3.8  File data
    
    Immediately following the local header for a file
    SHOULD be placed the compressed or stored data for the file.
    If the file is encrypted, the encryption header for the file
    SHOULD be placed after the local header and before the file
    data. The series of [local file header][encryption header]
    [file data][data descriptor] repeats for each file in the
    .ZIP archive.
    
    Zero-byte files, directories, and other file types that
    contain no content MUST not include file data.
    */
    var FileData = (function () {
        function FileData(header, record, data) {
            this.header = header;
            this.record = record;
            this.data = data;
        }
        FileData.prototype.decompress = function () {
            var buff = this.data;

            // Check the compression
            var compressionMethod = this.header.compressionMethod();
            switch (compressionMethod) {
                case 8 /* DEFLATE */:
                    // Convert to Uint8Array or an array of bytes for the library.
                    if (buff.getBufferCore() instanceof buffer_core_arraybuffer.BufferCoreArrayBuffer) {
                        // Grab a slice of the zip file that contains the compressed data
                        // (avoids copying).
                        // XXX: Does RawInflate mutate the buffer? I hope not.
                        var bcore = buff.getBufferCore();
                        var dview = bcore.getDataView();
                        var start = dview.byteOffset + buff.getOffset();
                        var uarray = (new Uint8Array(dview.buffer)).subarray(start, start + this.record.compressedSize());
                        var data = (new RawInflate(uarray)).decompress();
                        return new buffer.Buffer(new buffer_core_arraybuffer.BufferCoreArrayBuffer(data.buffer), data.byteOffset, data.byteOffset + data.length);
                    } else {
                        // Convert to an array of bytes and decompress, then write into a new
                        // buffer :(
                        var newBuff = buff.slice(0, this.record.compressedSize());
                        return new buffer.Buffer((new RawInflate(newBuff.toJSON().data)).decompress());
                    }
                case 0 /* STORED */:
                    // Grab and copy.
                    return buff.sliceCopy(0, this.record.uncompressedSize());
                default:
                    var name = CompressionMethod[compressionMethod];
                    name = name ? name : "Unknown: " + compressionMethod;
                    throw new ApiError(9 /* EINVAL */, "Invalid compression method on file '" + this.header.fileName() + "': " + name);
            }
        };
        return FileData;
    })();
    exports.FileData = FileData;

    /*
    4.3.9  Data descriptor:
    
    crc-32                          4 bytes
    compressed size                 4 bytes
    uncompressed size               4 bytes
    */
    var DataDescriptor = (function () {
        function DataDescriptor(data) {
            this.data = data;
        }
        DataDescriptor.prototype.crc32 = function () {
            return this.data.readUInt32LE(0);
        };
        DataDescriptor.prototype.compressedSize = function () {
            return this.data.readUInt32LE(4);
        };
        DataDescriptor.prototype.uncompressedSize = function () {
            return this.data.readUInt32LE(8);
        };
        return DataDescriptor;
    })();
    exports.DataDescriptor = DataDescriptor;

    /*
    ` 4.3.10  Archive decryption header:
    
    4.3.10.1 The Archive Decryption Header is introduced in version 6.2
    of the ZIP format specification.  This record exists in support
    of the Central Directory Encryption Feature implemented as part of
    the Strong Encryption Specification as described in this document.
    When the Central Directory Structure is encrypted, this decryption
    header MUST precede the encrypted data segment.
    */
    /*
    4.3.11  Archive extra data record:
    
    archive extra data signature    4 bytes  (0x08064b50)
    extra field length              4 bytes
    extra field data                (variable size)
    
    4.3.11.1 The Archive Extra Data Record is introduced in version 6.2
    of the ZIP format specification.  This record MAY be used in support
    of the Central Directory Encryption Feature implemented as part of
    the Strong Encryption Specification as described in this document.
    When present, this record MUST immediately precede the central
    directory data structure.
    */
    var ArchiveExtraDataRecord = (function () {
        function ArchiveExtraDataRecord(data) {
            this.data = data;
            if (this.data.readUInt32LE(0) !== 0x08064b50) {
                throw new ApiError(9 /* EINVAL */, "Invalid archive extra data record signature: " + this.data.readUInt32LE(0));
            }
        }
        ArchiveExtraDataRecord.prototype.length = function () {
            return this.data.readUInt32LE(4);
        };
        ArchiveExtraDataRecord.prototype.extraFieldData = function () {
            return this.data.slice(8, 8 + this.length());
        };
        return ArchiveExtraDataRecord;
    })();
    exports.ArchiveExtraDataRecord = ArchiveExtraDataRecord;

    /*
    4.3.13 Digital signature:
    
    header signature                4 bytes  (0x05054b50)
    size of data                    2 bytes
    signature data (variable size)
    
    With the introduction of the Central Directory Encryption
    feature in version 6.2 of this specification, the Central
    Directory Structure MAY be stored both compressed and encrypted.
    Although not required, it is assumed when encrypting the
    Central Directory Structure, that it will be compressed
    for greater storage efficiency.  Information on the
    Central Directory Encryption feature can be found in the section
    describing the Strong Encryption Specification. The Digital
    Signature record will be neither compressed nor encrypted.
    */
    var DigitalSignature = (function () {
        function DigitalSignature(data) {
            this.data = data;
            if (this.data.readUInt32LE(0) !== 0x05054b50) {
                throw new ApiError(9 /* EINVAL */, "Invalid digital signature signature: " + this.data.readUInt32LE(0));
            }
        }
        DigitalSignature.prototype.size = function () {
            return this.data.readUInt16LE(4);
        };
        DigitalSignature.prototype.signatureData = function () {
            return this.data.slice(6, 6 + this.size());
        };
        return DigitalSignature;
    })();
    exports.DigitalSignature = DigitalSignature;

    /*
    4.3.12  Central directory structure:
    
    central file header signature   4 bytes  (0x02014b50)
    version made by                 2 bytes
    version needed to extract       2 bytes
    general purpose bit flag        2 bytes
    compression method              2 bytes
    last mod file time              2 bytes
    last mod file date              2 bytes
    crc-32                          4 bytes
    compressed size                 4 bytes
    uncompressed size               4 bytes
    file name length                2 bytes
    extra field length              2 bytes
    file comment length             2 bytes
    disk number start               2 bytes
    internal file attributes        2 bytes
    external file attributes        4 bytes
    relative offset of local header 4 bytes
    
    file name (variable size)
    extra field (variable size)
    file comment (variable size)
    */
    var CentralDirectory = (function () {
        function CentralDirectory(zipData, data) {
            this.zipData = zipData;
            this.data = data;
            // Sanity check.
            if (this.data.readUInt32LE(0) !== 0x02014b50)
                throw new ApiError(9 /* EINVAL */, "Invalid Zip file: Central directory record has invalid signature: " + this.data.readUInt32LE(0));
        }
        CentralDirectory.prototype.versionMadeBy = function () {
            return this.data.readUInt16LE(4);
        };
        CentralDirectory.prototype.versionNeeded = function () {
            return this.data.readUInt16LE(6);
        };
        CentralDirectory.prototype.flag = function () {
            return this.data.readUInt16LE(8);
        };
        CentralDirectory.prototype.compressionMethod = function () {
            return this.data.readUInt16LE(10);
        };
        CentralDirectory.prototype.lastModFileTime = function () {
            // Time and date is in MS-DOS format.
            return msdos2date(this.data.readUInt16LE(12), this.data.readUInt16LE(14));
        };
        CentralDirectory.prototype.crc32 = function () {
            return this.data.readUInt32LE(16);
        };
        CentralDirectory.prototype.compressedSize = function () {
            return this.data.readUInt32LE(20);
        };
        CentralDirectory.prototype.uncompressedSize = function () {
            return this.data.readUInt32LE(24);
        };
        CentralDirectory.prototype.fileNameLength = function () {
            return this.data.readUInt16LE(28);
        };
        CentralDirectory.prototype.extraFieldLength = function () {
            return this.data.readUInt16LE(30);
        };
        CentralDirectory.prototype.fileCommentLength = function () {
            return this.data.readUInt16LE(32);
        };
        CentralDirectory.prototype.diskNumberStart = function () {
            return this.data.readUInt16LE(34);
        };
        CentralDirectory.prototype.internalAttributes = function () {
            return this.data.readUInt16LE(36);
        };
        CentralDirectory.prototype.externalAttributes = function () {
            return this.data.readUInt32LE(38);
        };
        CentralDirectory.prototype.headerRelativeOffset = function () {
            return this.data.readUInt32LE(42);
        };
        CentralDirectory.prototype.fileName = function () {
            /*
            4.4.17.1 claims:
            * All slashes are forward ('/') slashes.
            * Filename doesn't begin with a slash.
            * No drive letters or any nonsense like that.
            * If filename is missing, the input came from standard input.
            
            Unfortunately, this isn't true in practice. Some Windows zip utilities use
            a backslash here, but the correct Unix-style path in file headers.
            
            To avoid seeking all over the file to recover the known-good filenames
            from file headers, we simply convert '/' to '\' here.
            */
            var fileName = safeToString(this.data, this.useUTF8(), 46, this.fileNameLength());
            return fileName.replace(/\\/g, "/");
        };
        CentralDirectory.prototype.extraField = function () {
            var start = 44 + this.fileNameLength();
            return this.data.slice(start, start + this.extraFieldLength());
        };
        CentralDirectory.prototype.fileComment = function () {
            var start = 46 + this.fileNameLength() + this.extraFieldLength();
            return safeToString(this.data, this.useUTF8(), start, this.fileCommentLength());
        };
        CentralDirectory.prototype.totalSize = function () {
            return 46 + this.fileNameLength() + this.extraFieldLength() + this.fileCommentLength();
        };
        CentralDirectory.prototype.isDirectory = function () {
            // NOTE: This assumes that the zip file implementation uses the lower byte
            //       of external attributes for DOS attributes for
            //       backwards-compatibility. This is not mandated, but appears to be
            //       commonplace.
            //       According to the spec, the layout of external attributes is
            //       platform-dependent.
            //       If that fails, we also check if the name of the file ends in '/',
            //       which is what Java's ZipFile implementation does.
            var fileName = this.fileName();
            return (this.externalAttributes() & 0x10 ? true : false) || (fileName.charAt(fileName.length - 1) === '/');
        };
        CentralDirectory.prototype.isFile = function () {
            return !this.isDirectory();
        };
        CentralDirectory.prototype.useUTF8 = function () {
            return (this.flag() & 0x800) === 0x800;
        };
        CentralDirectory.prototype.isEncrypted = function () {
            return (this.flag() & 0x1) === 0x1;
        };
        CentralDirectory.prototype.getData = function () {
            // Need to grab the header before we can figure out where the actual
            // compressed data starts.
            var start = this.headerRelativeOffset();
            var header = new FileHeader(this.zipData.slice(start));
            var filedata = new FileData(header, this, this.zipData.slice(start + header.totalSize()));
            return filedata.decompress();
        };
        CentralDirectory.prototype.getStats = function () {
            return new node_fs_stats.Stats(32768 /* FILE */, this.uncompressedSize(), 0x16D, new Date(), this.lastModFileTime());
        };
        return CentralDirectory;
    })();
    exports.CentralDirectory = CentralDirectory;

    /*
    4.3.16: end of central directory record
    end of central dir signature    4 bytes  (0x06054b50)
    number of this disk             2 bytes
    number of the disk with the
    start of the central directory  2 bytes
    total number of entries in the
    central directory on this disk  2 bytes
    total number of entries in
    the central directory           2 bytes
    size of the central directory   4 bytes
    offset of start of central
    directory with respect to
    the starting disk number        4 bytes
    .ZIP file comment length        2 bytes
    .ZIP file comment       (variable size)
    */
    var EndOfCentralDirectory = (function () {
        function EndOfCentralDirectory(data) {
            this.data = data;
            if (this.data.readUInt32LE(0) !== 0x06054b50)
                throw new ApiError(9 /* EINVAL */, "Invalid Zip file: End of central directory record has invalid signature: " + this.data.readUInt32LE(0));
        }
        EndOfCentralDirectory.prototype.diskNumber = function () {
            return this.data.readUInt16LE(4);
        };
        EndOfCentralDirectory.prototype.cdDiskNumber = function () {
            return this.data.readUInt16LE(6);
        };
        EndOfCentralDirectory.prototype.cdDiskEntryCount = function () {
            return this.data.readUInt16LE(8);
        };
        EndOfCentralDirectory.prototype.cdTotalEntryCount = function () {
            return this.data.readUInt16LE(10);
        };
        EndOfCentralDirectory.prototype.cdSize = function () {
            return this.data.readUInt32LE(12);
        };
        EndOfCentralDirectory.prototype.cdOffset = function () {
            return this.data.readUInt32LE(16);
        };
        EndOfCentralDirectory.prototype.cdZipComment = function () {
            // Assuming UTF-8. The specification doesn't specify.
            return safeToString(this.data, true, 22, this.data.readUInt16LE(20));
        };
        return EndOfCentralDirectory;
    })();
    exports.EndOfCentralDirectory = EndOfCentralDirectory;

    var ZipFS = (function (_super) {
        __extends(ZipFS, _super);
        /**
        * Constructs a ZipFS from the given zip file data. Name is optional, and is
        * used primarily for our unit tests' purposes to differentiate different
        * test zip files in test output.
        */
        function ZipFS(data, name) {
            if (typeof name === "undefined") { name = ''; }
            _super.call(this);
            this.data = data;
            this.name = name;
            this._index = new file_index.FileIndex();
            this.populateIndex();
        }
        ZipFS.prototype.getName = function () {
            return 'ZipFS' + (this.name !== '' ? ' ' + this.name : '');
        };

        ZipFS.isAvailable = function () {
            return true;
        };

        ZipFS.prototype.diskSpace = function (path, cb) {
            // Read-only file system.
            cb(this.data.length, 0);
        };

        ZipFS.prototype.isReadOnly = function () {
            return true;
        };

        ZipFS.prototype.supportsLinks = function () {
            return false;
        };

        ZipFS.prototype.supportsProps = function () {
            return false;
        };

        ZipFS.prototype.supportsSynch = function () {
            return true;
        };

        ZipFS.prototype.statSync = function (path, isLstat) {
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw new ApiError(1 /* ENOENT */, "" + path + " not found.");
            }
            var stats;
            if (inode.isFile()) {
                stats = inode.getData().getStats();
            } else {
                stats = inode.getStats();
            }
            return stats;
        };

        ZipFS.prototype.openSync = function (path, flags, mode) {
            // INVARIANT: Cannot write to RO file systems.
            if (flags.isWriteable()) {
                throw new ApiError(0 /* EPERM */, path);
            }

            // Check if the path exists, and is a file.
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw new ApiError(1 /* ENOENT */, "" + path + " is not in the FileIndex.");
            }
            if (inode.isDir()) {
                throw new ApiError(8 /* EISDIR */, "" + path + " is a directory.");
            }
            var cdRecord = inode.getData();
            var stats = cdRecord.getStats();
            switch (flags.pathExistsAction()) {
                case 1 /* THROW_EXCEPTION */:
                case 2 /* TRUNCATE_FILE */:
                    throw new ApiError(6 /* EEXIST */, "" + path + " already exists.");
                case 0 /* NOP */:
                    return new preload_file.NoSyncFile(this, path, flags, stats, cdRecord.getData());
                default:
                    throw new ApiError(9 /* EINVAL */, 'Invalid FileMode object.');
            }
            return null;
        };

        ZipFS.prototype.readdirSync = function (path) {
            // Check if it exists.
            var inode = this._index.getInode(path);
            if (inode === null) {
                throw new ApiError(1 /* ENOENT */, "" + path + " not found.");
            } else if (inode.isFile()) {
                throw new ApiError(7 /* ENOTDIR */, "" + path + " is a file, not a directory.");
            }
            return inode.getListing();
        };

        /**
        * Specially-optimized readfile.
        */
        ZipFS.prototype.readFileSync = function (fname, encoding, flag) {
            // Get file.
            var fd = this.openSync(fname, flag, 0x1a4);
            try  {
                var fdCast = fd;
                var fdBuff = fdCast._buffer;
                if (encoding === null) {
                    if (fdBuff.length > 0) {
                        return fdBuff.sliceCopy();
                    } else {
                        return new buffer.Buffer(0);
                    }
                }
                return fdBuff.toString(encoding);
            } finally {
                fd.closeSync();
            }
        };

        /**
        * Locates the end of central directory record at the end of the file.
        * Throws an exception if it cannot be found.
        */
        ZipFS.prototype.getEOCD = function () {
            // Unfortunately, the comment is variable size and up to 64K in size.
            // We assume that the magic signature does not appear in the comment, and
            // in the bytes between the comment and the signature. Other ZIP
            // implementations make this same assumption, since the alternative is to
            // read thread every entry in the file to get to it. :(
            // These are *negative* offsets from the end of the file.
            var startOffset = 22;
            var endOffset = Math.min(startOffset + 0xFFFF, this.data.length - 1);

            for (var i = startOffset; i < endOffset; i++) {
                // Magic number: EOCD Signature
                if (this.data.readUInt32LE(this.data.length - i) === 0x06054b50) {
                    return new EndOfCentralDirectory(this.data.slice(this.data.length - i));
                }
            }
            throw new ApiError(9 /* EINVAL */, "Invalid ZIP file: Could not locate End of Central Directory signature.");
        };

        ZipFS.prototype.populateIndex = function () {
            var eocd = this.getEOCD();
            if (eocd.diskNumber() !== eocd.cdDiskNumber())
                throw new ApiError(9 /* EINVAL */, "ZipFS does not support spanned zip files.");

            var cdPtr = eocd.cdOffset();
            if (cdPtr === 0xFFFFFFFF)
                throw new ApiError(9 /* EINVAL */, "ZipFS does not support Zip64.");
            var cdEnd = cdPtr + eocd.cdSize();
            while (cdPtr < cdEnd) {
                var cd = new CentralDirectory(this.data, this.data.slice(cdPtr));
                cdPtr += cd.totalSize();

                // Paths must be absolute, yet zip file paths are always relative to the
                // zip root. So we append '/' and call it a day.
                var filename = cd.fileName();
                if (filename.charAt(0) === '/')
                    throw new Error("WHY IS THIS ABSOLUTE");

                // XXX: For the file index, strip the trailing '/'.
                if (filename.charAt(filename.length - 1) === '/') {
                    filename = filename.substr(0, filename.length - 1);
                }
                if (cd.isDirectory()) {
                    this._index.addPath('/' + filename, new file_index.DirInode());
                } else {
                    this._index.addPath('/' + filename, new file_index.FileInode(cd));
                }
            }
        };
        return ZipFS;
    })(file_system.SynchronousFileSystem);
    exports.ZipFS = ZipFS;

    browserfs.registerFileSystem('ZipFS', ZipFS);
});
//# sourceMappingURL=zipfs.js.map
;require('core/global').BrowserFS=require('core/browserfs');require('generic/emscripten_fs');require('backend/IndexedDB');require('backend/XmlHttpRequest');require('backend/dropbox');require('backend/html5fs');require('backend/in_memory');require('backend/localStorage');require('backend/mountable_file_system');require('backend/zipfs');})();
//# sourceMappingURL=browserfs.js.map