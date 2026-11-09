from pathlib import Path
import subprocess,tempfile,shlex,sys,shutil
binary=Path(sys.argv[-1]).resolve();runner=shlex.split(sys.argv[1]) if len(sys.argv)>2 else []
directory=tempfile.mkdtemp(prefix='retroarch-record-file-')
try:
 subprocess.run(runner+[str(binary)],cwd=directory,check=True,timeout=30)
except BaseException:
 print('Retained failed file acceptance in '+directory,flush=True)
 raise
else:
 shutil.rmtree(directory)
