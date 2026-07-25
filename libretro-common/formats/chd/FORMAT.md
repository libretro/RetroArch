# CHD container format

Working description of MAME's Compressed Hunks of Data container, written
to support `rchd.c`.

## Provenance

Every field below, with one marked exception, is derived from
**observation of generated files** rather than from reading an existing
implementation. Reference images are produced
with `chdman` from known input, and each claim is stated as a prediction
that the observed bytes then confirm or refute. `tools/chd/chd_probe.py`
reproduces the whole set.

One subsection is an exception and says so: §9.4, the framing of the A/V
video bitstream, was read from the reference implementation after
measurement fixed the transform but not the layout. It is marked in
place rather than left to be inferred.

This matters for two reasons. It keeps the implementation clean-room:
`rchd.c` is written against this document, and every constant in it can
be traced to a measurement rather than to someone's recollection. And it
is falsifiable — two entries below were wrong on the first pass and the
probe caught them.

Reference writer: `chdman` 0.264. That build predates the Zstandard
codecs, so `zstd` and `cdzs` were read off archived images written by a
later one rather than generated here.

Generated images cannot cover everything: they are all version 5, all
written by one tool version, and small. Claims are therefore also checked
against archived images produced by other tool versions over the years —
four CD images and two hard-disk images at the time of writing. Those are
what versions 1 to 4 and the CD codecs can be observed at all, and they
are the standing check that the description describes the format rather
than one writer's habits.

Status legend: **[V]** verified against observed bytes, **[P]** predicted
but not yet confirmed, **[?]** unknown.

---

## 1. Header

All integers are big-endian. All offsets are absolute byte offsets from
the start of the file unless stated otherwise.

### 1.1 Version 5 header — 124 bytes **[V]**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 8 | tag | ASCII `MComprHD` |
| 8 | 4 | length | 124 |
| 12 | 4 | version | 5 |
| 16 | 16 | compressors[4] | four 4CC tags, `0` for unused slots |
| 32 | 8 | logicalbytes | decompressed image size |
| 40 | 8 | mapoffset | start of the hunk map |
| 48 | 8 | metaoffset | first metadata entry, `0` if none |
| 56 | 4 | hunkbytes | |
| 60 | 4 | unitbytes | |
| 64 | 20 | rawsha1 | SHA-1 of the decompressed image |
| 84 | 20 | sha1 | SHA-1 of image plus metadata |
| 104 | 20 | parentsha1 | zero when there is no parent |

Confirmed by generating images from input whose SHA-1 was known in
advance and reading `rawsha1` back at offset 64 across five codec
configurations. The `compressors` array read back as exactly the tags
requested, in request order, for a four-codec image.

**An uncompressed image carries no hashes.** With `-c none`, `rawsha1`,
`sha1` and `parentsha1` are all zero. **[V]** A reader must not treat a
zero hash as a verification failure, and must not offer verification for
such an image at all — there is nothing to verify against.

### 1.2 Version 3 header — 120 bytes **[V]**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 8 | tag | ASCII `MComprHD` |
| 8 | 4 | length | 120 |
| 12 | 4 | version | 3 |
| 16 | 4 | flags | 0 observed |
| 20 | 4 | compression | an enum, not a 4CC; 2 observed, meaning zlib |
| 24 | 4 | totalhunks | |
| 28 | 8 | logicalbytes | |
| 36 | 8 | metaoffset | |
| 44 | 16 | md5 | |
| 60 | 16 | parentmd5 | zero when there is no parent |
| 76 | 4 | hunkbytes | |
| 80 | 20 | sha1 | |
| 100 | 20 | parentsha1 | |

Confirmed against two archived hard-disk images by cross-reading with an
independent tool: the SHA-1 at offset 80 matches the hash that tool
reports, and `totalhunks`, `logicalbytes` and `hunkbytes` all agree.

**There is no `unitbytes` field before version 5.** A reader that needs a
unit size must infer it. For the observed hard-disk images the value an
independent tool reports (512) is the `BPS` field of the `GDDD` metadata,
so the inference runs through metadata rather than through any header
field. Images carrying no metadata that names a unit size need a fallback
policy, which is **[?]**.

The `compression` field is an enum here rather than the four 4CC slots of
version 5, so the whole image uses one codec. Only value 2 has been
observed.

### 1.3 Version 4 header — 108 bytes **[V]**

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | tag |
| 8 | 4 | length — 108 |
| 12 | 4 | version — 4 |
| 16 | 4 | flags |
| 20 | 4 | compression enum |
| 24 | 4 | totalhunks |
| 28 | 8 | logicalbytes |
| 36 | 8 | metaoffset |
| 44 | 4 | hunkbytes |
| 48 | 20 | sha1 |
| 68 | 20 | parentsha1 |
| 88 | 20 | rawsha1 |

Version 4 drops version 3's two MD5 fields and adds `rawsha1`.

