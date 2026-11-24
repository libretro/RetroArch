#!/usr/bin/env python3
"""Token/directive safety and identical compiled GLSL compute programs."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from glsl_minify import decode_embedded, encode_embedded, minify, tokens

ROOT = Path(__file__).resolve().parents[1]


class Minify(unittest.TestCase):
    def test_tokens_and_boundaries(self):
        for text in ('float x = 1e-4 + .5;', 'a + ++b; a-- - b;',
                     'float x = 0x1p-2; uint y = 0xdeadbeefU;',
                     'a / /* comment */ / b;', 'a / /* comment */ * b;',
                     'foo /* comment */ bar', 'void main() { // comment\n return; }'):
            with self.subTest(text=text):
                self.assertEqual(tokens(text), tokens(minify(text)))
        self.assertEqual(minify('void main() { float x = 1.0; }'),
                         'void main(){float x=1.0;}')

    def test_directives(self):
        text = '#version 310 es\n#define F (x)\n#define G(x) x\n#if F\nvoid main() { G(1); }\n#endif\n'
        small = minify(text)
        before = [v for k, v in tokens(text) if k == 'directive']
        after = [v for k, v in tokens(small) if k == 'directive']
        self.assertEqual(before, after)
        self.assertTrue(small.startswith('#version 310 es\n'))

    def test_line_sensitive_source(self):
        for text in ('int x = __LINE__;\n', '#define F(x) \\\n x\nvoid main() { F(1); }'):
            self.assertEqual(text, minify(text))

    def test_embedded_formats(self):
        text = '#version 310 es\nvoid main() {\treturn; }'
        for kind in ('c-string', 'c-bytes'):
            self.assertEqual(decode_embedded(encode_embedded(text, kind)), (text, kind))
        self.assertEqual(decode_embedded('"void\tmain(){}"')[0], 'void\tmain(){}')

    def test_compute_compilation_is_identical(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for path in (ROOT / 'libretro-common/formats/pyrowave/shaders/precompiled').glob('*.comp'):
                with self.subTest(shader=path.name):
                    source = path.read_text(encoding='utf-8')
                    binaries = []
                    for label, text in (('original', source), ('minified', minify(source))):
                        shader = directory / (label + '.comp')
                        output = directory / (label + '.spv')
                        shader.write_text(text, encoding='utf-8')
                        subprocess.run(['glslangValidator', '-G', '-S', 'comp',
                                        '--auto-map-bindings', '--auto-map-locations',
                                        '-o', str(output), str(shader)],
                                       check=True, capture_output=True)
                        binaries.append(output.read_bytes())
                    self.assertEqual(binaries[0], binaries[1])


if __name__ == '__main__':
    unittest.main()
