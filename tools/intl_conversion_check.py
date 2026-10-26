#!/usr/bin/env python3
"""Keep internal label includes out of the translation round trip."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
header = b'MSG_HASH(MENU_ENUM_LABEL_VALUE_MAIN_MENU, "Main Menu")\n'
messages = {'MENU_ENUM_LABEL_VALUE_MAIN_MENU': 'Main Menu'}


def convert(script, argument, cwd):
    return subprocess.check_output([sys.executable, str(root / 'intl' / script),
                                    *argument], cwd=cwd, text=True)


with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'msg_hash_us.h').write_bytes(header)
    (tmp / 'msg_hash_us.json').write_text(json.dumps(messages), encoding='utf-8')
    for nested in (False, True):
        folder = tmp / 'nested' if nested else tmp
        folder.mkdir(exist_ok=True)
        for name in ('msg_hash_lbl', 'msg_hash_lbl_rows'):
            h = folder / (name + '.h')
            j = folder / (name + '.json')
            h.write_bytes(header)
            argument = str(h.relative_to(tmp))
            assert convert('h2json.py', [argument], tmp).strip() == 'Skip'
            assert not j.exists(), argument
            assert h.read_bytes() == header, argument
            j.write_text(json.dumps(messages), encoding='utf-8')
            for filename in (str(j.relative_to(tmp)), str(j)):
                assert convert('json2h.py', [filename], tmp).strip() == 'Skip'
                assert h.read_bytes() == header, filename
            for filename in (str(h.relative_to(tmp)), str(h)):
                assert convert('json2h.py', ['--repack', filename], tmp).strip() == 'Skip'
                assert h.read_bytes() == header, filename

    (tmp / 'msg_hash_us.json').unlink()
    convert('h2json.py', ['msg_hash_us.h'], tmp)
    assert json.loads((tmp / 'msg_hash_us.json').read_text(encoding='utf-8')) == messages
    print('PASS: internal label headers preserved; English translation source still exported')
