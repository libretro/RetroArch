# Writes rtt_fonts.h: run it with fontTools installed after changing the fonts.
# Builds the two small fonts rtt_test embeds: a TrueType one and a CID-keyed CFF one.
import math, io
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphComponent
from fontTools.ttLib import newTable
from fontTools.misc.psCharStrings import T2CharString

UPEM = 1000
order = [".notdef", "square", "circle", "combo", "dot"]
cmap = {0x41: "square", 0x4F: "circle", 0x43: "combo", 0x2E: "dot", 0x1F600: "circle"}

def ttf():
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    g = {}
    p = TTGlyphPen(None); g[".notdef"] = p.glyph()
    p = TTGlyphPen(None)
    p.moveTo((100, 0)); p.lineTo((100, 600)); p.lineTo((700, 600)); p.lineTo((700, 0)); p.closePath()
    g["square"] = p.glyph()
    # circle of radius 400 at (500, 400): 8 quadratic arcs
    p = TTGlyphPen(None)
    pts = []
    for i in range(8):
        a = i * math.pi / 4
        pts.append((500 + 400 * math.cos(a), 400 + 400 * math.sin(a)))
    k = 1 / math.cos(math.pi / 8)
    p.moveTo((round(pts[0][0]), round(pts[0][1])))
    for i in range(8):
        a = (i + 0.5) * math.pi / 4
        c = (round(500 + 400 * k * math.cos(a)), round(400 + 400 * k * math.sin(a)))
        e = pts[(i + 1) % 8]
        p.qCurveTo(c, (round(e[0]), round(e[1])))
    p.closePath()
    g["circle"] = p.glyph()
    # a lone point beside a tiny triangle
    p = TTGlyphPen(None)
    p.moveTo((0, 0)); p.lineTo((50, 0)); p.lineTo((0, 50)); p.closePath()
    p.moveTo((900, 950)); p.closePath()
    g["dot"] = p.glyph()
    combo = Glyph(); combo.numberOfContours = -1
    c1 = GlyphComponent(); c1.glyphName = "square"; c1.x = 0; c1.y = 0; c1.flags = 0x0004
    c2 = GlyphComponent(); c2.glyphName = "square"; c2.x = 800; c2.y = 100; c2.flags = 0x0004
    c2.transform = [[0.5, 0], [0, 0.5]]
    combo.components = [c1, c2]
    g["combo"] = combo
    fb.setupGlyf(g)
    fb.setupHorizontalMetrics({n: (1000, 0) for n in order} | {"square": (800, 100), "circle": (1000, 100), "combo": (1300, 100), "dot": (1000, 0)})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": "rtt test", "styleName": "Regular"})
    fb.setupOS2(); fb.setupPost()
    fb.setupVerticalMetrics({n: (1000, 100) for n in order} | {"square": (900, 200)})
    fb.setupVerticalHeader(ascent=500, descent=-500)
    kern = newTable("kern"); kern.version = 0
    from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0
    st = KernTable_format_0(); st.version = 0; st.coverage = 1; st.format = 0
    st.kernTable = {("square", "circle"): -50, ("circle", "square"): 30, ("combo", "dot"): 7}
    kern.kernTables = [st]
    fb.font["kern"] = kern
    fb.updateHead(created=0, modified=0); fb.font.recalcTimestamp = False
    b = io.BytesIO(); fb.save(b); return b.getvalue()

