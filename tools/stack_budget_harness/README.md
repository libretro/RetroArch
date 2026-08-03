# stack_budget differential harnesses

Byte-exactness gates for the stack-frame relocation campaign: each
harness decodes (or round-trips) real content through a TU built twice
-- once from the baseline source, once from the patched one -- and
prints FNV-1a hashes of every output byte.  Identical hashes mean the
relocation changed where scratch lives and nothing else.  These caught
one real bug during the campaign (rvorbis step2_flag is indexed by
floor point, not channel) before it was committed; that is the entire
argument for running them.

| harness       | covers                              | content source        |
|---------------|-------------------------------------|-----------------------|
| ropus_diff    | ropus s16+f32, SILK/hybrid/CELT     | ffmpeg -c:a libopus (`-application voip -cutoff 8000` forces true SILK; check the TOC config byte, ffmpeg defaults to hybrid) |
| rflac_diff    | rflac header + raw open paths       | ffmpeg -c:a flac (fixed blocksize) |
| rh264_diff    | rh264 CAVLC/CABAC, I/P/B            | ffmpeg -c:v libx264 -f h264 |
| rhuff_diff    | rhuff dec_build + read_tree_packed  | self-generating (differential fuzz, valid + corrupt + noise) |
| ntsc_diff     | snes_ntsc full-table init           | self-generating (4 setups) |
| final_diff    | -DMODE_VORBIS / _ZSTD / _CONFIG / _MEM | ffmpeg -c:a libvorbis; zstd self round-trips via rzstd_encode |

Usage pattern (see each file's header comment for exact deps):

    git show <base>:path/to/tu.c > /tmp/base_tu.c
    gcc -O2 -I libretro-common/include -o base  harness.c /tmp/base_tu.c [deps]
    gcc -O2 -I libretro-common/include -o new   harness.c path/to/tu.c  [deps]
    ./base fixture; ./new fixture     # hashes must match
    gcc -O1 -g -fsanitize=address,undefined ... && ./asan fixture

Known dep tails: rflac and the crc32 used by rvorbis need
features/features_cpu.c; final_diff MODE_CONFIG needs the file_path /
string_list / file_stream constellation (list in the campaign notes).
The harnesses leak their own decoders by design brevity -- LSan
findings inside the *decoder* matter, findings at harness main() do
not.