### 1.4 Versions 1 and 2 — 76 and 80 bytes **[V]**

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | tag |
| 8 | 4 | length — 76 or 80 |
| 12 | 4 | version |
| 16 | 4 | flags |
| 20 | 4 | compression enum |
| 24 | 4 | hunk size, **counted in sectors** |
| 28 | 4 | totalhunks |
| 32 | 4 | cylinders |
| 36 | 4 | heads |
| 40 | 4 | sectors |
| 44 | 16 | md5 |
| 60 | 16 | parentmd5 |
| 76 | 4 | sector length — **version 2 only** |

Neither version stores `logicalbytes` or `hunkbytes` directly. Both are
derived: sector length is 512 in version 1 and read from offset 76 in
version 2, `hunkbytes` is the field at 24 multiplied by it, and
`logicalbytes` is `cylinders * heads * sectors * seclen`.

The geometry fields are what distinguish these versions structurally, and
a reader must treat them as load-bearing rather than informational: they
are the only source of the image's size.

### 1.5 How versions 1, 2 and 4 were verified

No archived sample of these versions was available, and no current tool
writes them. They were verified by building images from the layouts above
and having independent readers decode them:

- Version 4 was accepted by both readers, extracted byte-identically, and
  reported "raw SHA-1 verification successful".
- Versions 1 and 2 were accepted by libchdr and decoded to a digest
  identical to a version 5 image of the same source data.

Writing a file a reader accepts is weaker evidence than reading a file
someone else wrote — a shared misunderstanding would go unnoticed — but
two independent readers agreeing on content that round-trips to the
original is strong enough to act on. These stay marked **[V]** with that
caveat, and archived samples would still be worth checking against.

**No current MAME tool reads versions 1 or 2 at all.** They are rejected
exactly as an unknown future version would be. libchdr does read them,
so supporting them is a compatibility requirement inherited from libchdr
rather than from upstream.

---

## 2. Hunk map

`hunkcount = ceil(logicalbytes / hunkbytes)`.

### 2.1 Uncompressed map **[V]**

Present when all four compressor slots are zero. `mapoffset` points just
past the header, at 124. The map is a flat array of `hunkcount`
big-endian `uint32` values, one per hunk, and nothing else — no header,
no checksum.

Resolution rule, confirmed against all 64 hunks of a reference image:

- **value `0` means a hole**: the hunk reads as `hunkbytes` of zero. It
  does **not** mean offset zero.
- **any other value** is a byte offset of `value * hunkbytes`.

The hole case is easy to miss and fails quietly. Reading value `0` as an
offset yields the first `hunkbytes` of the file — the header itself —
which is well-formed data of the right length that happens to be wrong.
The reference image exercises this: its hunk 0 is genuinely all zeros,
its map entry is `0`, and its hunk data region begins at offset 0, so the
naive reading and the correct one differ only in content.

Because offsets are counted in whole hunks from zero, the header and map
occupy space inside the region hunk 0 would otherwise address. A writer
therefore cannot place a real hunk at index 0; observed images only ever
emit `0` there as a hole.

### 2.2 Compressed map header — 16 bytes **[V]**

Present when any compressor slot is set. `mapoffset` points at this
header, and the compressed bitstream follows immediately at
`mapoffset + 16`.

| Offset | Size | Field | Observed |
|---|---|---|---|
| 0 | 4 | maplength | length of the compressed bitstream, excluding this header |
| 4 | 6 | datastart | offset of the first hunk blob — 124 in every image observed |
| 10 | 2 | crc16 | CRC-16 over the decoded map |
| 12 | 1 | lengthbits | bit width of a compressed-length field |
| 13 | 1 | hunkbits | bit width of a self-reference |
| 14 | 1 | parentunitbits | bit width of a parent reference |
| 15 | 1 | reserved | zero in every image observed |

For a 64-hunk image at 4096 bytes per hunk with `zlib`: `maplength` 219,
`datastart` 124, `lengthbits` 13, `hunkbits` 0, `parentunitbits` 0. The
widths are sized to the image: `lengthbits` 13 admits lengths to 8191,
the smallest power-of-two bound above `hunkbytes`; `hunkbits` and
`parentunitbits` are zero because that image contains no self or parent
references.

**The compressed map is the last structure in the file.** In every
compressed image observed, `mapoffset + 16 + maplength` equals the file
size exactly. **[V]**

This has a direct consequence for readers. A compressed image cannot
resolve a single hunk until the tail of the file is available, so
consuming one as a sequential stream — decoding from a growing prefix as
it downloads — is not possible. Random access to at least the tail is a
hard requirement, not an optimisation.

### 2.3 Compressed map bitstream **[V]**

The body begins at `mapoffset + 16` and is read most-significant-bit
first. It carries a tree, then two passes over the remaining stream —
the second continues where the first stops, with no alignment between
them.

#### 2.3.1 Tree

A canonical Huffman tree of 16 codes, at most 8 bits, serialised as in
§2.3.2. Codes are assigned **longest length first**, which is not RFC
1951's ordering; see §5.

#### 2.3.2 Tree serialisation

Code lengths are stored in order, each as a fixed-width value. The width
is 4 bits for a tree of at most 8-bit codes — the only case this
serialisation is used for, since the map's tree is its only user.

A stored value of 1 is an escape, not a length:

- escape followed by 1 — a single code of length 1.
- escape followed by any other value *v*, then a count *c* — the next
  *c* + 3 codes all have length *v*.