def cff():
    global order, cmap
    names = {"square": "cid00001", "circle": "cid00002", "combo": "cid00003", "dot": "cid00004"}
    order = [".notdef"] + [names[n] for n in order[1:]]
    cmap = {k: names[v] for k, v in cmap.items()}
    fb = FontBuilder(UPEM, isTTF=False)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    cs = {}
    cs[".notdef"] = T2CharString(program=[500, "endchar"])
    # square, with a width, stem hints and a hintmask
    cs["cid00001"] = T2CharString(program=[800, 0, 600, "hstemhm", 100, 600, "vstemhm",
        "hintmask", bytes([0xC0]), 100, 0, "rmoveto", 600, "vlineto", 600, "hlineto", -600, "vlineto", "endchar"])
    # circle from four cubics, drawn through a local and a global subroutine
    r = 400; kk = 0.5522847498 * r
    cs["cid00002"] = T2CharString(program=[900, 400, "rmoveto", 0 - 107, "callsubr", 0 - 107, "callgsubr", "endchar"])
    cs["cid00003"] = T2CharString(program=[0, 0, "rmoveto", 0, 600, 600, 0, 0, -600, "rlineto",
        # a flex back to the start line, then arithmetic: 2 3 add -> 5
        200, 0, "rmoveto", 5, 0, "rlineto", "endchar"])
    cs["cid00004"] = T2CharString(program=[0, 0, "rmoveto", 50, 0, -50, 50, "rlineto", "endchar"])
    fb.setupCFF("rtttest", {"FullName": "rtt test"}, cs, {})
    cffset = fb.font["CFF "].cff
    top = cffset.topDictIndex[0]
    from fontTools.cffLib import SubrsIndex, GlobalSubrsIndex
    # local subr 0: right half of the circle; global 0: left half
    loc = T2CharString(program=[0, kk, -(r - kk), r - kk, -kk, 0, "rrcurveto", -kk, 0, -(r - kk), -(r - kk), 0, -kk, "rrcurveto", "return"])
    glo = T2CharString(program=[0, -kk, r - kk, -(r - kk), kk, 0, "rrcurveto", kk, 0, r - kk, r - kk, 0, kk, "rrcurveto", "return"])
    priv = top.Private
    priv.Subrs = SubrsIndex(); priv.Subrs.append(loc)
    cffset.GlobalSubrs.append(glo)
    for c in cs.values():
        c.private = priv; c.globalSubrs = cffset.GlobalSubrs
    loc.private = priv; loc.globalSubrs = cffset.GlobalSubrs
    glo.private = priv; glo.globalSubrs = cffset.GlobalSubrs
    fb.setupHorizontalMetrics({n: (1000, 0) for n in order} | {"cid00001": (800, 100), "cid00002": (1000, 100)})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": "rtt test", "styleName": "Regular"})
    fb.setupOS2(); fb.setupPost()
    fb.updateHead(created=0, modified=0); fb.font.recalcTimestamp = False
    b = io.BytesIO(); fb.save(b)
    data = b.getvalue()
    # Make it CID-keyed with two font dicts; .notdef..circle in FD 0, the rest in FD 1
    from fontTools.ttLib import TTFont
    f = TTFont(io.BytesIO(data), recalcTimestamp=False)
    cset = f["CFF "].cff; top = cset.topDictIndex[0]
    from fontTools.cffLib import FDArrayIndex, FontDict, FDSelect, PrivateDict
    top.ROS = ("Adobe", "Identity", 0)
    top.CIDCount = len(order)
    fda = FDArrayIndex(); fda.strings = cset.strings
    for i in range(2):
        fd = FontDict(); fd.strings = cset.strings; fd.Private = top.Private
        fda.append(fd)
    top.FDArray = fda
    top.FDSelect = FDSelect(); top.FDSelect.format = 3
    top.FDSelect.gidArray = [0, 0, 0, 1, 1]
    top.charset = order
    del top.rawDict["Private"]
    if hasattr(top, "Private"):
        del top.Private
    b = io.BytesIO(); f.save(b); return b.getvalue()

def carray(name, data):
    out = ["static const unsigned char %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 16):
        out.append("   " + ",".join("0x%02x" % x for x in data[i:i + 16]) + ",")
    out.append("};")
    return "\n".join(out)

t = ttf(); c = cff()


hdr = ["/* Generated by rtt_fonts.py with fontTools: two small fonts drawn",
       " * for rtt_test, a TrueType one and a CID-keyed CFF one. */", "",
       carray("rtt_test_ttf", t), "", carray("rtt_test_cff", c), ""]
open("rtt_fonts.h", "w").write("\n".join(hdr))
