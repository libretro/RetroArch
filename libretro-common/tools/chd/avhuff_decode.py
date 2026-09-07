"""Reference decode of an A/V hunk's video, for checking FORMAT.md 9.4
against fields another implementation extracts.

    chdman extractld -i <image>.chd -o out.avi -isf 0 -if 4

gives four frames as 720x524 YUY2 with the two fields woven a row at a
time, so field n is hunk n.  Kept in Python because its only job is to
check the C against known output, and being a second implementation
rather than a copy of the first is the point.
"""
import sys
sys.path.insert(0,'/home/claude/chd')
from mapdec import Bits

def bflush(bs):
    while bs.bits >= 8:
        bs.o -= 1; bs.bits -= 8
    bs.bits = 0; bs.buf = 0
    return bs.o

def canon_down(lengths,maxbits):
    histo=[0]*(maxbits+2)
    for L in lengths:
        if L>maxbits: return None
        histo[L]+=1
    codes=[0]*len(lengths); cur=0; st=[0]*(maxbits+2)
    for L in range(maxbits,0,-1):
        tot=cur+histo[L]
        if L!=1 and (tot&1): return None
        st[L]=cur; cur=tot>>1
    for i,L in enumerate(lengths):
        if L: codes[i]=st[L]; st[L]+=1
    lut={}
    for i,L in enumerate(lengths):
        if L==0: continue
        sh=maxbits-L; base=codes[i]<<sh
        for j in range(1<<sh): lut[base+j]=(i,L)
    return lut

def read_tree_rle(bs,nc,mb):
    nb=5 if mb>=16 else (4 if mb>=8 else 3)
    lengths=[0]*nc; cur=0
    while cur<nc:
        v=bs.read(nb)
        if v!=1: lengths[cur]=v; cur+=1
        else:
            v=bs.read(nb)
            if v==1: lengths[cur]=1; cur+=1
            else:
                rep=bs.read(nb)+3
                while rep and cur<nc: lengths[cur]=v; cur+=1; rep-=1
    return canon_down(lengths,mb)

def rlecount(c):
    if c==0: return 1
    if c<=0x107: return 8+(c-0x100)
    return 16<<(c-0x108)

class Ctx:
    def __init__(s,l): s.l=l; s.prev=0; s.rle=0
    def one(s,bs):
        if s.rle: s.rle-=1; return s.prev
        i,L=s.l[bs.peek(16)]; bs.remove(L)
        if i<0x100: s.prev=(s.prev+i)&0xFF; return s.prev
        s.rle=rlecount(i)-1; return s.prev
    def flush(s): s.rle=0

def decode_video(v,W,H):
    bs=Bits(v); bs.read(8)
    ctx=[]
    for k in range(3):
        ctx.append(Ctx(read_tree_rle(bs,272,16))); bflush(bs)
    Y,Cb,Cr=ctx
    out=bytearray()
    for row in range(H):
        for x in range(W//2):
            out.append(Y.one(bs)); out.append(Cb.one(bs))
            out.append(Y.one(bs)); out.append(Cr.one(bs))
        Y.flush(); Cb.flush(); Cr.flush()
    return bytes(out)