- any other value is a length, for one code.

Reading stops once every code has a length.

#### 2.3.3 Pass one — a reference code per hunk

One symbol per hunk. Two of the sixteen are escapes that repeat the
previous hunk's code rather than naming their own:

| Code | Meaning |
|---|---|
| 0–3 | compressed with `compressors[code]` |
| 4 | stored uncompressed |
| 5 | self reference, target read in pass two |
| 6 | parent reference, target read in pass two |
| 7 | repeat previous, count 2 + next symbol |
| 8 | repeat previous, count 18 + (next << 4) + next |
| 9 | self reference to the same target as the last one |
| 10 | self reference to the last target + 1 |
| 11 | parent reference at this hunk's own unit position |
| 12 | parent reference to the same target as the last one |
| 13 | parent reference to the last target + `hunkbytes / unitbytes` |

Codes 14 and 15 were not observed.

#### 2.3.4 Pass two — the fields each code implies

Continuing the same stream, with a running offset that starts at
`datastart`:

- **0–3**: `length` = read(`lengthbits`), `crc` = read(16). Offset is the
  running offset, which then advances by `length`.
- **4**: `length` = `hunkbytes`, `crc` = read(16). Same advance.
- **5**: offset = read(`hunkbits`); becomes the last self target.
- **6**: offset = read(`parentbits`); becomes the last parent target.
- **9, 10, 11, 12, 13**: consume no bits; the offset comes from the
  running self or parent target as described above, and the code is
  rewritten to 5 or 6.

#### 2.3.5 Decoded entry — 12 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | reference code, escapes already resolved to 5 or 6 |
| 1 | 3 | length |
| 4 | 6 | offset |
| 10 | 2 | CRC-32 of the hunk, truncated to 16 bits as stored |

#### 2.3.6 Validation

CRC-16/CCITT-FALSE — polynomial 0x1021, initial 0xffff, no reflection,
no final xor — over `hunkcount * 12` decoded bytes must equal the `crc16`
in the map header.

A second check is available and worth applying, because it catches
different mistakes: the offsets of every compressed and uncompressed
hunk must partition `[datastart, mapoffset)` exactly, with no gap and no
overlap. A wrong bit width usually still produces a decodable stream but
almost never a partition.

Both were confirmed on seven images spanning `zlib`, `lzma`, `huff`,
`flac`, a four-codec image, a parent-differenced image and one built to
force self references. Between them those exercise codes 0, 4, 5, 9, 10
and 11.

### 2.4 Version 3 map entries — 16 bytes each **[V]**

The map begins immediately after the header, at offset 120, and holds
`totalhunks` entries with no header of its own and no checksum.

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | offset of the hunk's data |
| 8 | 4 | CRC-32 of the decompressed hunk |
| 12 | 2 | length, low 16 bits |
| 14 | 1 | length, high 8 bits |
| 15 | 1 | flags; the low four bits are the entry type |

Length is therefore 24 bits, assembled from a 16-bit and an 8-bit field
rather than stored as one value.

Entry types, from the low nibble of the flags byte:

| Type | Meaning | Observed |
|---|---|---|
| 0 | invalid | no |
| 1 | compressed | yes |
| 2 | stored uncompressed | yes |
| 3 | mini — the hunk is a repeat of the 8 bytes in the offset field | yes |
| 4 | self reference | yes |
| 5 | parent reference | no |
| 6 | compressed with a secondary codec | no |

Types 5 and 6 are **[P]**: the archived images carry no parent and no
secondary codec.

#### Map terminator **[V]**

The entry array of a version 1 to 4 map is followed by one further
entry-sized slot holding the ASCII string `EndOfListCookie` — eight bytes
for versions 1 and 2, sixteen padded with a NUL for versions 3 and 4.
Hunk data or metadata begins after it.

This is easy to miss because nothing points at it: it is found by
computing where the map ends. It also accounts for the otherwise
unexplained gap between the end of a real image's map and its
`metaoffset`. One of the two readers requires it and rejects an image
without it; the other does not check. Writing it is therefore mandatory
and checking it is optional, so a reader should tolerate its absence.

#### Version 1 and 2 map entries — 8 bytes each **[V]**

A single big-endian 64-bit value per hunk, packing two fields:

- **bits 0-43** — byte offset of the hunk's data.
- **bits 44-63** — length of that data.

There is no type field and no checksum. A hunk is stored uncompressed
when its length equals `hunkbytes`, and compressed otherwise; that
comparison is the only signal available.

#### Verification

Confirmed the same way as §2.3: across 31,904 and 71,221 data-bearing
entries in two images, the offsets and lengths partition the file from
the end of the metadata to EOF with **no gap and no overlap**. The high
bits of the flags byte were not exercised and are **[?]**.

---

## 3. Codec tags **[V]**

Read back verbatim from the `compressors` array of generated images.

| Tag | Meaning |
|---|---|
| `zlib` | raw DEFLATE |
| `lzma` | LZMA, properties not stored |
| `huff` | static-tree Huffman |
| `flac` | FLAC, headerless |
| `cdzl` `cdlz` `cdfl` | the above with CD framing |
| `zstd` `cdzs` | Zstandard, and CD-framed |
| `avhu` | audio/video, version 5 |

