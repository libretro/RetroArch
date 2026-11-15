"""Build the regression against the shipping reader, without frontend stubs."""
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
start = source.index("void video_driver_cached_frame_read(\n")
end = source.index("\nbool video_driver_cached_frame_is_hw_render(", start)
Path(sys.argv[2]).write_text(source[start:end])
