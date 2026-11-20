#!/usr/bin/env python3
"""Offline apt-install.sh regression checks; never invokes sudo or a real APT."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FAKE = r'''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
root=Path(os.environ['APT_TEST_ROOT'])
name=Path(sys.argv[0]).name
args=sys.argv[1:]
if name=='sudo':
    if args[0]=='chown':sys.exit(0)
    args=[str(root/'system-lists')+a[len('/var/lib/apt/lists'):] if a.startswith('/var/lib/apt/lists') else a for a in args]
    os.execvp(args[0],args)
if name=='sleep':sys.exit(0)
if name=='dpkg':
    if args==['--print-architecture']:print('amd64');sys.exit(0)
    sys.exit(0 if os.environ['APT_TEST_CASE']=='present' else 1)
statefile=root/'state.json'
state=json.loads(statefile.read_text()) if statefile.exists() else {'events':[],'updates':0,'downloads':0}
case=os.environ['APT_TEST_CASE']
state['events'].append(args)
lists=Path(next((a.split('=',1)[1] for a in args if a.startswith('Dir::State::Lists=')),str(root/'cache/lists')))
rc=0
if args[0]=='update':
    state['updates']+=1
    if 'APT::Update::Error-Mode=any' not in args:raise SystemExit('partial updates must be errors')
    if (lists/'stale-index').exists():raise SystemExit('mirror change retained stale indexes')
    if case=='update-fails' or (case=='fallback-update-fails' and state['downloads']):rc=100
    else:(lists/'index').write_text('fresh')
elif '--no-download' in args:
    rc=0 if case in ('warm','warm-main') else 100
elif '--download-only' in args:
    state['downloads']+=1
    if case in ('fallback','fallback-update-fails') and state['downloads']==1:
        (lists/'stale-index').write_text('interrupted mirror')
        rc=100
else:
    if case=='install-fails':rc=100
statefile.write_text(json.dumps(state))
sys.exit(rc)
'''


def run_case(case, deb822=False):
    with tempfile.TemporaryDirectory(prefix='retroarch-apt-test-') as directory:
        folder = Path(directory)
        (folder / 'bin').mkdir()
        (folder / 'system-lists').mkdir()
        (folder / 'sources.d').mkdir()
        source = folder / 'sources.list'
        if deb822:
            source = folder / 'sources.d' / 'ubuntu.sources'
            source.write_text('Types: deb\nURIs: mirror+file:/etc/apt/apt-mirrors.txt\nSuites: noble noble-updates\nComponents: main universe\n')
        else:
            source.write_text('deb http://azure.archive.ubuntu.com/ubuntu noble main\n')
        fake = folder / 'fake.py'
        fake.write_text(FAKE)
        fake.chmod(0o755)
        for name in ('sudo', 'apt-get', 'dpkg', 'sleep'):
            (folder / 'bin' / name).symlink_to(fake)
        cache = folder / 'cache'
        lists = cache / 'lists'
        lists.mkdir(parents=True)
        if case in ('warm', 'warm-main'):
            (lists / '.complete-v2').touch()
            if case == 'warm-main': (lists / '.main-archive').touch()
        else:
            # Old caches retain archives but must never trust their indexes.
            (lists / '.complete').touch()
            (lists / 'stale-index').touch()
        env = dict(os.environ, PATH=str(folder / 'bin') + ':' + os.environ['PATH'],
            APT_TEST_ROOT=str(folder), APT_TEST_CASE=case, APT_CACHE=str(cache),
            APT_SOURCES_LIST=str(source), APT_SOURCES_DIR=str(folder / 'sources.d'),
            GITHUB_ENV=str(folder / 'github-env'))
        process = subprocess.run(['sh', str(ROOT / '.github/workflows/scripts/apt-install.sh'),
            'build-essential', 'libegl-dev'], env=env, capture_output=True, text=True, timeout=20)
        statefile = folder / 'state.json'
        state = json.loads(statefile.read_text()) if statefile.exists() else {'events': [], 'updates': 0, 'downloads': 0}
        success = case in ('present', 'warm', 'warm-main', 'cold', 'fallback')
        assert (process.returncode == 0) == success, (case, process.stdout, process.stderr)
        if case == 'present':
            assert not state['events']
        elif case in ('warm', 'warm-main'):
            assert state['updates'] == state['downloads'] == 0
            if case == 'warm-main': assert 'https://archive.ubuntu.com/ubuntu' in source.read_text()
        elif case == 'fallback':
            assert state['updates'] == state['downloads'] == 2
            assert 'https://archive.ubuntu.com/ubuntu' in source.read_text()
        elif case == 'fallback-update-fails':
            assert state['downloads'] == 1, 'downloads continued with unusable indexes'
        elif case == 'update-fails':
            assert state['downloads'] == 0
        if not success:
            assert not (lists / '.complete-v2').exists()
            assert not (folder / 'github-env').exists()
        print('PASS: apt-install', case, 'Deb822' if deb822 else 'sources.list')


if __name__ == '__main__':
    for case in ('present', 'warm', 'warm-main', 'cold', 'fallback', 'update-fails', 'fallback-update-fails', 'install-fails'):
        run_case(case)
        run_case(case, deb822=True)