Versions 1 to 4 use a small enum in the header rather than four tags, so
one codec applies to the whole image:

| Value | Meaning |
|---|---|
| 0 | none |
| 1 | zlib |
| 2 | zlib, a variant; both decode as zlib **[V]** |
| 3 | audio/video — a *different* codec from version 5's `avhu` **[P]** |

**The `flac` codec carries a byte of `L` or `B`** ahead of a bare run of
FLAC frames, stating the order the decoded samples are to be written
back in. There is no header: the geometry one would carry is the hunk
size, and a hunk is read as interleaved 16-bit stereo whatever it
actually holds -- a hard-disk image uses this codec too, so nothing
about it implies audio. **[V]**

**The zlib codec is raw DEFLATE**, with no zlib header and no adler32
trailer. **[V]** An image built with zlib-wrapped hunks is rejected with a
decompression error; rebuilding with raw DEFLATE and changing nothing
else makes it decode. This applies to every version.

**Unit size, when no header field carries it.** Versions before 5 have no
`unitbytes`. Both readers infer it from metadata that names a sector
size, and both fall back to `hunkbytes` when no such metadata exists.
**[V]**

### 3.1 Metadata entries **[V]**

`metaoffset` points at a singly linked list. Each entry is a 16-byte
header followed by its payload:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | four-character tag |
| 4 | 1 | flags |
| 5 | 3 | payload length |
| 8 | 8 | offset of the next entry, zero at the end |

The payload follows immediately. Hard-disk images carry a `GDDD` entry
whose payload is ASCII of the form `CYLS:n,HEADS:n,SECS:n,BPS:n.`, and
the `BPS` value is what supplies the unit size described above.

---

### 3.2 CD track layout **[V]**

A disc image's tracks come from metadata, not from any header field.
Each `CHTR` or `CHT2` entry describes one, as text:

    TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:15784 PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0

**Each track occupies its frame count padded up to a multiple of four,
and tracks sit one after another in that padded form.** Measured across
eleven images with between one and twenty-nine tracks, the padded total
equals the image's frame count exactly, every time — the unpadded total
never does except when every track happens to be a multiple of four
already.

**A GD-ROM image describes its tracks the same way, under the tag
`CHGD`, with one field more.** A GD track carries `PAD:`, which a CD
track does not:

    TRACK:2 TYPE:AUDIO SUBTYPE:NONE FRAMES:44164 PAD:43335 PREGAP:0 ...

**`PAD` is not the storage padding.** The four-frame rounding above is
what places a GD track in the image, exactly as it places a CD one:
measured on a five-track image, the frame counts rounded up to four sum
to 549152, which is the frame count the header implies, while adding
`PAD` instead gives 592935 and adding nothing gives 549150. `PAD`
describes the disc rather than the file -- track 2's 43335 is the gap
between the single-density and high-density areas -- and a reader
placing tracks by it reads every track after the first from the wrong
offset.

The two happen to agree on where the high-density area starts: 836 plus
44164 is 45000, so a reader accumulating stored frames arrives at the
same LBA the disc uses. That is a property of how the image was built
and not something to rely on.

**A frame is 2448 bytes whatever the track holds.** The sector data a
track actually carries depends on its type, and the rest of the frame is
reserved:

| Type | Sector bytes |
|---|---|
| `MODE1`, `MODE2_FORM1` | 2048 |
| `MODE2_FORM2` | 2324 |
| `MODE2`, `MODE2_FORM_MIX` | 2336 |
| `MODE1_RAW`, `MODE2_RAW`, `AUDIO` | 2352 |

`SUBTYPE` adds 96 bytes when it is not `NONE`. A read addressed by
sector therefore has no fixed stride: crossing from a data track into an
audio one changes how much each sector yields.

### 3.3 DVD and UMD images **[V]**

A DVD image describes itself with a single metadata entry tagged `DVD `
carrying one byte, and no track metadata at all. Its unit size is the
sector size, 2048, and there is one run of sectors from the start.
Verified on a UMD image, which uses the same shape.

**A disc image is not necessarily framed as a disc.** A PlayStation 2
DVD to hand is stored the other way: `CHT2` track metadata, `cdlz` and
friends, a unit of 2448, and a single `MODE1` track whose sectors are
2048 bytes inside 2448-byte frames. The container shape follows the
tool that made the image rather than the medium it came from, so a
reader cannot infer one from the other. It works out at 2294320 sectors
either way; only the packing differs.

That image is also the first to hand whose sectors are smaller than a
frame. Every disc image before it had 2352-byte sectors, so the
distinction between a track's data size and the frame that holds it was
carried but never exercised.

### 3.4 Hunk size ceiling **[V]**

The largest hunk the format permits differs either side of version 5:

| Versions | Ceiling |
|---|---|
| 1 to 4 | 16 MiB |
| 5 | 512 KiB |

A reader with one ceiling for both is wrong in one direction or the
other -- too generous for version 5, or strict enough to refuse an
earlier image that another reader accepts.

## 4. Pending

In the order they block work:

