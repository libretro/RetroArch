#!/usr/bin/env python3
"""Run the shipping Vulkan recorder decision with actual configure helpers."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--shell', default='sh')
parser.add_argument('--make', default='make')
parser.add_argument('--source', type=Path)
a = parser.parse_args()
root = Path(__file__).resolve().parents[3]
libs = (root / 'qb/qb.libs.sh').read_text(encoding='utf-8')
helpers = []
for name in ('check_enabled', 'check_platform'):
    match = re.search(r'^' + name + r'\(\)\n\{.*?^\}', libs, re.M | re.S)
    assert match, name
    helpers.append(match[0])
text = (a.source or root / 'qb/config.libs.sh').read_text(encoding='utf-8')
start = text.index("check_platform 'Linux Win32' RECORD_VULKAN")
stop = text.index('check_enabled NETWORKING RTMP', start)
block = text[start:stop]
base = '. qb/qb.init.sh\nadd_opt() { :; }\n' + '\n'.join(helpers) + '\n'
deps = ('VULKAN', 'THREADS', 'CONFIGFILE', 'RMP4', 'RSLC')
count = 0
for platform in ('Linux', 'Win32', 'Darwin', 'BSD', 'Cygwin', 'Haiku'):
    for requested in ('auto', 'yes', 'no'):
        for missing in (None, *deps) if platform in ('Linux', 'Win32') else (None,):
            supported = platform in ('Linux', 'Win32') and missing is None
            expected_error = requested == 'yes' and not supported
            setup = f'OS={platform}\nUSER_RECORD_VULKAN={requested}\nHAVE_RECORD_VULKAN={requested}\n'
            setup += ''.join(f'HAVE_{dep}={"no" if missing == dep else "yes"}\n' for dep in deps)
            result = subprocess.run([a.shell], cwd=root, input=base + setup + block +
                                    '\nprintf "%s\\n" "$HAVE_RECORD_VULKAN"\n',
                                    text=True, capture_output=True)
            assert (result.returncode != 0) == expected_error, (platform, requested, missing, result)
            if not expected_error:
                expected = 'yes' if requested != 'no' and supported else 'no'
                assert result.stdout.strip() == expected, (platform, requested, missing, result.stdout)
                if requested == 'auto' and missing is None:
                    flags = f'OS := {platform}\nHAVE_RECORD_VULKAN := {1 if expected == "yes" else 0}\n'
                    flags += ''.join(f'HAVE_{dep} := 1\n' for dep in deps)
                    probe = flags + 'OBJ :=\ninclude Makefile.common\n.PHONY: probe\nprobe:\n'
                    probe += '\t@echo $(filter record/drivers/record_vulkan.o,$(OBJ))\n'
                    build = subprocess.run([a.make, '--no-print-directory', '-f', '-', 'probe'],
                                           cwd=root, input=probe, text=True, capture_output=True)
                    assert build.returncode == 0, build.stderr
                    assert bool(build.stdout.strip()) == (expected == 'yes'), (platform, build.stdout)
            count += 1
print(f'PASS {count} configure recorder platform/dependency/forced-option decisions')
