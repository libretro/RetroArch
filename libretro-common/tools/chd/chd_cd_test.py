import struct,sys,zlib,lzma,subprocess,os
sys.path.insert(0,'/home/claude/chd')
from mapdec import Bits, read_tree_rle, decode_one

f_lut=[0]*256; b_lut=[0]*256
for i in range(256): f_lut[i]=((i<<1)^(0x11D if i&0x80 else 0))&0xFF
for i in range(256): b_lut[i^f_lut[i]]=i
def ecc_block(sec,base,mc,mn,mm,mi,dest):
    size=mc*mn
    for major in range(mc):
        idx=(major>>1)*mm+(major&1); a=b=0
        for _ in range(mn):
            t=sec[base+idx]; idx+=mi
            if idx>=size: idx-=size
            a^=t; b^=t; a=f_lut[a]
        a=b_lut[f_lut[a]^b]
        sec[dest+major]=a; sec[dest+major+mc]=a^b
def ecc_generate(sec):
    sec[0]=0
    for i in range(1,11): sec[i]=0xFF
    sec[11]=0
    hdr=bytes(sec[12:16]); sec[12:16]=b'\0'*4
    ecc_block(sec,0x0C,86,24,2,86,0x81C)
    ecc_block(sec,0x0C,52,43,86,88,0x8C8)
    sec[12:16]=hdr

def lzma_raw(data, outlen, hunkbytes):
    d=1<<11
    while d < hunkbytes: d<<=1
    filt=[{"id":lzma.FILTER_LZMA1,"lc":3,"lp":0,"pb":2,"dict_size":d}]
    return lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=filt).decompress(data,outlen)

def load(path):
    f=open(path,'rb'); hdr=f.read(124)
    if struct.unpack('>I',hdr[12:16])[0]!=5: return None
    mo=struct.unpack('>Q',hdr[40:48])[0]
    codecs=[hdr[16+i*4:20+i*4].decode('latin1') for i in range(4)]
    hb_,ub=struct.unpack('>II',hdr[56:64])
    nh=(struct.unpack('>Q',hdr[32:40])[0]+hb_-1)//hb_
    f.seek(mo); h=f.read(16); ml=struct.unpack('>I',h[0:4])[0]
    ds=int.from_bytes(h[4:10],'big'); lb=h[12]
    b=Bits(f.read(ml)); lut,_=read_tree_rle(b,16,8,4,'down')
    comp=[];last=0;rep=0
    for _ in range(nh):
        if rep>0: comp.append(last); rep-=1; continue
        v=decode_one(b,lut,8)
        if v==7: comp.append(last); rep=2+decode_one(b,lut,8)
        elif v==8: comp.append(last); rep=2+16+(decode_one(b,lut,8)<<4)+decode_one(b,lut,8)
        else: last=v; comp.append(v)
    cur=ds; blobs=[]
    hb2=h[13]; pb2=h[14]
    for n in range(nh):
        c=comp[n]
        if c in (0,1,2,3): L=b.read(lb); b.read(16); blobs.append((c,cur,L)); cur+=L
        elif c==4: b.read(16); blobs.append((c,cur,hb_)); cur+=hb_
        elif c==5: b.read(hb2); blobs.append((c,None,0))    # self ref: consumes bits
        elif c==6: b.read(pb2); blobs.append((c,None,0))    # parent ref: consumes bits
        else: blobs.append((c,None,0))                      # pseudo-codes: no bits
    if cur-ds != mo-ds:
        raise ValueError("map span %d != expected %d" % (cur-ds, mo-ds))
    return f,codecs,hb_,ub,blobs

def decode_cd(f,codec,blob,hb_,ub):
    frames=hb_//ub; eccn=(frames+7)//8
    cln = 2 if hb_ < 65536 else 3
    hh=eccn+cln
    eccmap=blob[:eccn]
    complen=int.from_bytes(blob[eccn:eccn+cln],'big')
    bd=blob[hh:hh+complen]; sd=blob[hh+complen:]
    if codec=='cdzl':   base=zlib.decompressobj(-15).decompress(bd)
    elif codec=='cdlz': base=lzma_raw(bd,frames*2352,hb_)
    else: return None
    sub=zlib.decompressobj(-15).decompress(sd) if sd else b'\0'*(frames*96)
    out=bytearray()
    for i in range(frames):
        sec=bytearray(base[i*2352:(i+1)*2352])
        if eccmap[i>>3] & (1 << (i&7)): ecc_generate(sec)
        out+=sec; out+=sub[i*96:(i+1)*96]
    return bytes(out)

for p in sorted(os.listdir('corpus')):
    if not p.endswith('.chd'): continue
    path='corpus/'+p
    r0=load(path)
    if r0 is None: continue
    f,codecs,hb_,ub,blobs=r0
    if 'cdzl' not in codecs and 'cdlz' not in codecs: continue
    tried=matched=skipped=0
    for n,(c,off,L) in enumerate(blobs):
        if c is None or off is None or c>3: continue
        codec=codecs[c].strip('\0')
        if codec not in ('cdzl','cdlz'): skipped+=1; continue
        if tried>=40: break
        f.seek(off); blob=f.read(L)
        try: mine=decode_cd(f,codec,blob,hb_,ub)
        except Exception: mine=None
        r=subprocess.run(['./dumphunk',path,str(n),'/tmp/tr.bin'],capture_output=True)
        if r.returncode!=0: continue
        truth=open('/tmp/tr.bin','rb').read()
        tried+=1
        if mine==truth: matched+=1
    print("%-46s codecs=%-22s hunks tried=%-3d byte-exact=%-3d %s"
          % (p[:46],','.join(c.strip('\0') for c in codecs if c.strip('\0')),tried,matched,
             "PASS" if tried and tried==matched else "FAIL"))
