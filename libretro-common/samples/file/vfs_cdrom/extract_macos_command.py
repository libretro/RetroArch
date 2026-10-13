"""Compile the actual macOS mapper on test hosts without an Apple SDK.

Only SDK types/calls are stood in by the harness; the function body is
copied verbatim from production, so fixes and regressions are exercised.
"""
from pathlib import Path
import sys

source = Path(__file__).resolve().parents[3] / 'cdrom/cdrom.c'
text = source.read_text()
start = text.index('static int cdrom_send_command_macos(')
end = text.index('/* The classes that actually publish', start)
out = Path(sys.argv[1])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(text[start:end])
