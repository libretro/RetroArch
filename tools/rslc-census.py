#!/usr/bin/env python3
"""Census of what the slang-shaders corpus actually uses.

Reads a directory written by samples/gfx/rslc_corpus/rslc_corpus_dump
(per-stage GLSL and SPIR-V plus manifest.txt) and reports, for every
language construct, how many stages use it and which, so the rslc
dialect is the measured union of the corpus rather than a guess.

Two views of each stage, because they catch different things:

  source   the include-expanded GLSL the compiler is handed: directives,
           extensions, #version, types, qualifiers, layouts, precision,
           control flow, builtin functions called, operators that need
           special handling, constructs glslang accepts quietly
  spirv    glslang's module, through spirv-dis: capabilities, extended
           instruction sets, opcodes, decorations, GLSL.std.450 ops,
           image operands.  This is what the SPIR-V ingest reader and
           the emitter have to reproduce.

Only stages whose manifest status is ok are counted; what glslang
rejects today is listed separately and is not part of the dialect.

  tools/rslc-census.py <dumpdir> [-o census.md] [--json census.json]
"""
import collections
import json
import os
import re
import subprocess
import sys

SPIRV_DIS = os.environ.get('SPIRV_DIS', 'spirv-dis')

TYPES = ('void bool int uint float double vec2 vec3 vec4 ivec2 ivec3 ivec4 '
         'uvec2 uvec3 uvec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 mat2 mat3 mat4 '
         'mat2x2 mat2x3 mat2x4 mat3x2 mat3x3 mat3x4 mat4x2 mat4x3 mat4x4 '
         'dmat2 dmat3 dmat4 sampler1D sampler2D sampler3D samplerCube '
         'sampler2DArray sampler2DShadow samplerBuffer texture2D sampler '
         'image2D iimage2D uimage2D atomic_uint int8_t int16_t int64_t '
         'uint8_t uint16_t uint64_t float16_t f16vec2 f16vec3 f16vec4 '
         'i64vec2 u64vec2').split()

QUALIFIERS = ('const in out inout uniform buffer shared attribute varying '
              'flat smooth noperspective centroid sample patch invariant '
              'precise lowp mediump highp coherent volatile restrict '
              'readonly writeonly push_constant std140 std430 '
              'row_major column_major').split()

KEYWORDS = ('if else for while do switch case default break continue '
            'return discard struct precision').split()

BUILTIN_VARS = ('gl_Position gl_FragCoord gl_FragDepth gl_VertexIndex '
                'gl_InstanceIndex gl_PointSize gl_FrontFacing gl_PointCoord '
                'gl_FragColor gl_FragData gl_SampleID gl_NumSamples '
                'gl_ClipDistance gl_HelperInvocation').split()

# Builtin functions glslang knows; anything called that is neither one
# of these nor declared in the stage is an error glslang would have
# raised, so a match here is the whole set.
BUILTIN_FUNCS = set('''
radians degrees sin cos tan asin acos atan sinh cosh tanh asinh acosh atanh
pow exp log exp2 log2 sqrt inversesqrt abs sign floor trunc round
roundEven ceil fract mod modf min max clamp mix step smoothstep isnan
isinf floatBitsToInt floatBitsToUint intBitsToFloat uintBitsToFloat fma
frexp ldexp packUnorm2x16 packSnorm2x16 packUnorm4x8 packSnorm4x8
unpackUnorm2x16 unpackSnorm2x16 unpackUnorm4x8 unpackSnorm4x8
packHalf2x16 unpackHalf2x16 packDouble2x32 unpackDouble2x32 length
distance dot cross normalize faceforward reflect refract matrixCompMult
outerProduct transpose determinant inverse lessThan lessThanEqual
greaterThan greaterThanEqual equal notEqual any all not uaddCarry
usubBorrow umulExtended imulExtended bitfieldExtract bitfieldInsert
bitfieldReverse bitCount findLSB findMSB textureSize textureQueryLod
textureQueryLevels textureSamples texture textureProj textureLod
textureOffset texelFetch texelFetchOffset textureProjOffset
textureLodOffset textureProjLod textureProjLodOffset textureGrad
textureGradOffset textureProjGrad textureProjGradOffset textureGather
textureGatherOffset textureGatherOffsets texture2D texture2DLod
texture2DProj dFdx dFdy fwidth dFdxFine dFdyFine fwidthFine dFdxCoarse
dFdyCoarse fwidthCoarse interpolateAtCentroid interpolateAtSample
interpolateAtOffset noise1 noise2 noise3 noise4 EmitVertex EndPrimitive
barrier memoryBarrier imageLoad imageStore imageSize atomicAdd
atomicMin atomicMax atomicAnd atomicOr atomicXor atomicExchange
atomicCompSwap subgroupAdd mod289 float16BitsToInt16
'''.split())