1. **Packed tree serialisation** — the form used by `huff` and `avhu`,
   distinct from §2.3.2. See §6 for what has been ruled out.
2. **The version 1 to 4 A/V codec.** Compression enum 3 is not version
   5's `avhu`; it is the earlier codec that `avhu` replaced. Laserdisc
   images for older emulators use it, so full coverage needs both.
3. **CD framing** — sector and subchannel interleave, the ECC bitmap, and
   the per-codec header shape. `cdfl` is the odd one and needs its own
   check.
5. **`flac` framing** — the endianness byte and the synthesised stream
   parameters.
6. **avhuff hunk layout.**
7. **Versions 1 to 4**, blocked on samples.

---

## 5. Canonical code ordering **[V]**

Codes are assigned from code lengths by walking from the **longest**
length down to the shortest, halving the starting code at each step. RFC
1951 walks the other way, from shortest up, doubling. The two produce
different codes: for lengths `{1, 2, 2}` this gives `1`, `00`, `01`
where DEFLATE gives `0`, `10`, `11`.

Most map trees cannot tell the two apart, because a tree whose codes are
all one length is assigned identically either way. The four-codec
reference image is the one that can: its tree has lengths `{1, 2, 2}`
exactly, and the stored CRC-16 matches only under the downward walk.

This is why `rhuff` documents its ordering as a dialect rather than as
"canonical Huffman" — an implementation that reached for RFC 1951's
ordering would decode single-codec images correctly and fail on
multi-codec ones.

---

## 6. Packed tree serialisation **[V]**

Used by the `huff` hunk codec and by A/V hunks. Distinct from the RLE
form of §2.3.2, which the map uses.

### 6.1 Structure

A tree over the main alphabet is described by a *second*, smaller tree,
which codes the main tree's code lengths.

**The small tree.** Its own code lengths are stored as fixed 3-bit
values over an alphabet of 24 symbols, at most 6 bits per code:

- one 3-bit value: the length of symbol 0;
- one 3-bit value: an index, plus one, at which the remaining lengths
  resume, so leading unused symbols cost nothing;
- 3-bit lengths from that index upward, ending at a stored value of 7.
  Seven cannot be a real length here because the ceiling is 6, which is
  what frees it to terminate the list.

**The main lengths.** Symbols decoded with the small tree, where a
symbol *v* other than zero means a code length of *v* − 1, and zero
introduces a run repeating the last length emitted:

- a 3-bit field holds the run count less two, so 0 to 6 gives runs of
  2 to 8;
- a field of 7 escapes: the count is 9 plus a further field of
  `ceil(log2(numcodes - 9))` bits — 8 for a 256-symbol alphabet.

Both trees then get the canonical assignment of §5, longest length
first.

### 6.2 Incomplete trees are legal

A hunk of one repeated byte yields an alphabet with a single used
symbol, whose one-bit code covers only half the code space. **That is a
valid tree and must be accepted.** Rejecting it as incomplete fails
exactly the inputs that compress best, and a decoder that requires
completeness will decode ordinary data and fall over on a run of
identical bytes.

Overlap between codes must still be rejected; it is only the
requirement that they *cover* the space that has to go.

### 6.3 Verification

Seventy-one `huff` hunks across four images, decoded and compared against
the original uncompressed source. All byte-exact, in both a Python model
and the C implementation, the latter clean under ASan and UBSan.

The images are built to exercise the tree shape directly rather than
sampled from real content: one hunk per distinct-symbol count from 1 to
8; sixteen hunks each holding a single repeated byte, at values spread
across the alphabet; and nineteen holding exactly two symbols, at index 0
and at a second index chosen to step the zero run between them one at a
time. That last set is what made the encoding legible — a single-symbol
hunk varies two runs at once, and the resulting signal is too muddy to
read.

---|---|---|
| 1 | 518 | 6 |
| 2 | 518 | 6 |
| 4 | 1031 | 7 |
| 8 | 1543 | 7 |

**The whole serialised tree is 4 to 7 bytes.** For the single-symbol
image the tree is 4 bytes at symbol values 0x00, 0x01, 0xfe and 0xff,
5 bytes at 0x02 through 0x08, and 6 bytes from 0x10 upward.

That size is the strongest constraint available and it rules out both
families tried:

- **A 24-code small tree with fixed-width lengths.** At three bits per
  length that needs at least 72 bits before any length vector is coded.
  The whole tree is 32 to 56.
- **The RLE form of §2.3.2 at a wider value width.** Encoding 255 zeros
  in runs bounded by a 5-bit count needs eight runs at fifteen bits.
  Swept widths 3 through 6; none produces a valid complete tree.

A parameter sweep over small-tree code counts 16/20/24, value widths
3/4, length ceilings 6/7/8, both senses of the skip condition, and
repeat widths 3/4, checked against all sixteen known vectors, produced no
match.

### 6.3 The run-length encoding **[V]**

Hunks built from exactly two symbols, at index 0 and index *N*, give a
length vector with two entries of length 1 and a run of *N*-1 zeros
between them. Stepping *N* varies that run one at a time while
everything before it stays fixed, which isolates the field.

