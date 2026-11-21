#!/usr/bin/env python3
"""Execute the shipping MSL fragment, translated to C89, on CPU vectors."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
start = source.index('      "fragment float4 views_media(')
end = source.index('      "fragment float4 views_layer(', start)
fragment = "".join(json.loads(line.strip()) for line in source[start:end].splitlines())
fragment = re.sub(r"\s*\[\[[^\]]+\]\]", "", fragment)


class MslToC:
    """Translates the MSL subset the viewer fragment uses into C89 calls,
    so the shipping shader text runs as written.  Anything outside the
    subset raises, so a shader change cannot silently drift."""

    VECS = {"float2": 2, "float3": 3, "float4": 4}
    SCALARS = ("float", "int")
    FIELDS = {"ViewsOut": {"texcoord": "float2"}, "ViewsUBO": {"clamp": "float4"}}
    ELEMENTWISE = ("pow", "max", "min", "abs", "sign", "clamp")
    PREC = {"<": 10, ">": 10, "<=": 10, ">=": 10, "==": 10, "!=": 10,
            "+": 20, "-": 20, "*": 30, "/": 30}

    def __init__(self, text):
        self.toks = re.findall(r"\d+\.\d*(?:[eE][-+]?\d+)?|\.\d+(?:[eE][-+]?\d+)?|\d+"
                               r"|[A-Za-z_]\w*|<=|>=|==|!=|\S", text)
        self.pos = 0
        self.env = {}
        self.swizzles = {}
        self.tmp = 0
        self.ret = None

    def peek(self):
        return self.toks[self.pos] if self.pos < len(self.toks) else ""

    def take(self, want=None):
        tok = self.peek()
        if want is not None and tok != want:
            raise SyntaxError("MSL: expected %r, got %r" % (want, tok))
        self.pos += 1
        return tok

    def translate(self):
        if self.peek() == "fragment":
            self.take()
        self.ret = self.take()
        name = self.take()
        self.take("(")
        params = []
        while self.peek() != ")":
            if self.peek() == "constant":
                self.take()
            typ = self.take()
            if typ == "texture2d":
                self.take("<")
                self.take("float")
                self.take(">")
                typ, ctype = "texture", "const texture2d *"
            elif self.peek() in ("&", "*"):
                kind = "ref:" if self.take() == "&" else "ptr:"
                typ, ctype = kind + typ, "const %s *" % typ
            else:
                ctype = typ + " "
            var = self.take()
            self.env[var] = typ
            params.append(ctype + var)
            if self.peek() == ",":
                self.take()
        self.take(")")
        body = self.block(0)
        if self.pos != len(self.toks):
            raise SyntaxError("MSL: trailing text after " + name)
        helpers = "".join(
            "static float%d %s(float%d v) { return v%d(%s); }\n"
            % (len(c), h, n, len(c), ", ".join("v." + x for x in c))
            for h, (n, c) in sorted(self.swizzles.items()))
        return "%sstatic %s %s(%s)\n%s\n" % (helpers, self.ret, name, ", ".join(params), body)

    def block(self, depth):
        """C89 wants declarations first: a declaration after a statement
        opens a nested block running to the end of the MSL one."""
        pad = "   " * depth
        self.take("{")
        out, opened, stmts = [pad + "{"], 0, False
        while self.peek() != "}":
            decl = self.peek() == "float" or self.peek() in self.VECS
            if decl and stmts:
                out.append(pad + "   " * (opened + 1) + "{")
                opened += 1
                stmts = False
            out.append(self.statement(depth + 1 + opened))
            stmts = stmts or not decl
        self.take("}")
        for i in range(opened, 0, -1):
            out.append(pad + "   " * i + "}")
        out.append(pad + "}")
        return "\n".join(out)

    def nested(self, depth):
        return self.statement(depth if self.peek() == "{" else depth + 1)

    def statement(self, depth):
        pad = "   " * depth
        tok = self.peek()
        if tok == "{":
            return self.block(depth)
        if tok == "if":
            self.take()
            self.take("(")
            cond = self.expr()[0]
            self.take(")")
            text = pad + "if (%s)\n%s" % (cond, self.nested(depth))
            if self.peek() == "else":
                self.take()
                text += "\n" + pad + "else\n" + self.nested(depth)
            return text
        if tok == "return":
            self.take()
            code = self.coerce(self.expr(), self.ret)
            self.take(";")
            return pad + "return %s;" % code
        if tok == "float" or tok in self.VECS:
            typ = self.take()
            var = self.take()
            self.take("=")
            code = self.coerce(self.expr(), typ)
            self.take(";")
            self.env[var] = typ
            return pad + "%s %s = %s;" % (typ, var, code)
        lhs = self.unary()
        self.take("=")
        rhs = self.expr()
        self.take(";")
        if len(lhs) < 3:
            raise SyntaxError("MSL: assignment to an rvalue " + lhs[0])
        if lhs[2]:
            base, comps = lhs[2]
            typ = "float%d" % len(comps)
            self.tmp += 1
            t = "msl_tmp%d" % self.tmp
            sets = " ".join("%s.%s = %s.%s;" % (base, c, t, "xyzw"[i])
                            for i, c in enumerate(comps))
            return pad + "{ %s %s = %s; %s }" % (typ, t, self.coerce(rhs, typ), sets)
        return pad + "%s = %s;" % (lhs[0], self.coerce(rhs, lhs[1]))

    def coerce(self, value, typ):
        code, have = value[0], value[1]
        if have == typ or (typ == "float" and have in self.SCALARS):
            return code
        if typ in self.VECS and have in self.SCALARS:
            return "v%ds(%s)" % (self.VECS[typ], code)
        raise TypeError("MSL: %s where %s is needed: %s" % (have, typ, code))

    def expr(self, prec=0):
        left = self.unary()
        while self.peek() in self.PREC and self.PREC[self.peek()] > prec:
            op = self.take()
            left = self.binary(op, left, self.expr(self.PREC[op]))
        return left[:2]

    def binary(self, op, a, b):
        if self.PREC[op] == 10:
            if a[1] not in self.SCALARS or b[1] not in self.SCALARS:
                raise TypeError("MSL: vector comparison")
            return ("(%s %s %s)" % (a[0], op, b[0]), "int")
        width = max(self.VECS.get(a[1], 1), self.VECS.get(b[1], 1))
        if width == 1:
            return ("(%s %s %s)" % (a[0], op, b[0]), "float")
        typ = "float%d" % width
        name = {"+": "add", "-": "sub", "*": "mul", "/": "div"}[op]
        return ("v%d_%s(%s, %s)" % (width, name, self.coerce(a, typ), self.coerce(b, typ)), typ)

    def unary(self):
        if self.peek() == "-":
            self.take()
            code, typ = self.unary()[:2]
            if typ in self.VECS:
                n = self.VECS[typ]
                return ("v%d_sub(v%ds(0.0f), %s)" % (n, n, code), typ)
            return ("(-%s)" % code, typ)
        if self.peek() == "(":
            self.take()
            value = self.expr()
            self.take(")")
            return self.postfix(("(%s)" % value[0], value[1]))
        return self.postfix(self.primary())

    def primary(self):
        tok = self.take()
        if re.match(r"\d|\.\d", tok):
            if "." in tok or "e" in tok.lower():
                return (tok + "f", "float")
            return (tok, "int")
        if self.peek() == "(":
            return self.call(tok, self.args())
        if tok not in self.env:
            raise NameError("MSL: unknown name " + tok)
        return (tok, self.env[tok], None)

    def args(self):
        self.take("(")
        out = []
        while self.peek() != ")":
            out.append(self.expr())
            if self.peek() == ",":
                self.take()
        self.take(")")
        return out

    def call(self, name, args):
        types = [a[1] for a in args]
        if name in self.VECS:
            n = self.VECS[name]
            if len(args) == n and all(t in self.SCALARS for t in types):
                return ("v%d(%s)" % (n, ", ".join(a[0] for a in args)), name)
            if n == 4 and len(args) == 2 and types[0] == "float3" and types[1] in self.SCALARS:
                return ("v4_31(%s, %s)" % (args[0][0], args[1][0]), name)
            if len(args) == 1 and types[0] in self.SCALARS:
                return ("v%ds(%s)" % (n, args[0][0]), name)
            raise TypeError("MSL: %s(%s)" % (name, ", ".join(types)))
        if name == "dot":
            if len(args) != 2 or types[0] != types[1] or types[0] not in self.VECS:
                raise TypeError("MSL: dot(%s)" % ", ".join(types))
            return ("v%d_dot(%s, %s)" % (self.VECS[types[0]], args[0][0], args[1][0]), "float")
        if name in self.ELEMENTWISE:
            arity = {"abs": 1, "sign": 1, "clamp": 3}.get(name, 2)
            if len(args) != arity:
                raise TypeError("MSL: %s takes %d arguments" % (name, arity))
            width = max(self.VECS.get(t, 1) for t in types)
            if width == 1:
                return ("s_%s(%s)" % (name, ", ".join(a[0] for a in args)), "float")
            typ = "float%d" % width
            return ("v%d_%s(%s)" % (width, name, ", ".join(self.coerce(a, typ) for a in args)), typ)
        raise NameError("MSL: unsupported call " + name)

    def postfix(self, value):
        while self.peek() in (".", "["):
            code, typ = value[0], value[1]
            lvalue = len(value) > 2
            if self.take() == "[":
                index = self.expr()
                self.take("]")
                if not typ.startswith("ptr:"):
                    raise TypeError("MSL: indexing " + typ)
                value = ("%s[%s]" % (code, index[0]), typ[4:], None)
                continue
            member = self.take()
            if typ == "texture" and member == "sample":
                args = self.args()
                if len(args) != 2 or args[0][1] != "sampler":
                    raise TypeError("MSL: texture sample arguments")
                value = ("tex_sample(%s, %s, %s)" % (code, args[0][0],
                         self.coerce(args[1], "float2")), "float4")
            elif typ.startswith("ref:") and member in self.FIELDS.get(typ[4:], {}):
                value = ("%s->%s" % (code, member), self.FIELDS[typ[4:]][member], None)
            elif member in self.FIELDS.get(typ, {}):
                value = ("%s.%s" % (code, member), self.FIELDS[typ][member], None)
            elif typ in self.VECS and re.fullmatch(r"[xyzw]{1,4}|[rgba]{1,4}", member):
                comps = "".join("xyzw"["rgba".index(c)] if c in "rgba" else c for c in member)
                n = self.VECS[typ]
                if any("xyzw".index(c) >= n for c in comps):
                    raise TypeError("MSL: .%s on %s" % (member, typ))
                if len(comps) == 1:
                    value = ("%s.%s" % (code, comps), "float") + ((None,) if lvalue else ())
                else:
                    helper = "v%d_%s" % (n, comps)
                    self.swizzles[helper] = (n, comps)
                    value = ("%s(%s)" % (helper, code), "float%d" % len(comps)) \
                        + (((code, comps),) if lvalue else ())
            else:
                raise TypeError("MSL: .%s on %s" % (member, typ))
        return value


def vector_library():
    out = []
    for n in (2, 3, 4):
        out.append("typedef struct float%d { float %s; } float%d;" % (n, ",".join("xyzw"[:n]), n))
    for n in (2, 3, 4):
        f, t = "xyzw"[:n], "float%d" % n

        def each(e):
            return ", ".join(e.replace("#", c) for c in f)
        out.append("%s v%d(%s) { %s r; %s return r; }" % (
            t, n, ", ".join("float " + c for c in f), t, " ".join("r.%s = %s;" % (c, c) for c in f)))
        out.append("%s v%ds(float s) { return v%d(%s); }" % (t, n, n, ", ".join("s" for c in f)))
        for name, op in (("add", "+"), ("sub", "-"), ("mul", "*"), ("div", "/")):
            out.append("%s v%d_%s(%s a, %s b) { return v%d(%s); }" % (t, n, name, t, t, n, each("a.# %s b.#" % op)))
        for name in ("pow", "max", "min"):
            out.append("%s v%d_%s(%s a, %s b) { return v%d(%s); }" % (t, n, name, t, t, n, each("s_%s(a.#, b.#)" % name)))
        for name in ("abs", "sign"):
            out.append("%s v%d_%s(%s a) { return v%d(%s); }" % (t, n, name, t, n, each("s_%s(a.#)" % name)))
        out.append("%s v%d_clamp(%s a, %s lo, %s hi) { return v%d(%s); }" % (
            t, n, t, t, t, n, each("s_clamp(a.#, lo.#, hi.#)")))
        out.append("float v%d_dot(%s a, %s b) { return %s; }" % (n, t, t, " + ".join("a.%s * b.%s" % (c, c) for c in f)))
    out.append("float4 v4_31(float3 a, float w) { return v4(a.x, a.y, a.z, w); }")
    return "\n".join(out)


fragment = MslToC(fragment).translate()
fixture = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <boolean.h>
#include "HDR_ROWS"

float s_pow(float a, float b) { return (float)pow(a, b); }
float s_max(float a, float b) { return a > b ? a : b; }
float s_min(float a, float b) { return a < b ? a : b; }
float s_abs(float a) { return (float)fabs(a); }
float s_sign(float a) { return a > 0 ? 1.0f : a < 0 ? -1.0f : 0.0f; }
float s_clamp(float a, float lo, float hi) { return a < lo ? lo : a > hi ? hi : a; }
VECTORS

typedef struct ViewsOut { float2 texcoord; } ViewsOut;
typedef struct ViewsUBO { float4 clamp; } ViewsUBO;
typedef struct sampler { int unused; } sampler;
typedef struct texture2d { float4 value; } texture2d;

static float4 tex_sample(const texture2d *t, sampler s, float2 p)
{
   (void)s;
   assert(p.x >= 0 && p.x <= 1 && p.y >= 0 && p.y <= 1);
   return t->value;
}

FRAGMENT
static int is_finite(double v) { return v == v && fabs(v) < 1e30; }

int main(void)
{
   static const double colors[8][3] = {{0,0,0},{1,1,1},{1,0,0},{0,1,0},{0,0,1},{0,1,1},{1,0,1},{.23,.58,.91}};
   static const float white[4] = {20,80,203,1000};
   static const double to2020[4][9] = {
      {.627404,.329282,.0433136,.069097,.91954,.0113612,.0163916,.0880132,.895595},
      {.627404,.329282,.0433136,.0457456,.941777,.0124772,-.00121055,.0176041,.983607},
      {.753833,.198597,.047570,.045744,.941777,.012479,-.001210,.017602,.983609},
      {1,0,0,0,1,0,0,0,1}};
   static const double from2020[9] = {1.660491,-.5876411,-.0728499,-.1245505,1.1328999,-.0083494,-.0181508,-.1005789,1.1187297};
   unsigned failures = 0, mode, gamut, w, pixel, i, j;
   ViewsOut in;
   ViewsUBO u;
   sampler s;
   texture2d tex;
   float4 c[4], v;

   in.texcoord = v2(.5f, .5f);
   u.clamp     = v4(0, 0, 1, 1);
   s.unused    = 0;
   for (mode = 1; mode <= 2; mode++)
   for (gamut = 0; gamut < 4; gamut++)
   for (w = 0; w < 4; w++)
   for (pixel = 0; pixel < 8; pixel++)
   {
      float rows[12];
      double expected[3], input[3], encoded[3], linear[3], rgb2020[3];
      c[3] = v4((float)mode, 0, 0, 0);
      hdr_viewer_colour_rows(rows, true, white[w], gamut);
      for (i = 0; i < 3; i++)
      {
         double p;
         c[i] = v4(rows[4*i], rows[4*i+1], rows[4*i+2], rows[4*i+3]);
         expected[i] = 0;
         for (j = 0; j < 3; j++) expected[i] += from2020[3*i+j]*colors[pixel][j]*12.5;
         p = pow(colors[pixel][i]*.1, 2610.0/16384.0);
         input[i] = mode == 1 ? pow((3424.0/4096.0+2413.0/128.0*p)/(1+2392.0/128.0*p), 2523.0/32.0) : expected[i];
      }
      tex.value = v4((float)input[0], (float)input[1], (float)input[2], (float)(pixel%4+1)/4);
      v = views_media(in, &u, c, &tex, s);
      encoded[0] = v.x; encoded[1] = v.y; encoded[2] = v.z;
      for (i = 0; i < 3; i++) linear[i] = (encoded[i] < 0 ? -1.0 : 1.0)*pow(fabs(encoded[i]), 2.4);
      for (i = 0; i < 3; i++)
      {
         rgb2020[i] = 0;
         for (j = 0; j < 3; j++) rgb2020[i] += to2020[gamut][3*i+j]*linear[j]*white[w]/80;
      }
      for (i = 0; i < 3; i++)
      {
         double actual = 0;
         for (j = 0; j < 3; j++) actual += from2020[3*i+j]*rgb2020[j];
         if (!is_finite(actual) || fabs(actual-expected[i]) > .003+.003*fabs(expected[i])) failures++;
      }
      assert(v.w == 1);
   }
   for (i = 0; i < 4; i++) c[i] = v4s(0);
   tex.value = v4(-.2f, .5f, 1.5f, .25f);
   v = views_media(in, &u, c, &tex, s);
   assert(v.x == -.2f && v.y == .5f && v.z == 1.5f && v.w == 1);
   printf("Metal viewer fragment: 256 linear/PQ vectors, all gamuts/white levels, opaque alpha and SDR, %u failures\n", failures);
   return failures ? 1 : 0;
}
'''.replace("HDR_ROWS", (root / "gfx/common/hdr_viewer_colour.h").as_posix()).replace(
    "VECTORS", vector_library()).replace("FRAGMENT", fragment)


def c_compiler():
    """CC, else the C driver beside CXX, which the CI jobs still pass."""
    if os.environ.get("CC"):
        return shlex.split(os.environ["CC"])
    cxx = shlex.split(os.environ.get("CXX", ""))
    if not cxx:
        return ["cc"]
    for cpp, c in (("clang++", "clang"), ("g++", "gcc"), ("c++", "cc")):
        if cxx[0].endswith(cpp):
            return [cxx[0][:-len(cpp)] + c] + cxx[1:]
    return cxx


cc = c_compiler()
cflags = shlex.split(os.environ.get("CFLAGS", os.environ.get("CXXFLAGS", "")))
with tempfile.TemporaryDirectory(prefix="metal-viewer-pixels-") as directory:
    if "--native" in sys.argv:
        begin = source.index('   static NSString *src = @', source.index('static void metal_views_init('))
        end = source.index('   NSError *err      = nil;', begin)
        metal = "".join(json.loads(line.strip().removesuffix(";"))
            for line in source[begin:end].splitlines()[1:] if line.strip())
        shader = Path(directory) / "views.metal"
        shader.write_bytes(metal.encode("utf-8"))
        native = Path(directory) / "native"
        subprocess.run(shlex.split(os.environ.get("OBJC", "clang")) + [
            "-fobjc-arc", "-I" + str(root / "gfx/common"),
            str(Path(__file__).with_name("metal_viewer_native.m")),
            "-o", str(native), "-framework", "Foundation", "-framework", "Metal"], check=True)
        result = subprocess.run([str(native), str(shader)])
        if result.returncode == 77:
            print("SKIP native Metal viewer: no Metal device")
        else:
            result.check_returncode()
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(cc + cflags + [
        "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement",
        "-I" + str(root / "libretro-common/include"), str(unit), "-o", str(exe), "-lm"], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
