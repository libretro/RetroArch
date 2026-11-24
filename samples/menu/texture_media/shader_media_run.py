"""GPU regression for coreless CLI presets and Qt/external preset changes."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

p = argparse.ArgumentParser()
p.add_argument('--video-driver', choices=('glcore', 'vulkan', 'gl'), default='glcore')
a, extra = p.parse_known_args()
root = Path(__file__).resolve().parents[3]
slang = '''#version 450
layout(push_constant) uniform Push { vec4 SourceSize; vec4 OriginalSize; vec4 OutputSize; uint FrameCount; } params;
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; uint FrameTimeDelta; float OriginalFPS; uint TotalSubFrames; uint CurrentSubFrame; float OriginalAspect; float OriginalAspectRotated; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 uv;
void main() { gl_Position = global.MVP * Position; uv = TexCoord; }
#pragma stage fragment
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main() { FragColor = vec4(COLOUR, 1.0); }
'''
glsl = '''#version 130
#if defined(VERTEX)
in vec4 VertexCoord;
in vec2 TexCoord;
out vec2 uv;
uniform mat4 MVPMatrix;
void main() { gl_Position = MVPMatrix * VertexCoord; uv = TexCoord; }
#elif defined(FRAGMENT)
in vec2 uv;
out vec4 FragColor;
uniform sampler2D Texture;
void main() { FragColor = vec4(COLOUR, 1.0); }
#endif
'''

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

with tempfile.TemporaryDirectory(prefix='media-shaders-') as tmp:
    dst = Path(tmp)
    # Red distinguishes image sampling from the black core framebuffer.
    image = dst / 'red.png'
    image.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 256, 192, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress((b'\0' + b'\xff\0\0' * 256) * 192)) + chunk(b'IEND', b''))
    ext = 'glsl' if a.video_driver == 'gl' else 'slang'
    source = glsl if ext == 'glsl' else slang
    sample = 'texture(Texture, uv)' if ext == 'glsl' else 'texture(Source, uv)'
    if ext == 'slang':
        source = source.replace('FragColor = vec4(COLOUR, 1.0);',
            'bool valid = global.OriginalFPS > 1.0 && global.TotalSubFrames == 1u && global.CurrentSubFrame == 1u && abs(global.OriginalAspectRotated - global.OriginalAspect) < 0.001; FragColor = valid ? vec4(COLOUR, 1.0) : vec4(1.0, 0.0, 1.0, 1.0);')
    for name, colour in [('invert', 'vec3(1.0) - ' + sample + '.rgb'), ('blue', sample + '.bgr')]:
        (dst / (name + '.' + ext)).write_text(source.replace('COLOUR', colour))
        (dst / (name + '.' + ext + 'p')).write_text('shaders = "1"\nshader0 = "' + name + '.' + ext + '"\n')
    subprocess.run([sys.executable, str(root / 'samples/menu/rgui_media/run.py'), str(image),
                    '--external', '--driver', 'ozone', '--video-driver', a.video_driver,
                    '--shader-preset', str(dst / ('invert.' + ext + 'p')),
                    '--shader-reload', str(dst / ('blue.' + ext + 'p'))] + extra, check=True)