A run is coded as a **3-bit field holding the count less three**, so
values 0 to 6 express runs of 3 to 9. **A field of 7 is an escape**: the
count is then 10 plus a further **8-bit** field.

Confirmed on fifteen runs from 3 to 128. The escape width is
`ceil(log2(numcodes - 9))`, which is 8 for a 256-symbol alphabet — the
same relation the map's alphabet would give, so it is a rule rather than
a constant.

The bias is worth stating precisely because an earlier attempt used a
count of *field + 2* escaping at 9, which is off by one and decodes every
short run wrongly while still consuming the right number of bits.

### 6.4 What remains

The serialisation of the *small tree* — the one used to code the length
values and the run escapes themselves. What is known:

- It occupies the first 16 bits in the common case, and the first coded
  symbol lands at bit 16.
- It takes one of two forms across the sixteen single-symbol hunks:
  thirteen share a 16-bit prefix, three share a different one.
- Sweeping small alphabet sizes 16/20/24, value widths 3/4, code-length
  ceilings 6/7/8 and both senses of the skip condition — now with the
  corrected run coding — still matches none of the nineteen known length
  vectors.

Since the run coding is now fixed and verified, the remaining unknown is
narrow: how roughly 16 bits describe a tree over the small alphabet of
length values. Solving it from the two-symbol series is the next step,
working backwards from the known emission sequence — a length value, a
run, a length value, a run — to the codes those four symbols must have.

---

## 7. CD framing **[V]**

Applies to `cdzl`, `cdlz`, `cdfl` and `cdzs`. A CD image has
`unitbytes` 2448 — a 2352-byte sector followed by 96 bytes of
subchannel — and `hunkbytes / unitbytes` frames per hunk.

### 7.1 Blob layout

| Part | Size | Notes |
|---|---|---|
| ECC bitmap | `(frames + 7) / 8` bytes | one bit per frame |
| sector stream length | 2 bytes, or 3 when `hunkbytes >= 65536` | big-endian |
| sector stream | the length above | `frames * 2352` bytes when decoded |
| subchannel stream | whatever remains | `frames * 96` bytes when decoded |

The sector stream uses the codec's base compressor — DEFLATE for `cdzl`,
LZMA for `cdlz`, FLAC for `cdfl`, Zstandard for `cdzs`.

**The subchannel stream is raw DEFLATE for every CD codec except
`cdzs`, which uses Zstandard for both of its streams.**

The exception is easy to miss. Three of the four codecs share the
DEFLATE rule, so a corpus without a `cdzs` image confirms it and
generalises wrongly — which is exactly what happened here, and it was
found only when such an image arrived. A mixed-codec image makes it
visible immediately: every `cdzs` hunk fails while every `cdfl` hunk of
the same file decodes, so the fault cannot be in the framing they
share.

The two streams are stored whole and consecutively, not interleaved: all
sector data, then all subchannel data. They are interleaved only on
output, where each frame's 2352 sector bytes are followed by its own 96
subchannel bytes.

### 7.2 The ECC bitmap

A set bit means that frame's sync pattern and ECC field were stripped
before compression and must be rebuilt.

**Bit *i* is `1 << (i & 7)` of byte `i >> 3` — least significant bit
first.** This is worth stating plainly because the opposite order is the
natural guess and is wrong in a way that hides: a hunk whose bitmap is
`0xff` decodes identically under either convention, and those are common.
The first hunk observed to distinguish them had a bitmap of `0x0f`, where
the wrong order both skips the frames that needed rebuilding and corrupts
the ones that did not.

### 7.3 Rebuilding a stripped frame

Sync first: byte 0 is `0x00`, bytes 1 to 10 are `0xff`, byte 11 is
`0x00`. Only bytes 1 to 10 are actually absent from the stored data; the
two `0x00` bytes are already there.

Then the layered error correction of ECMA-130, over GF(2^8) with
primitive polynomial `0x11d`:

- **P parity** — 86 majors of 24 minors, major multiplier 2, minor
  increment 86, read from sector offset `0x0c`, written to `0x81c`.
- **Q parity** — 52 majors of 43 minors, major multiplier 86, minor
  increment 88, read from the same base, written to `0x8c8`. Q covers the
  P parity just written, so the order matters.

**Whether the four header bytes at offset 12 take part depends on the
sector's mode**, which is the byte at offset 15:

| Mode | Header during ECC |
|---|---|
| 1 | included |
| 2 | treated as zero, restored afterwards |

Getting this backwards produces ECC that is wrong for every sector of
the affected mode, and nothing in an ordinary read notices: the field is
only consulted by hardware and by verification tools, so an image reads
back plausibly and fails much later, somewhere else.

It is easy to arrive at one rule and stop, because a corpus of
PlayStation discs is entirely Mode 2 and a corpus of PC discs entirely
Mode 1. Confirmed both ways: on a Mode 2 disc the zeroed form
reproduces all eight frames of a hunk and the included form none, and on
two Mode 1 discs it is exactly reversed.

**The EDC at offset 2072 is stored, not regenerated.** It falls outside
the rebuilt range, which is 2076 to 2351 — the 276 ECC bytes of a 2352
byte sector, after 12 sync, 4 header, 8 subheader, 2048 data and 4 EDC.

