let root, BFS, BFSDB;
let bundleCounter = 0;
let loadedScripts = [];
const FS = {};
const helper = {};

// this is huge and takes between 2 and 3 minutes to unzip. (10 minutes for firefox?)
// luckily it only needs to be done once.
const bundlePath = ["assets/frontend/bundle.zip.aa",
                    "assets/frontend/bundle.zip.ab",
                    "assets/frontend/bundle.zip.ac",
                    "assets/frontend/bundle.zip.ad",
                    "assets/frontend/bundle.zip.ae"];
// ["assets/frontend/bundle-minimal.zip"]
const removeLeadingZipDirs = 1;

// list of directories to migrate. previously these were mounted in the "userdata" directory. retroarch.cfg is ignored intentionally.
const dirsToMigrate = ["cheats", "config", "content", "logs", "playlists", "saves", "screenshots", "states", "system", "thumbnails"];

/* no return functions that run on the browser thread */

const noReturnProxyFunctions = ["debugLog", "setProgress", "setProgressColor", "setProgressText"]

function handleNoReturnProxyFunction(func, args) {
	postMessage({type: "noReturn", func: func, args: args});
}

// add global functions
for (const func of noReturnProxyFunctions) {
	self[func] = function() {
		handleNoReturnProxyFunction(func, Array.from(arguments));
	}
}

/* misc */

function sleep(ms) {
	return new Promise(function(resolve) {
		setTimeout(resolve, ms);
	});
}

// for lazy-loading of scripts, doesn't load if it's already loaded
function loadScripts() {
	for (const path of arguments) {
		if (loadedScripts.includes(path)) continue;
		importScripts(path);
		loadedScripts.push(path);
	}
}

/* OPFS misc */

async function getDirHandle(path, create) {
	const parts = path.split("/");
	let here = root;
	for (const part of parts) {
		if (part == "") continue;
		try {
			here = await here.getDirectoryHandle(part, {create: !!create});
		} catch (e) {
			return;
		}
	}
	return here;
}

/* OPFS impl */

FS.init = async function() {
	root = await navigator.storage.getDirectory();
}

FS.writeFile = async function(path, data) {
	const dir_end = path.lastIndexOf("/");
	const parent = path.substr(0, dir_end);
	const child = path.substr(dir_end + 1);
	const parent_dir = await getDirHandle(parent, true);
	const file = await parent_dir.getFileHandle(child, {create: true});
	const handle = await file.createSyncAccessHandle({mode: "readwrite"});
	handle.write(data);
	// todo: should we call handle.flush() here?
	handle.close();
}

FS.readFile = async function(path) {
	const dir_end = path.lastIndexOf("/");
	const parent = path.substr(0, dir_end);
	const child = path.substr(dir_end + 1);
	const parent_dir = await getDirHandle(parent);
	if (!parent_dir) throw "directory doesn't exist";
	const file = await parent_dir.getFileHandle(child);
	const handle = await file.createSyncAccessHandle({mode: "read-only"});
	let data = new Uint8Array(new ArrayBuffer(handle.getSize()));
	handle.read(data);
	handle.close();
	return data;
}

// unlimited arguments
FS.mkdirTree = async function() {
	for (const path of arguments) {
		await getDirHandle(path, true);
	}
}

FS.readdir = async function(path) {
	let items = [];
	const dir = await getDirHandle(path);
	if (!dir) return;
	for await (const entry of dir.keys()) {
		items.push(entry);
	}
	items.reverse();
	return items;
}

FS.readdirTree = async function(path, maxDepth) {
	let items = [];
	if (isNaN(maxDepth)) maxDepth = 10;
	const dir = await getDirHandle(path);
	if (!dir) return;
	if (!path.endsWith("/")) path += "/";
	for await (const handle of dir.values()) {
		if (handle.kind == "file") {
			items.push(path + handle.name);
		} else if (handle.kind == "directory" && maxDepth > 0) {
			items.push.apply(items, await FS.readdirTree(path + handle.name, maxDepth - 1));
		}
	}
	items.reverse();
	return items;
}

// unlimited arguments
FS.rm = async function() {
	for (const path of arguments) {
		const dir_end = path.lastIndexOf("/");
		const parent = path.substr(0, dir_end);
		const child = path.substr(dir_end + 1);
		const parent_dir = await getDirHandle(parent);
		if (!parent_dir) continue;
		try {
			await parent_dir.removeEntry(child, {recursive: true});
		} catch (e) {
			continue;
		}
	}
}

