#!/usr/bin/env node

var fs, fs_listing, rdSync, symLinks;

fs = require('fs');

symLinks = {};

rdSync = function(dpath, tree, name) {
  var child, file, files, fpath, fstat, i, len, lstat, stat_dev;
  files = fs.readdirSync(dpath);
  for (i = 0, len = files.length; i < len; i++) {
    file = files[i];
    if ((file === '.git' || file === 'node_modules' || file === 'bower_components' || file === 'build') || file[0] === '.') {
      continue;
    }
    fpath = dpath + '/' + file;
    try {
      lstat = fs.lstatSync(fpath);
      if (lstat.isSymbolicLink()) {
        if (symLinks[stat_dev = lstat.dev] == null) {
          symLinks[stat_dev] = {};
        }
        if (symLinks[lstat.dev][lstat.ino] != null) {
          continue;
        }
        symLinks[lstat.dev][lstat.ino] = 0;
      }
      fstat = fs.statSync(fpath);
      if (fstat.isDirectory()) {
        tree[file] = child = {};
        rdSync(fpath, child, file);
      } else {
        tree[file] = null;
      }
    } catch (_error) {}
  }
  return tree;
};

fs_listing = rdSync(process.cwd(), {}, '/');

console.log(JSON.stringify(fs_listing));