### 7.4 Verification

Hunks across seven commercial CD images, reconstructed and compared
byte-for-byte against an independent reader's decode of the same hunk.
All match. Between them they cover Mode 1 and Mode 2 data tracks, pure
audio discs, mixed-mode discs, and both hunk geometries -- one frame per
hunk and eight.

**The subchannel half is verified separately, and against a built
image.** Every commercial disc to hand reports `SUBTYPE:NONE` and
carries nothing but zeros there, so the half of each frame it occupies
would otherwise be untested: a decoder that dropped it, or put it in the
wrong half, would agree with every one of them. `tools/chd/make_subchannel_cd.py`
builds an image that does carry it; 50 of 50 hunks of that image
reconstruct byte-for-byte, with 38142 non-zero subchannel bytes among
them.

Two things about that image are deliberate. Its sectors are structured
rather than lifted from a real disc, because real game data is already
compressed -- an image made from it stores every hunk whole and the
codec under test never runs. And its subchannel is distinctive per
sector but compressible, for the same reason.

---

## 8. `cdfl` **[V]**

`cdfl` shares §7's CD geometry but has a different, simpler blob layout.

### 8.1 Blob layout

**There is no header at all.** No ECC bitmap, no length field. The FLAC
stream begins at byte zero, and the subchannel stream follows it.

| Part | Notes |
|---|---|
| FLAC stream | from byte 0; decodes to `frames * 2352` bytes |
| subchannel stream | raw DEFLATE, whatever trails the FLAC data |

The FLAC data is two channel, sixteen bit. A decoder knows where it ends
by decoding until it has produced `frames * 2352` bytes; everything after
that is the subchannel.

The absence of an ECC bitmap follows from what `cdfl` is chosen for: an
encoder picks it for audio, and audio sectors carry no sync pattern and
no ECC field, so there is nothing to strip and nothing to rebuild.

### 8.2 Sample byte order

**The decoded samples are byte-swapped relative to the sector data.**
Each 16-bit sample is stored most significant byte first, where a disc
image holds it least significant byte first, so every pair must be
swapped on the way out. This is invisible until compared against real
output — the stream decodes perfectly either way, it is simply wrong.

### 8.3 Verification

Thirty hunks across two commercial images, decoded and compared
byte-for-byte against an independent reader. All thirty match.

A synthetic image built from content controlled sector by sector —
silence, a DC level, a tone, a ramp and noise, three homogeneous hunks
each — confirms the layout holds across the whole compressibility range,
from a 41-byte blob to one of 18849 for a 18816-byte payload.

### 8.4 A note on method

An earlier pass concluded that most `cdfl` blobs contained no FLAC stream
at any offset, and recorded that as a ruled-out hypothesis. It was wrong,
and the cause is worth recording.

The test harness decoded the hunk map without consuming the offset bits
that a self reference or a parent reference carries. Every hunk after the
first such reference therefore had a wrong offset, and the blobs being
probed were arbitrary slices of neighbouring hunks. The commercial images
used for that pass each contain exactly one self reference early on, so
almost everything examined was garbage.

Two things hid it. The map's own CRC still matched, because the CRC is
computed over the decoded entries and the separate script that produced
those consumed the bits correctly — only the harness copy did not. And
the CD framing tests still passed, because the hunks they sampled
happened to precede the first self reference.

The harness now asserts that decoded hunk offsets partition the data
region exactly, which is the check that would have caught it immediately.

---

## 9. Audio/video hunks — `avhu`

A laserdisc image stores one video field and its audio per hunk, with
`hunkbytes` equal to `unitbytes`. Verified against a commercial title
of 79484 hunks of 380236 bytes.

### 9.1 Geometry, from metadata **[V]**

An `AVAV` metadata entry states it as ASCII:

    FPS:59.940058 WIDTH:720 HEIGHT:262 INTERLACED:1 CHANNELS:2 SAMPLERATE:44100

An `AVLD` entry alongside it carries binary per-frame laserdisc data.
Its layout is **[?]** and it is not needed to decode audio or video.

The hunk size follows from the geometry, which is worth checking because
it confirms the decoded layout without decoding anything:

| Part | Size |
|---|---|
| header | 12 |
| video, `width * height * 2` | 377280 |
| audio, up to `ceil(rate / fps)` samples per channel, 16-bit | 2944 |
| **total** | **380236** |

Video is two bytes per pixel.

**The audio sample count varies between hunks.** 44100 over 59.940058 is
735.75, so a hunk carries 736 samples or 735 and the average comes out
right, while the hunk size is fixed for the larger case. A hunk with the
smaller count simply ends four bytes short of its size, and a reader that
requires the parts to add up exactly rejects one hunk in four.

### 9.2 Compressed hunk layout **[V]**

The blob a hunk's map entry points at begins with a header of
`10 + 2 * channels` bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | metadata length |
| 1 | 1 | channel count |
| 2 | 2 | samples per channel |
| 4 | 2 | width |
| 6 | 2 | height |
| 8 | 2 | how the audio is coded; see below |
| 10 | 2 each | compressed length, one per channel |

