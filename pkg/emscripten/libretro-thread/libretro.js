/**
 * RetroArch Web Player
 *
 * This provides the basic JavaScript for the RetroArch web player.
 */

const canvas = document.getElementById("canvas");
const webplayerPreview = document.getElementById("webplayer-preview");
const menuBar = document.getElementById("navbar");
const menuHider = document.getElementById("menuhider");
const coreSelector = document.getElementById("core-selector");
const coreSelectorCurrent = document.getElementById("current-core");
const dropdownBox = document.getElementById("dropdown-box");
const btnFiles = document.getElementById("btnFiles");
const btnRun = document.getElementById("btnRun");
const btnMenu = document.getElementById("btnMenu");
const btnFullscreen = document.getElementById("btnFullscreen");
const btnHelp = document.getElementById("btnHelp");
const btnAdd = document.getElementById("btnAdd");
const icnRun = document.getElementById("icnRun");
const icnAdd = document.getElementById("icnAdd");
const modalContainer = document.getElementById("modals");
const modalWindow = document.getElementById("modal-window");
const modalTitle = document.getElementById("modal-title");
const modalClose = document.getElementById("modal-close");
const fileManagerPanel = document.getElementById("fileManagerPanel");

const progressTrackers = {
	"main":  {bar: document.getElementById("progressBarMain"),  text: document.getElementById("progressTextMain")},
	"modal": {bar: document.getElementById("progressBarModal"), text: document.getElementById("progressTextModal")}
};

const modals = {
	"help":  {title: "Basics",          width: "750px", element: document.getElementById("helpModal")},
	"files": {title: "File Management", width: "400px", element: document.getElementById("filesModal")}
};

// Attempt to disable some default browser keys.
const disableKeys = {
	9: "tab",
	13: "enter",
	16: "shift",
	18: "alt",
	27: "esc",
	33: "rePag",
	34: "avPag",
	35: "end",
	36: "home",
	37: "left",
	38: "up",
	39: "right",
	40: "down",
	112: "F1",
	113: "F2",
	114: "F3",
	115: "F4",
	116: "F5",
	117: "F6",
	118: "F7",
	119: "F8",
	120: "F9",
	121: "F10",
	122: "F11",
	123: "F12"
};

let fsLoadPromise;

// all methods provided by the worker that we may require
const workerHandlers = {FS: ["init", "writeFile", "readFile", "mkdirTree", "readdir", "readdirTree", "rm", "stat"], helper: ["loadFS", "zipDirs"]};

const worker = new Worker("libretro.worker.js");
let workerMessageQueue = [];
worker.onmessage = (msg) => {
	switch (msg.data?.type) {
		case "noReturn":
			window[msg.data?.func]?.apply(null, msg.data?.args);
			break;
		case "ret":
			const ind = workerMessageQueue.findIndex(i => msg.data?.id in i);
			if (ind < 0) break;
			const promise = workerMessageQueue.splice(ind, 1)[0][msg.data.id];
			if (msg.data.err) {
				promise.reject(msg.data?.ret);
			} else {
				promise.resolve(msg.data?.ret);
			}
			break;
	}
}

function handleWorkerFunc(handler, method, args) {
	return new Promise((resolve, reject) => {
		const id = "" + Math.random();
		workerMessageQueue.push({[id]: {resolve: resolve, reject: reject}});
		worker.postMessage({id: id, handler: handler, method: method, args: args});
	});
}

// add the global functions from workerHandlers
// this makes the methods here appear identical to the implementation in the worker
for (let [handler, methods] of Object.entries(workerHandlers)) {
	let methodHandlers = {};
	for (let method of methods) {
		methodHandlers[method] = async function() {
			return await handleWorkerFunc(handler, method, Array.from(arguments));
		}
	}
	window[handler] = methodHandlers;
}

// console.log alias for worker to use
function debugLog() {
	console.log.apply(null, Array.from(arguments));
}

// n is the name of the bar ("main" or "modal")
// progress in range [0, 1]
function setProgress(n, progress) {
	const progressBar = progressTrackers[n]?.bar;
	if (!progressBar) return;
	if (isNaN(progress)) progress = 0;
	progressBar.style.height = progress ? "4px" : "0px";
	progressBar.style.setProperty("--progressbarpercent", (progress * 100) + "%");
}

function setProgressColor(n, color) {
	const progressBar = progressTrackers[n]?.bar;
	if (!progressBar) return;
	progressBar.style.setProperty("--progressbarcolor", color || "#1fb01a");
}

function setProgressText(n, text) {
	const progressText = progressTrackers[n]?.text;
	if (!progressText) return;
	progressText.textContent = text ?? "";
}

