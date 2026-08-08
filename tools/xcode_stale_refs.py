"""Report Xcode file references naming files that no longer exist.

Run from the repository root:

    tools/xcode_stale_refs.py [-v]

Every PBXFileReference with sourceTree "<group>" names a file that
should be somewhere in the tree. This checks by basename, so it finds
references to files that were deleted or renamed, which is how
bitmap.h, stb_unicode.c, d3d10_common.c, glslang_util_cxx.cpp,
opaque.cg.h, zconf.h and zlib.h were found still listed after the
files themselves had gone.

Two limits worth knowing. It does not resolve the group path, so a
reference that names a file which still exists but has moved to a
different directory will not be flagged. And absolute paths, and paths
climbing out of the tree, are skipped: those are the SDK, Homebrew and
build products, none of which are ours to resolve."""
import re,os,glob,sys
from collections import defaultdict

present=defaultdict(list)
for root,dirs,fs in os.walk('.'):
    if '.git' in root.split(os.sep): continue
    for f in fs + dirs:                      # bundles/xcassets are directories
        present[f].append(os.path.join(root,f))

tot=0
for pb in sorted(glob.glob('pkg/apple/*.xcodeproj/project.pbxproj')):
    s=open(pb,encoding='utf-8',errors='surrogateescape').read()
    miss=[]
    for m in re.finditer(r'\{isa = PBXFileReference;(.*?)\};', s, re.S):
        b=m.group(1)
        st=re.search(r'sourceTree = ("[^"]*"|[^;]*);', b)
        st=(st.group(1).strip('"') if st else '')
        if st!='<group>': continue          # SDK, build products, source root
        p=re.search(r'\bpath = ("[^"]*"|[^;]*);', b)
        if not p: continue
        path=p.group(1).strip('"')
        # absolute paths and paths climbing out of the tree are the
        # SDK, Homebrew or build products - not ours to resolve
        if path.startswith('/') or path.startswith('../../../'):
            continue
        base=os.path.basename(path)
        if base not in present:
            miss.append(path)
    tot+=len(miss)
    print("%-42s stale=%d" % (pb.split('/')[-2], len(miss)))
    if miss and '-v' in sys.argv:
        for x in sorted(set(miss)): print("      ", x)
print("\ntotal stale references:", tot)