Then the audio streams, one per channel in order, each of the length the
header gave; then the video for the rest of the blob.

Note the geometry is restated per hunk rather than taken from the
metadata, so a decoder can size its buffers from the blob alone -- and
should check the two agree.

### 9.3 Audio **[V]** for the FLAC mode, **[?]** for the others

The field at offset 8 chooses how the channels are coded, and it is a
length with two reserved values rather than a flag:

| Value | Meaning |
|---|---|
| `0xffff` | each channel is a FLAC stream |
| `0x0000` | each channel is raw 16-bit deltas |
| anything else | that many bytes of Huffman trees, then coded deltas |

Only `0xffff` appears in the image to hand, and only that is verified.
The distinction matters to a reader: the three modes share a header and
differ only in this field, so one that ignores it decodes the other two
to noise without noticing.

The other two modes are described here and implemented, but have never
decoded a real stream. Both code each channel as deltas on the previous
sample starting from zero -- raw big-endian pairs when the field is
zero, or, when it is a length, that many bytes holding two trees whose
symbols are the high and low halves of each delta.

**In the FLAC mode each channel is a separate headerless single-channel
stream**, not one interleaved stereo stream, and the samples are final
rather than deltas -- the codec does its own prediction. There is no
marker and no STREAMINFO; the sample count comes from the hunk header
and the rest of the geometry from the metadata.

Confirmed by decoding both channels of a hunk against the same audio
extracted by another implementation, sample for sample.

### 9.4 Video **[V]**, framing derived from the reference implementation

**Provenance: this subsection is not clean-room.** Everything else in
this document was derived by observing files. The framing below was read
from MAME's `avhuff.cpp`, after measurement established the transform
and then failed to locate the bitstream layout across two passes. The
distinction is recorded here rather than left to be inferred; §9.1 to
§9.3 remain observed, as does the transform in this section.

**Established by measurement first.** A field's output, obtained by
extracting frames with another implementation, entropy-codes to 100971
bytes under a horizontal delta against an actual 102991 -- within 2%,
where raw samples give 127974 and a vertical delta 127000. That fixed
the transform before any source was read.

**Layout.** One byte is skipped, then three code-length trees in the
serialisation of §2.3.2, over an alphabet of **272** symbols at up to 16
bits, one tree each for Y, Cb and Cr. **Each tree is followed by a flush
to the next byte boundary**, which is why sweeping bit offsets for a
single tree never resolved: the second and third do not begin where a
continuous bit stream would put them.

**Symbols.** A value below 0x100 is a delta added to that plane's
running sample, which starts at zero. A value at or above it is a run of
the previous sample:

| Code | Repeats |
|---|---|
| 0x100 to 0x107 | 8 to 15, as `8 + (code - 0x100)` |
| 0x108 to 0x10f | 16, 32, 64 ... 2048, as `16 << (code - 0x108)` |

**Order.** Samples are emitted straight into packed YUY2 -- for each
pair of pixels, one Y, one Cb, one Y, one Cr, drawn alternately from the
three contexts. The planes are separate only in that each keeps its own
running sample and run state; the output is interleaved, not planar.
**Every context's run count is cleared at the end of each row**, so a
run never spans rows.

### 9.5 Verification

Eight consecutive fields of a commercial laserdisc image decode
byte-for-byte against the same fields extracted by another
implementation -- audio and video both, the audio through the
per-channel FLAC path of §9.3 and the video through the above.

### 9.6 The earlier A/V codec **[P]**

Versions 1 to 4 name their A/V codec by the enum value 3 rather than by
a tag. It is close enough that the difference is much narrower than the
version gap suggests, and secondary accounts overstate it: descriptions
of the two as differing in video layout and delta processing do not
survive a look at the implementations.

**The video coding is the same.** Both skip a byte, import three
delta-RLE trees for Y, Cb and Cr, and decode with the four contexts
Y, Cb, Y, Cr into packed order. Both reset each context's run count at
the end of a row and start each running sample at zero. The alphabet is
256 plus 16 in both, and the run mapping is the same function -- `8 +
(code - 0x100)` up to 0x107, then `16 << (code - 0x108)`. The two
differ only in how each tree's end is found: the earlier form returns a
byte count and the later flushes to a byte boundary, which come to the
same thing.

So §9.4 should decode an image of either era unchanged.

**The decoded hunk is the same.** `'chav'`, the same twelve-byte header,
metadata, one run of samples per channel, then the video field.
Anything reading a decoded hunk needs no changes.

**The compressed hunk header is the same shape**, and the audio mode
field at offset 8 is the same field -- the earlier codec simply has no
`0xffff` case, because FLAC is what version 5 added. Its two other
modes, raw deltas and Huffman-coded deltas, are the ones described in
§9.3 and are unimplemented here for both codecs.

**So the audio is the whole of the difference**, and the two modes that
carry it are implemented, unverified, in §9.3. An image of the earlier
era would exercise them and settle both codecs at once. This is marked predicted rather than
verified because no image using it is to hand; a laserdisc set built for
an emulator of that era would have one, since what decides the codec is
the age of the image rather than of the emulator reading it.
