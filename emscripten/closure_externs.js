// Prevents the closure compiler from minifying certain property names.
// I don't know of a better way to do this without modifying emscripten libraries.
// Note that this isn't inserted into compiled JS file; it only instructs the closure compiler.

// FS methods
Object.prototype.FSNode={};
Object.prototype.FSStream={};
Object.prototype.absolutePath={};
Object.prototype.analyzePath={};
Object.prototype.chdir={};
Object.prototype.checkOpExists={};
Object.prototype.chmod={};
Object.prototype.chown={};
Object.prototype.close={};
Object.prototype.closeStream={};
Object.prototype.create={};
Object.prototype.createDataFile={};
Object.prototype.createDefaultDevices={};
Object.prototype.createDefaultDirectories={};
Object.prototype.createDevice={};
Object.prototype.createFile={};
Object.prototype.createFolder={};
Object.prototype.createLazyFile={};
Object.prototype.createLink={};
Object.prototype.createNode={};
Object.prototype.createPath={};
Object.prototype.createPreloadedFile={};
Object.prototype.createSpecialDirectories={};
Object.prototype.createStandardStreams={};
Object.prototype.createStream={};
Object.prototype.cwd={};
Object.prototype.destroyNode={};
Object.prototype.doChmod={};
Object.prototype.doChown={};
Object.prototype.doSetAttr={};
Object.prototype.doTruncate={};
Object.prototype.dupStream={};
Object.prototype.fchmod={};
Object.prototype.fchown={};
Object.prototype.findObject={};
Object.prototype.flagsToPermissionString={};
Object.prototype.forceLoadFile={};
Object.prototype.fstat={};
Object.prototype.ftruncate={};
Object.prototype.getDevice={};
Object.prototype.getMounts={};
Object.prototype.getPath={};
Object.prototype.getStream={};
Object.prototype.getStreamChecked={};
Object.prototype.hashAddNode={};
Object.prototype.hashName={};
Object.prototype.hashRemoveNode={};
Object.prototype.init={};
Object.prototype.ioctl={};
Object.prototype.isBlkdev={};
Object.prototype.isChrdev={};
Object.prototype.isClosed={};
Object.prototype.isDir={};
Object.prototype.isFIFO={};
Object.prototype.isFile={};
Object.prototype.isLink={};
Object.prototype.isMountpoint={};
Object.prototype.isRoot={};
Object.prototype.isSocket={};
Object.prototype.joinPath={};
Object.prototype.lchmod={};
Object.prototype.lchown={};
Object.prototype.llseek={};
Object.prototype.lookup={};
Object.prototype.lookupNode={};
Object.prototype.lookupPath={};
Object.prototype.lstat={};
Object.prototype.major={};
Object.prototype.makedev={};
Object.prototype.mayCreate={};
Object.prototype.mayDelete={};
Object.prototype.mayLookup={};
Object.prototype.mayOpen={};
Object.prototype.minor={};
Object.prototype.mkdev={};
Object.prototype.mkdir={};
Object.prototype.mkdirTree={};
Object.prototype.mknod={};
Object.prototype.mmap={};
Object.prototype.mmapAlloc={};
Object.prototype.mount={};
Object.prototype.msync={};
Object.prototype.nextfd={};
Object.prototype.nodePermissions={};
Object.prototype.open={};
Object.prototype.preloadFile={};
Object.prototype.quit={};
Object.prototype.read={};
Object.prototype.readFile={};
Object.prototype.readdir={};
Object.prototype.readlink={};
Object.prototype.registerDevice={};
Object.prototype.rename={};
Object.prototype.rmdir={};
Object.prototype.standardizePath={};
Object.prototype.stat={};
Object.prototype.statfs={};
Object.prototype.statfsNode={};
Object.prototype.statfsStream={};
Object.prototype.staticInit={};
Object.prototype.symlink={};
Object.prototype.syncfs={};
Object.prototype.truncate={};
Object.prototype.unlink={};
Object.prototype.unmount={};
Object.prototype.utime={};
Object.prototype.write={};
Object.prototype.writeFile={};

// FS properties
Object.prototype.MAX_OPEN_FDS={};
Object.prototype.chrdev_stream_ops={};
Object.prototype.currentPath={};
Object.prototype.devices={};
Object.prototype.filesystems={};
Object.prototype.ignorePermissions={};
Object.prototype.initialized={};
Object.prototype.mounts={};
Object.prototype.nameTable={};
Object.prototype.nextInode={};
Object.prototype.readFiles={};
Object.prototype.root={};
Object.prototype.streams={};
Object.prototype.syncFSRequests={};
Object.prototype.trackingDelegate={};

// FS.analyzePath() object
Object.prototype.error={};
Object.prototype.exists={};
Object.prototype.isRoot={};
Object.prototype.name={};
Object.prototype.object={};
Object.prototype.parentExists={};
Object.prototype.parentObject={};
Object.prototype.parentPath={};
Object.prototype.path={};

// FS.lookupPath() object
Object.prototype.node={};
Object.prototype.path={};

// FS.FSNode / FS.FSStream
Object.prototype.atime={};
Object.prototype.ctime={};
Object.prototype.id={};
Object.prototype.mode={};
Object.prototype.mount={};
Object.prototype.mounted={};
Object.prototype.mtime={};
Object.prototype.name={};
Object.prototype.node_ops={};
Object.prototype.parent={};
Object.prototype.rdev={};
Object.prototype.readMode={};
Object.prototype.stream_ops={};
Object.prototype.writeMode={};

Object.prototype.isDevice={};
Object.prototype.isFolder={};
Object.prototype.read={};
Object.prototype.write={};

Object.prototype.contents={};
Object.prototype.name_next={};

Object.prototype.dev={};
Object.prototype.ino={};
Object.prototype.nlink={};
Object.prototype.uid={};
Object.prototype.gid={};
Object.prototype.size={};
Object.prototype.blksize={};
Object.prototype.blocks={};
Object.prototype.timestamp={};

Object.prototype.mountpoint={};
Object.prototype.mounts={};
Object.prototype.opts={};
Object.prototype.root={};
Object.prototype.type={};

Object.prototype.nfd={};
Object.prototype.flags={};
Object.prototype.position={};

Object.prototype.getattr={};
Object.prototype.setattr={};

// PATH methods
Object.prototype.basename={};
Object.prototype.dirname={};
Object.prototype.isAbs={};
Object.prototype.join={};
Object.prototype.join2={};
Object.prototype.normalize={};
Object.prototype.normalizeArray={};
Object.prototype.splitPath={};

// ErrnoError properties
Object.prototype.errno={};
Object.prototype.message={};
Object.prototype.name={};
Object.prototype.node={};

// Avoid emscripten bug with ASSERTIONS=0 FS_DEBUG=1
/**
 * @suppress {duplicate}
 */
function dbg(){}

// RWebPad rumble
Object.prototype.vibrationActuator={};
Object.prototype.playEffect={};
Object.prototype.reset={};