// "help" or "files"
function openModal(which) {
	if (which in modals) {
		for (const modal of Object.values(modals)) {
			modal.element.style.display = "none";
		}
		modalTitle.textContent = modals[which].title ?? "";
		modalWindow.style.width = modals[which].width ?? "750px";
		modals[which].element.style.display = "block";
		modalContainer.style.display = "block";
	}
}

modalClose.addEventListener("click", function() {
	modalContainer.style.display = "none";
});

var Module = {
	noInitialRun: true,
	arguments: ["-v", "--menu"],
	noImageDecoding: true,
	noAudioDecoding: true,

	retroArchSend: function(msg) {
		this.EmscriptenSendCommand(msg);
	},
	retroArchRecv: function() {
		return this.EmscriptenReceiveCommandReply();
	},
	preRun: [
		function(module) {
			module.ENV["OPFS_MOUNT"] = "/home/web_user";
		}
	],
	onRuntimeInitialized: function() {
		appInitialized();
	},
	print: function(text) {
		console.log("stdout:", text);
	},
	printErr: function(text) {
		console.log("stderr:", text);
	},
	canvas: canvas,
	totalDependencies: 0,
	monitorRunDependencies: function(left) {
		this.totalDependencies = Math.max(this.totalDependencies, left);
	}
};

// read File object to an ArrayBuffer
function readFile(file) {
	return new Promise((resolve, reject) => {
		let reader = new FileReader();
		reader.onload = function() {
			resolve(this.result);
		}
		reader.onerror = function(e) {
			reject(e);
		}
		reader.readAsArrayBuffer(file);
	});
}

// accept (optional) can be used to specify file extensions (string, comma delimited)
// returns an array of {path: String, data: ArrayBuffer}
function uploadFiles(accept) {
	return new Promise((resolve, reject) => {
		let input = document.createElement("input");
		input.type = "file";
		input.setAttribute("multiple", "");
		if (accept) input.accept = accept;
		input.style.setProperty("display", "none", "important");
		document.body.appendChild(input);
		input.addEventListener("change", async function() {
			let files = [];
			for (const file of this.files) {
				files.push({path: file.name, data: await readFile(file)});
			}
			document.body.removeChild(input);
			resolve(files);
		});
		input.oncancel = function() {
			document.body.removeChild(input);
			resolve([]);
		}
		input.click();
	});
}

// prompt user to upload file(s) to a dir in OPFS, e.g. "/retroarch/content"
async function uploadFilesToDir(dir, accept) {
	const files = await uploadFiles(accept);
	for (const file of files) {
		await FS.writeFile(dir + "/" + file.path, new Uint8Array(file.data));
		console.log("file upload complete: " + file.path);
	}
}

// download data (ArrayBuffer/DataView) with file name and optional mime type
function downloadFile(data, name, mime) {
	let a = document.createElement("a");
	a.download = name;
	a.href = URL.createObjectURL(new Blob([data], {type: mime || "application/octet-stream"}));
	a.click();
	window.setTimeout(function() {
		URL.revokeObjectURL(a.href);
	}, 2000);
}

// click handler for the file manager modal
async function fileManagerEvent(target) {
	const action = target?.dataset?.action;
	if (!action) return;
	target.classList.add("disabled");
	let data;
	switch (action) {
		case "upload_saves":
			await uploadFilesToDir("/retroarch/saves");
			break;
		case "upload_states":
			await uploadFilesToDir("/retroarch/states");
			break;
		case "upload_system":
			await uploadFilesToDir("/retroarch/system");
			break;
		case "download_sss":
			data = await helper.zipDirs("/retroarch/saves", "/retroarch/states", "/retroarch/screenshots");
			downloadFile(data, "saves_states_screenshots.zip", "application/zip");
			break;
		case "download_all":
			data = await helper.zipDirs("/retroarch/saves", "/retroarch/states", "/retroarch/screenshots", "/retroarch/content");
			downloadFile(data, "all.zip", "application/zip");
			break;
		case "delete_sss":
			await FS.rm("/retroarch/saves", "/retroarch/states", "/retroarch/screenshots");
			break;
		case "delete_content":
			await FS.rm("/retroarch/content");
			break;
		case "delete_config":
			await FS.rm("/.config/retroarch");
			break;
		case "delete_assets":
			await FS.rm("/retroarch/.bundle-timestamp", "/retroarch/assets", "/retroarch/autoconfig",
			            "/retroarch/database", "/retroarch/filters", "/retroarch/info", "/retroarch/overlays", "/retroarch/shaders");
			break;
		case "delete_all":
			await FS.rm("/retroarch", "/.config/retroarch");
			break;
	}
	target.classList.remove("disabled");
}