FS.stat = async function(path) {
	const dir_end = path.lastIndexOf("/");
	const parent = path.substr(0, dir_end);
	if (!parent) return "directory";
	const child = path.substr(dir_end + 1);
	const parent_dir = await getDirHandle(parent);
	if (!parent_dir) return;
	for await (const handle of parent_dir.values()) {
		if (handle.name == child) return handle.kind;
	}
}

/* data migration */

function idbExists(dbName, objStoreName) {
	return new Promise((resolve, reject) => {
		let request = indexedDB.open(dbName);
		request.onupgradeneeded = function(e) {
			e.target.transaction.abort();
			resolve(false);
		}
		request.onsuccess = function(e) {
			let exists = objStoreName ? Array.from(e.target.result.objectStoreNames).includes(objStoreName) : true;
			e.target.result.close();
			resolve(exists);
		}
		request.onerror = function(e) {
			reject(e);
		}
	});
}

function deleteIdb(name) {
	return new Promise((resolve, reject) => {
		let request = indexedDB.deleteDatabase(name);
		request.onsuccess = function() {
			resolve();
		}
		request.onerror = function(e) {
			reject("Error deleting IndexedDB!");
		}
		request.onblocked = function(e) {
			reject("Request to delete IndexedDB was blocked!");
		}
	});
}

function initBfsIdbfs(name) {
	return new Promise((resolve, reject) => {
		loadScripts("jsdeps/browserfs.min.js");
		BrowserFS.getFileSystem({fs: "IndexedDB", options: {storeName: name}}, function(err, rv) {
			if (err) {
				reject(err);
			} else {
				BrowserFS.initialize(rv);
				BFSDB = rv.store.db;
				BFS = BrowserFS.BFSRequire("fs");
				resolve();
			}
		});
	});
}

// calls BFS.<method> with arguments..., returns a promise
function bfsAsyncCall(method) {
	return new Promise((resolve, reject) => {
		BFS[method].apply(null, Array.from(arguments).slice(1).concat(function(err, rv) {
			if (err) {
				reject(err);
			} else {
				resolve(rv);
			}
		}));
	});
}

async function migrateFiles(files) {
	for (let i = 0; i < files.length; i++) {
		const path = files[i];
		debugLog("    Migrating " + path);
		setProgressText("main", "Migrating file: " + path.substr(1));
		setProgress("main", (i + 1) / files.length);
		const data = await bfsAsyncCall("readFile", path);
		await FS.writeFile("/retroarch" + path, data);
	}
	setProgress("main", 0);
	setProgressText("main");
}

// this is really finicky (thanks browserfs), probably don't touch this
async function indexMigrateTree(dir, maxDepth) {
	let toMigrate = [];
	if (isNaN(maxDepth)) maxDepth = 10;
	const children = await bfsAsyncCall("readdir", dir);
	if (!dir.endsWith("/")) dir += "/";
	for (const child of children) {
		const info = await bfsAsyncCall("lstat", dir + child);
		if (info.isSymbolicLink()) continue;
		if (info.isFile() && dir != "/") {
			toMigrate.push(dir + child);
		} else if (info.isDirectory() && maxDepth > 0 && (dir != "/" || dirsToMigrate.includes(child))) {
			toMigrate.push.apply(toMigrate, await indexMigrateTree(dir + child, maxDepth - 1));
		}
	}
	return toMigrate;
}

// look for and migrate any data to the OPFS from the old BrowserFS in IndexedDB
async function tryMigrateFromIdbfs() {
	if (await FS.stat("/retroarch/.migration-finished") == "file" || !(await idbExists("RetroArch", "RetroArch"))) return;
	debugLog("Migrating data from BrowserFS IndexedDB");
	await initBfsIdbfs("RetroArch");
	const files = await indexMigrateTree("/", 5);
	await migrateFiles(files);
	await FS.writeFile("/retroarch/.migration-finished", new Uint8Array());
	BFSDB.close();
	await sleep(100); // above method might need extra time, and indexedDB.deleteDatabase only gives us one shot
	try {
		await deleteIdb("RetroArch");
	} catch (e) {
		debugLog("Warning: failed to delete old IndexedDB, probably doesn't matter.", e);
	}
	debugLog("Finished data migration! " + files.length + " files migrated successfully.");
}

/* bundle loading */

function incBundleCounter() {
	setProgress("main", ++bundleCounter / bundlePath.length);
}

