importScripts("zip-no-worker.min.js");

async function setupZipFS(zipBuf) {
  async function writeFile(path, data) {
    const dir_end = path.lastIndexOf("/");
    const parent = path.substr(0, dir_end);
    const child = path.substr(dir_end+1);
    const parent_dir = await mkdirTree(parent);
    const file = await parent_dir.getFileHandle(child,{create:true});
    const stream = await file.createSyncAccessHandle();
    const written = stream.write(data);
    stream.close();
  }
  async function mkdirTree(path) {
    const parts = path.split("/");
    let here = root;
    for (const part of parts) {
      if (part == "") { continue; }
      here = await here.getDirectoryHandle(part, {create:true});
    }
    return here;
  }
  const root = await navigator.storage.getDirectory();
  const zipReader = new zip.ZipReader(new zip.Uint8ArrayReader(zipBuf), {useWebWorkers:false});
  const entries = await zipReader.getEntries();
  for(const file of entries) {
    if (file.getData && !file.directory) {
      const writer = new zip.Uint8ArrayWriter();
      const data = await file.getData(writer);
      await writeFile(file.filename, data);
    } else if (file.directory) {
      await mkdirTree(file.filename);
    }
  }
  await zipReader.close();
}

onmessage = async (msg) => {
  let old_timestamp = msg.data;
  try {
    const root = await navigator.storage.getDirectory();
    const _bundle = await root.getDirectoryHandle("bundle");
  } catch (_e) {
    old_timestamp = "";
  }
  let resp = await fetch("assets/frontend/bundle-minimal.zip", {
    headers: {
      "If-Modified-Since": old_timestamp
    }
  });
  if (resp.status == 200) {
    await setupZipFS(new Uint8Array(await resp.arrayBuffer()));
  } else {
    await resp.text();
  }
  postMessage(resp.headers.get("last-modified"));
  close();
}
