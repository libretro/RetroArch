"""Exercise the shipping Qt event pump with Qt and native window messages."""
from pathlib import Path
import argparse
import os
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=root / "ui/drivers/ui_qt.cpp")
args = parser.parse_args()
source = args.source.read_text(encoding="utf-8")
start = source.find("static bool ui_application_qt_next_message(void)")
if start < 0:
    start = source.index("static void ui_application_qt_process_events(void)")
end = source.index("static void ui_application_qt_quit(void)", start)
compiler = os.environ.get("CXX") or shutil.which("c++")
if not compiler:
    raise SystemExit("A C++ compiler is required")
pkg_config = shutil.which("pkg-config") or shutil.which("pkgconf")
if not pkg_config:
    raise SystemExit("pkg-config or pkgconf is required (MSYS2: install $MINGW_PACKAGE_PREFIX-pkgconf)")
qt = next((name for name in ("Qt6Widgets", "Qt5Widgets")
           if subprocess.run([pkg_config, "--exists", name]).returncode == 0), None)
if not qt:
    raise SystemExit("Qt Widgets development files are required")
flags = shlex.split(subprocess.check_output(
    [pkg_config, "--cflags", "--libs", qt], text=True))
with tempfile.TemporaryDirectory(prefix="qt-modal-") as directory:
    temporary = Path(directory)
    (temporary / "qt_process_events.inc").write_text(source[start:end], encoding="utf-8")
    executable = temporary / ("qt_modal_test.exe" if os.name == "nt" else "qt_modal_test")
    command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-function", "-Wframe-larger-than=4096",
               "-I" + str(temporary), "-I" + str(root / "libretro-common/include"),
               str(Path(__file__).with_name("qt_modal_test.cpp")), "-o", str(executable)]
    if os.name == "nt":
        command += ["-luser32"]
    else:
        command += ["-fPIC"]
    subprocess.run(command + flags, check=True)
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "windows" if os.name == "nt" else "offscreen"
    result = subprocess.run([str(executable)], env=environment, timeout=30)
    if result.returncode != 77:
        result.check_returncode()