async function setupZipFS(zipBuf) {
	loadScripts("jsdeps/zip-full.min.js");
	const mount = "/retroarch/";
	const zipReader = new zip.ZipReader(new zip.Uint8ArrayReader(zipBuf), {useWebWorkers: false});
	const entries = await zipReader.getEntries();
	setProgressText("main", "Extracting bundle... This only happens on the first visit or when the bundle is updated");
	for (let i = 0; i < entries.length; i++) {
		const file = entries[i];
		if (file.getData && !file.directory) {
			setProgress("main", (i + 1) / entries.length);
			const path = mount + file.filename.split("/").slice(removeLeadingZipDirs).join("/");
			const data = await file.getData(new zip.Uint8ArrayWriter());
			await FS.writeFile(path, data);
		}
	}
	await zipReader.close();
	setProgress("main", 0);
	setProgressText("main");
}

async function tryLoadBundle() {
	let outBuf;
	const timestampFile = "/retroarch/.bundle-timestamp";
	let timestamp = "";
	if (await FS.stat(timestampFile) == "file")
		timestamp = new TextDecoder().decode(await FS.readFile(timestampFile));

	let resp = await fetch(bundlePath[0], {headers: {"If-Modified-Since": timestamp, "Cache-Control": "public, max-age=0"}});
	if (resp.status == 200) {
		debugLog("Got new bundle");
		timestamp = resp.headers.get("last-modified");
		if (bundlePath.length > 1) {
			// split bundle
			let firstBuffer = await resp.arrayBuffer();
			setProgressColor("main", "#0275d8");
			setProgressText("main", "Fetching bundle... This only happens on the first visit or when the bundle is updated");
			incBundleCounter();
			// 256 MB max bundle size
			let buffer = new ArrayBuffer(256 * 1024 * 1024);
			let bufferView = new Uint8Array(buffer);
			bufferView.set(new Uint8Array(firstBuffer), 0);
			let idx = firstBuffer.byteLength;
			let buffers = await Promise.all(bundlePath.slice(1).map(i => fetch(i).then(r => { incBundleCounter(); return r.arrayBuffer(); })));
			for (let buf of buffers) {
				if (idx + buf.byteLength > buffer.maxByteLength) {
					throw "error: bundle zip is too large";
				}
				bufferView.set(new Uint8Array(buf), idx);
				idx += buf.byteLength;
			}
			setProgress("main", 0);
			setProgressColor("main");
			setProgressText("main");
			outBuf = new Uint8Array(buffer, 0, idx);
		} else {
			// single-file bundle
			outBuf = new Uint8Array(await resp.arrayBuffer());
		}
		debugLog("Unzipping...");
		let oldTime = performance.now();
		await setupZipFS(outBuf);
		await FS.writeFile(timestampFile, new TextEncoder().encode(timestamp));
		debugLog("Finished bundle load in " + Math.round((performance.now() - oldTime) / 1000) + " seconds");
	} else {
		debugLog("No new bundle exists");
	}
}

/* helper functions */

helper.loadFS = async function() {
	await tryMigrateFromIdbfs();
	await tryLoadBundle();
}

// zip directories... and return Uint8Array with zip file data
helper.zipDirs = async function() {
	let toZip = [];
	for (const path of arguments) {
		const files = await FS.readdirTree(path);
		if (files) toZip.push.apply(toZip, files);
	}
	if (toZip.length == 0) return;

	loadScripts("jsdeps/zip-full.min.js");
	const u8aWriter = new zip.Uint8ArrayWriter("application/zip");
	// using workers is faster for deflating, hmm...
	const writer = new zip.ZipWriter(u8aWriter, {useWebWorkers: true});
	for (let i = 0; i < toZip.length; i++) {
		const path = toZip[i];
		setProgress("modal", (i + 1) / toZip.length);
		setProgressText("modal", "Deflating: " + path.substr(1));
		try {
			const data = await FS.readFile(path);
			await writer.add(path.substr(1), new zip.Uint8ArrayReader(data), {level: 1});
		} catch (e) {
			debugLog("error while preparing zip", e);
		}
	}
	await writer.close();
	const zipped = await u8aWriter.getData();
	setProgress("modal", 0);
	setProgressText("modal");
	return zipped;
}

/* handle messages from main thread */

const handlers = {FS: FS, helper: helper};

onmessage = async function(msg) {
	if (msg.data?.handler in handlers && msg.data?.method in handlers[msg.data.handler]) {
		let ret;
		let err = false;
		try {
			ret = await handlers[msg.data.handler][msg.data.method].apply(null, msg.data?.args);
		} catch (e) {
			ret = e;
			err = true;
		}
		postMessage({type: "ret", id: msg.data?.id, ret: ret, err: err});
	}
}
