"""Exercise the shipping driver loader expression in the preview harness."""
from pathlib import Path
import re
source = Path(__file__).resolve().parents[3] / "audio/audio_driver.c"
text = source.read_text(encoding="utf-8")
case = text.split("case AUDIO_MIXER_TYPE_M4A:", 1)[1].split("break;", 1)[0]
call = re.search(r"handle\s*=\s*(audio_mixer_load_m4a[^;]+);", case)
if not call:
    raise SystemExit("MP4 mixer loader expression missing")
Path(__file__).with_name("preview_m4a_load.h").write_text(
    "sound = " + call.group(1).replace("params->", "p->").replace("(buf,", "(p->buf,") + ";\n", encoding="utf-8")
