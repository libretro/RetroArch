"""Run a core's snapshot profiles with the shared public-libretro test host."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True, type=Path)
parser.add_argument('--core', required=True, type=Path)
parser.add_argument('--content', required=True, type=Path)
parser.add_argument('--suite', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path, help='new directory')
parser.add_argument('--system', type=Path, help='existing firmware directory, read only')
args = parser.parse_args()
host, core, content, suite_path, output = (
    value.resolve() for value in (args.host,args.core,args.content,args.suite,args.output))
suite = json.loads(suite_path.read_text(encoding='utf-8'))
profile_path = suite_path.parent / suite['profile']
options = {}
for line in profile_path.read_text(encoding='utf-8').splitlines():
    if not line or line.startswith('#'):
        continue
    key, value = line.split('=',1)
    if key in options:
        parser.error('duplicate profile option: ' + key)
    options[key] = value
names = [variant['name'] for variant in suite['variants']]
if not names or len(set(names)) != len(names) or any(
        not name or name in ('.','..') or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-_' for c in name)
        for name in names):
    parser.error('variant names must be unique lowercase file names')
output.mkdir(parents=True,exist_ok=False)
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
report = dict(host_sha256=digest(host), core_sha256=digest(core),
              content_sha256=digest(content), suite_sha256=digest(suite_path),
              profile_sha256=digest(profile_path), runs=[])
for variant in suite['variants']:
    settings = dict(options)
    settings.update(variant.get('options',{}))
    profile = output / (variant['name'] + '.cfg')
    profile.write_text(''.join(key+'='+value+'\n' for key,value in settings.items()),encoding='utf-8')
    command = [str(host),str(core),str(output/variant['name']),str(profile),str(content)]
    if args.system:command.append(str(args.system.resolve(strict=True)))
    with (output/(variant['name']+'.log')).open('wb') as log:
        try:
            result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    report['runs'].append(dict(name=variant['name'],options=settings,command=command,exit_code=code))
    (output/'results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(variant['name'], 'PASS' if code == 0 else 'FAIL',flush=True)
    if code != 0:
        raise SystemExit('See ' + str(output/(variant['name']+'.log')))
    if variant.get('equal_to'):
        for name in ['final-ram.bin','final-rgb.bin']:
            assert digest(output/variant['name']/name)==digest(output/variant['equal_to']/name),(variant['name'],name)
    if variant.get('counter_delta'):
        def counter(name):
            return int(dict(line.split('=',1) for line in (output/name/'checks.txt').read_text().splitlines())['counter'])
        other,delta=variant['counter_delta']
        assert counter(variant['name'])-counter(other)==delta,(variant['name'],'native prediction depth differs')