DIRECTIVE_RE = re.compile(r'^\s*#\s*([A-Za-z_]+)(.*)$', re.M)
IDENT_RE = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
CALL_RE = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)\s*\(')
LAYOUT_RE = re.compile(r'layout\s*\(([^)]*)\)')
EXT_RE = re.compile(r'#\s*extension\s+([A-Za-z_0-9]+)\s*:\s*([a-z]+)')
VERSION_RE = re.compile(r'#\s*version\s+(\d+)(\s+es)?')
NUMBER_RE = re.compile(r'\b(0[xX][0-9a-fA-F]+[uU]?|\d+\.\d*(?:[eE][+-]?\d+)?'
                       r'(?:lf|LF|f|F)?|\.\d+(?:[eE][+-]?\d+)?(?:f|F)?|'
                       r'\d+[eE][+-]?\d+(?:f|F)?|\d+[uU]|\d+(?:f|F|lf|LF))')
SWIZZLE_RE = re.compile(r'\.([xyzw]{1,4}|[rgba]{1,4}|[stpq]{1,4})\b')
OPS = ['<<', '>>', '&&', '||', '^^', '==', '!=', '<=', '>=', '++', '--',
       '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=',
       '?', ':', '%', '&', '|', '^', '~', '!']


def strip_comments(src):
    src = re.sub(r'/\*.*?\*/', lambda m: ' ' * (m.group(0).count('\n')) if False else '\n' * m.group(0).count('\n'), src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    return src


def strip_strings(src):
    return re.sub(r'"[^"\n]*"', '""', src)


class Census:
    def __init__(self):
        self.stages = 0
        self.by_key = collections.defaultdict(set)   # key -> set(stage ids)

    def hit(self, key, stage):
        self.by_key[key].add(stage)


def census_source(path, stage_id, c):
    raw = open(path, encoding='utf-8', errors='replace').read()
    m = VERSION_RE.search(raw)
    if m:
        c.hit('version/' + m.group(1) + (' es' if m.group(2) else ''), stage_id)
    for name, pr in EXT_RE.findall(raw):
        c.hit('extension/%s:%s' % (name, pr), stage_id)

    src = strip_strings(strip_comments(raw))

    # Directives, with their argument shape where it matters.
    for name, rest in DIRECTIVE_RE.findall(src):
        rest = rest.strip()
        if name in ('if', 'elif'):
            if 'defined' in rest:
                c.hit('cpp/%s defined()' % name, stage_id)
            if re.search(r'[<>=!&|+\-*/]', rest):
                c.hit('cpp/%s arithmetic' % name, stage_id)
            c.hit('cpp/' + name, stage_id)
        elif name == 'define':
            if re.match(r'[A-Za-z_]\w*\(', rest):
                c.hit('cpp/define function-like', stage_id)
            else:
                c.hit('cpp/define', stage_id)
            if '##' in rest:
                c.hit('cpp/define token-paste', stage_id)
            if re.search(r'(^|[^#])#[A-Za-z_]', rest):
                c.hit('cpp/define stringize', stage_id)
            if rest.endswith('\\'):
                c.hit('cpp/define multi-line', stage_id)
        elif name == 'pragma':
            c.hit('cpp/pragma ' + (rest.split() or ['?'])[0], stage_id)
        elif name == 'line':
            c.hit('cpp/line' + (' "file"' if '"' in rest else ''), stage_id)
        else:
            c.hit('cpp/' + name, stage_id)

    idents = collections.Counter(IDENT_RE.findall(src))
    for t in TYPES:
        if idents.get(t):
            c.hit('type/' + t, stage_id)
    for q in QUALIFIERS:
        if idents.get(q):
            c.hit('qualifier/' + q, stage_id)
    for k in KEYWORDS:
        if idents.get(k):
            c.hit('keyword/' + k, stage_id)
    for v in BUILTIN_VARS:
        if idents.get(v):
            c.hit('builtin-var/' + v, stage_id)

    declared = set(re.findall(r'\b(?:void|bool|int|uint|float|[iub]?vec[234]|mat[234](?:x[234])?)\s+([A-Za-z_]\w*)\s*\(', src))
    for fn in set(CALL_RE.findall(src)):
        if fn in BUILTIN_FUNCS:
            c.hit('builtin-fn/' + fn, stage_id)
        elif fn in TYPES:
            c.hit('constructor/' + fn, stage_id)
        elif fn in declared:
            c.hit('user-function', stage_id)
    if 'user-function' in [k for k in c.by_key if stage_id in c.by_key[k]]:
        pass
    # overloads: a user function name defined more than once
    defs = collections.Counter(re.findall(r'\b(?:void|bool|int|uint|float|[iub]?vec[234]|mat[234](?:x[234])?)\s+([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{', src))
    if any(n > 1 for n in defs.values()):
        c.hit('user-function overloaded', stage_id)
    if re.search(r'\b(?:in|out|inout)\s+(?:bool|int|uint|float|[iub]?vec[234]|mat[234])\b', src):
        c.hit('user-function in/out params', stage_id)

    for lay in LAYOUT_RE.findall(src):
        for item in lay.split(','):
            key = item.strip().split('=')[0].strip()
            if key:
                c.hit('layout/' + key, stage_id)
    if re.search(r'\bpush_constant\b', src):
        c.hit('block/push_constant', stage_id)
    if re.search(r'\buniform\s+[A-Za-z_]\w*\s*\{', src):
        c.hit('block/uniform', stage_id)
    nblocks = len(re.findall(r'\blayout\s*\([^)]*\)\s*uniform\s+[A-Za-z_]\w*\s*\{', src))
    if nblocks > 2:
        c.hit('block/more-than-two', stage_id)
    if re.search(r'\bstruct\s+[A-Za-z_]\w*\s*\{', src):
        c.hit('struct/definition', stage_id)
    if re.search(r'\b(?:in|out)\s+[A-Z]\w*\s+[A-Za-z_]\w*\s*;', src) and re.search(r'\bstruct\b', src):
        c.hit('struct/varying', stage_id)
    if re.search(r'\b(?:in|out)\s+(?:[A-Za-z_]\w*\s*\{)', src):
        c.hit('block/io-interface-block', stage_id)
    if re.search(r'\[\s*\d+\s*\]\s*(?:=|;|,|\))', src) or re.search(r'\b[A-Za-z_]\w*\s*\[\s*\d*\s*\]\s*(?:=|;)', src):
        c.hit('array/declaration', stage_id)
    if re.search(r'\]\s*\[', src):
        c.hit('array/multidimensional', stage_id)
    if re.search(r'\w+\s*\[\s*[A-Za-z_]\w*', src):
        c.hit('array/dynamic-index', stage_id)
    if re.search(r'\.length\s*\(\s*\)', src):
        c.hit('array/.length()', stage_id)
    if re.search(r'=\s*(?:[iub]?vec[234]|mat[234]|float|int|uint|bool)\s*\[', src):
        c.hit('array/constructor', stage_id)
    if re.search(r'\bconst\s+(?:[iub]?vec[234]|mat[234]|float|int|uint|bool)\s*\[', src):
        c.hit('array/const-array', stage_id)
    if re.search(r'\bswitch\s*\(', src):
        c.hit('control/switch', stage_id)
    if re.search(r'\bfor\s*\([^;]*;[^;]*;[^)]*\)\s*\{?', src):
        c.hit('control/for', stage_id)
    if re.search(r'\bdo\s*\{', src):
        c.hit('control/do-while', stage_id)
    if re.search(r'\bwhile\s*\(', src):
        c.hit('control/while', stage_id)
    if re.search(r'\?', src):
        c.hit('expr/ternary', stage_id)
    for op in OPS:
        if op in src:
            c.hit('op/' + op, stage_id)
    sw = set(SWIZZLE_RE.findall(src))
    if sw:
        c.hit('expr/swizzle', stage_id)
        if any(len(s) != len(set(s)) for s in sw):
            c.hit('expr/swizzle repeated-component', stage_id)
    if re.search(r'\.[xyzwrgbastpq]{1,4}\s*(?:=|\+=|-=|\*=|/=)[^=]', src):
        c.hit('expr/swizzle lvalue', stage_id)
    for n in set(NUMBER_RE.findall(src)):
        if n.lower().startswith('0x'):
            c.hit('literal/hex', stage_id)
        elif n.endswith(('u', 'U')):
            c.hit('literal/uint', stage_id)
        elif n.lower().endswith('lf'):
            c.hit('literal/double', stage_id)
        elif 'e' in n.lower():
            c.hit('literal/exponent', stage_id)
    if re.search(r'\bprecision\s+(?:lowp|mediump|highp)\s+', src):
        c.hit('precision/default-statement', stage_id)
    if re.search(r'\b(?:lowp|mediump|highp)\s+(?:[iub]?vec[234]|mat[234]|float|int|uint|sampler2D)', src):
        c.hit('precision/on-declaration', stage_id)
    if re.search(r'\bconst\s+(?:[iub]?vec[234]|mat[234]|float|int|uint|bool)\s+[A-Za-z_]\w*\s*=', src):
        c.hit('const/global-or-local', stage_id)
    if re.search(r'\b(?:[iub]?vec[234]|mat[234]|float|int|uint|bool)\s+[A-Za-z_]\w*\s*(?:,\s*[A-Za-z_]\w*\s*)+;', src):
        c.hit('decl/multiple-declarators', stage_id)
    if re.search(r'\b(?:[iub]?vec[234]|mat[234]|float|int|uint|bool)\s+[A-Za-z_]\w*\s*=[^;]*,\s*[A-Za-z_]\w*\s*=', src):
        c.hit('decl/multiple-declarators-initialised', stage_id)
    if re.search(r'\bfor\s*\(\s*(?:int|uint|float)\s+', src):
        c.hit('decl/for-init-declaration', stage_id)
    if re.search(r'\bmat[234]\s*\(\s*(?:[iub]?vec|float|mat)', src):
        c.hit('constructor/matrix-from-components', stage_id)
    if re.search(r'\bmat[234]\w*\s+\w+\s*=\s*mat[234]', src):
        c.hit('constructor/matrix', stage_id)
    if re.search(r'\bmat[234]\w*\s*\*\s*[iub]?vec|\bvec[234]\s*\*\s*mat', src):
        c.hit('expr/matrix-vector-multiply', stage_id)
    if re.search(r'\[\s*\d+\s*\]\s*\[\s*\d+\s*\]', src):
        c.hit('expr/matrix-element', stage_id)
    if re.search(r'\bvec[234]\s*\(\s*[A-Za-z_]\w*\.[xyzw]{2,}\s*,', src):
        c.hit('constructor/vector-from-mixed', stage_id)
    if re.search(r'\b(?:int|uint|float|bool)\s*\(\s*[A-Za-z_]', src):
        c.hit('constructor/scalar-cast', stage_id)
    if re.search(r'\bsampler2D\b', src) and re.search(r'\bin\s+vec4\s+Position\b', src):
        c.hit('spec/sampler-in-vertex', stage_id)
    if re.search(r'\bgl_FragCoord\b', src):
        c.hit('spec/gl_FragCoord', stage_id)


DIS_INST_RE = re.compile(r'^\s*(?:%\w+\s*=\s*)?(Op\w+)\s*(.*)$')


def census_spirv(path, stage_id, c):
    try:
        out = subprocess.run([SPIRV_DIS, '--raw-id', path], capture_output=True,
                             text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        c.hit('spirv/dis-failed', stage_id)
        return
    for line in out.splitlines():
        m = DIS_INST_RE.match(line)
        if not m:
            continue
        op, args = m.group(1), m.group(2)
        c.hit('spv-op/' + op, stage_id)
        if op == 'OpCapability':
            c.hit('spv-cap/' + args.strip(), stage_id)
        elif op == 'OpExtInstImport':
            c.hit('spv-extset/' + args.strip(), stage_id)
        elif op == 'OpExtInst':
            parts = args.split()
            if len(parts) >= 3:
                c.hit('spv-std450/' + parts[2], stage_id)
        elif op in ('OpDecorate', 'OpMemberDecorate'):
            parts = args.split()
            idx = 1 if op == 'OpDecorate' else 2
            if len(parts) > idx:
                c.hit('spv-dec/' + parts[idx], stage_id)
        elif op == 'OpExecutionMode':
            parts = args.split()
            if len(parts) > 1:
                c.hit('spv-mode/' + parts[1], stage_id)
        elif op == 'OpTypeImage':
            c.hit('spv-image/' + ' '.join(args.split()[1:]), stage_id)
        elif op.startswith('OpImage'):
            # image operands mask names after the coordinate
            for tok in ('Bias', 'Lod', 'Grad', 'ConstOffset', 'Offset',
                        'ConstOffsets', 'Sample', 'MinLod'):
                if re.search(r'\b%s\b' % tok, args):
                    c.hit('spv-imgop/' + tok, stage_id)
        elif op == 'OpLoopMerge' or op == 'OpSelectionMerge':
            parts = args.split()
            if len(parts) > 1 and op == 'OpLoopMerge':
                c.hit('spv-loopctl/' + parts[2] if len(parts) > 2 else 'spv-loopctl/None', stage_id)


def load_manifest(dumpdir):
    ok, bad = [], []
    for line in open(os.path.join(dumpdir, 'manifest.txt')):
        parts = line.rstrip('\n').split(' ', 2)
        if len(parts) < 3:
            continue
        status, name, path = parts
        (ok if status == 'ok' else bad).append((status, name, path))
    return ok, bad


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    dumpdir = argv[1]
    out_md = 'rslc-census.md'
    out_json = None
    i = 2
    while i < len(argv):
        if argv[i] == '-o':
            out_md = argv[i + 1]; i += 2
        elif argv[i] == '--json':
            out_json = argv[i + 1]; i += 2
        else:
            i += 1

    ok, bad = load_manifest(dumpdir)
    c = Census()
    for status, name, path in ok:
        for stage, ext in (('vert', 'vert'), ('frag', 'frag')):
            sid = '%s:%s' % (name, stage)
            c.stages += 1
            census_source(os.path.join(dumpdir, '%s.%s.glsl' % (name, ext)), sid, c)
            spv = os.path.join(dumpdir, '%s.%s.spv' % (name, ext))
            if os.path.exists(spv):
                census_spirv(spv, sid, c)

    groups = collections.defaultdict(list)
    for key, stages in c.by_key.items():
        group, _, item = key.partition('/')
        groups[group].append((item, len(stages), sorted(stages)))

    order = ['version', 'extension', 'cpp', 'type', 'qualifier', 'layout',
             'block', 'struct', 'array', 'precision', 'keyword', 'control',
             'decl', 'const', 'expr', 'op', 'literal', 'constructor',
             'builtin-var', 'builtin-fn', 'user-function', 'spec',
             'spv-cap', 'spv-extset', 'spv-mode', 'spv-dec', 'spv-image',
             'spv-imgop', 'spv-loopctl', 'spv-std450', 'spv-op', 'spirv']
    with open(out_md, 'w') as f:
        f.write('# rslc corpus census\n\n')
        f.write('Dump: `%s`. %d shaders compile with glslang '
                '(%d stages); %d do not and are excluded.\n\n'
                % (dumpdir, len(ok), c.stages, len(bad)))
        f.write('Every row is a construct, the number of stages that use '
                'it, and up to three example stages. A construct with one '
                'stage is still in the dialect: that shader works today.\n\n')
        for g in order + sorted(set(groups) - set(order)):
            if g not in groups:
                continue
            f.write('## %s\n\n| construct | stages | examples |\n|---|---:|---|\n' % g)
            for item, n, ex in sorted(groups[g], key=lambda t: (-t[1], t[0])):
                f.write('| `%s` | %d | %s |\n' % (item.replace('|', '\\|'), n,
                        ', '.join(ex[:3]).replace('|', '\\|')))
            f.write('\n')
        f.write('## rejected by glslang today (not part of the dialect)\n\n')
        for status, name, path in bad:
            f.write('- %s: `%s`\n' % (status, path))
    if out_json:
        with open(out_json, 'w') as f:
            json.dump({k: sorted(v) for k, v in c.by_key.items()}, f, indent=1, sort_keys=True)
    print('%d shaders ok, %d rejected, %d stages, %d constructs -> %s'
          % (len(ok), len(bad), c.stages, len(c.by_key), out_md))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