function appIsSmallScreen() {
	return window.matchMedia("(max-width: 720px)").matches;
}

// used for the menu hider
function adjustMenuHeight() {
	const actualMenuHeight = menuHider.checked ? 0 : 65;
	document.body.style.setProperty("--actualmenuheight", actualMenuHeight + "px", "important")
}

function startRetroArch() {
	// show the "changes you made may not be saved" warning
	window.addEventListener("beforeunload", function(e) { e.preventDefault(); });

	window.addEventListener("keydown", function(e) {
		if (disableKeys[e.which]) e.preventDefault();
	});

	webplayerPreview.classList.add("hide");
	btnRun.classList.add("hide");

	btnMenu.classList.remove("disabled");
	btnMenu.addEventListener("click", function() {
		Module._cmd_toggle_menu();
	});

	btnFullscreen.classList.remove("disabled");
	btnFullscreen.addEventListener("click", function() {
		Module.requestFullscreen(false);
	});

	// ensure the canvas is focused so that keyboard events work
	Module.canvas.focus();
	Module.canvas.addEventListener("pointerdown", function() {
		Module.canvas.focus();
	}, false);
	menuBar.addEventListener("pointerdown", function() {
		setTimeout(function() {
			Module.canvas.focus();
		}, 0);
	}, false);

	Module.callMain(Module.arguments);
}

// called when the emscripten module has loaded
async function appInitialized() {
	console.log("WASM runtime initialized");
	await fsLoadPromise;
	console.log("FS initialized");
	setProgress("main");
	setProgressText("main");
	icnRun.classList.remove("fa-spinner", "fa-spin");
	icnRun.classList.add("fa-play");
	// Make the Preview image clickable to start RetroArch.
	webplayerPreview.classList.add("loaded");
	webplayerPreview.addEventListener("click", function() {
		startRetroArch();
	});
	btnRun.classList.remove("disabled");
	btnRun.addEventListener("click", function() {
		startRetroArch();
	});
}

async function downloadScript(src) {
	let resp = await fetch(src);
	let blob = await resp.blob();
	return blob;
}

async function loadCore(core) {
	// Make the core the selected core in the UI.
	const coreTitle = document.querySelector('#core-selector a[data-core="' + core + '"]')?.textContent;
	if (coreTitle) coreSelectorCurrent.textContent = coreTitle;
	const fileExt = (core == "retroarch") ? ".js" : "_libretro.js";
	const url = URL.createObjectURL(await downloadScript("./" + core + fileExt));
	Module.mainScriptUrlOrBlob = url;
	import(url).then(script => {
		script.default(Module).then(mod => {
			Module = mod;
		}).catch(err => { console.error("Couldn't instantiate module", err); throw err; });
	}).catch(err => { console.error("Couldn't load script", err); throw err; });
}

// When the browser has loaded everything.
document.addEventListener("DOMContentLoaded", async function() {
	// watch the menu toggle checkbox
	menuHider.addEventListener("change", adjustMenuHeight);
	if (appIsSmallScreen()) menuHider.checked = true;
	adjustMenuHeight();

	// make it easier to exit the core selector drop-down menu
	document.addEventListener("click", function(e) {
		if (!coreSelector.parentElement.contains(e.target)) dropdownBox.checked = false;
	});

	// disable default right click action
	canvas.addEventListener("contextmenu", function(e) {
		e.preventDefault();
	}, false);

	// init the OPFS
	await FS.init();
	fsLoadPromise = helper.loadFS();

	btnFiles.addEventListener("click", function() {
		openModal("files");
	});

	btnHelp.addEventListener("click", function() {
		openModal("help");
	});

	btnAdd.classList.remove("disabled");
	btnAdd.addEventListener("click", async function() {
		btnAdd.classList.add("disabled");
		icnAdd.classList.remove("fa-plus");
		icnAdd.classList.add("fa-spinner", "fa-spin");
		await uploadFilesToDir("/retroarch/content");
		btnAdd.classList.remove("disabled");
		icnAdd.classList.remove("fa-spinner", "fa-spin");
		icnAdd.classList.add("fa-plus");
	});
	
	fileManagerPanel.addEventListener("click", function(e) {
		fileManagerEvent(e.target);
	});

	// Switch the core when selecting one.
	coreSelector.addEventListener("click", function(e) {
		const coreChoice = e.target.dataset?.core;
		if (coreChoice) localStorage.setItem("core", coreChoice);
	});

	// Find which core to load.
	const core = localStorage.getItem("core") || "gambatte";
	loadCore(core);
});
