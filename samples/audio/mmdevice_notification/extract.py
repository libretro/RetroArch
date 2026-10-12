from pathlib import Path
import re

here = Path(__file__).resolve().parent
parts = []
for driver in ('xaudio', 'dsound'):
    source = (here.parents[2] / f'audio/drivers/{driver}.c').read_text()
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    start = source.index(f'static bool {driver}_imm_start_thread(')
    end = masked.index('{', start) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    parts.append(source[start:end])
(here / 'driver_start.inc').write_text('\n\n'.join(parts) + '\n')
