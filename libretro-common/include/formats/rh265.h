/* rh265 -- self-contained H.265/HEVC decoder for libretro-common.
 *
 * Main and Main 10 profile, 4:2:0 at 8 and 10 bits: I, P and B slices
 * with the full intra toolset, merge and AMVP with temporal
 * candidates, weighted prediction, the 4x4 DST and 4/8/16/32 inverse
 * DCTs, transform skip, sign-data hiding, cu_qp_delta quantisation
 * groups, explicit scaling lists, transquant bypass, constrained intra
 * prediction, multiple slices, wavefront-coded pictures, the in-loop
 * deblocking filter and sample-adaptive offset. Short- and long-term
 * reference pictures are supported. Pictures decode
 * concurrently on a thread pool (rh265_video_set_thread_pool). Every
 * supported fixture is checked against FFmpeg to the sample by the
 * oracle in samples/formats/h265.
 *
 * Out-of-scope streams are refused at the parameter-set or slice level
 * rather than decoded wrongly: 4:2:2/4:4:4/monochrome, other bit
 * depths, tiles, dependent slice segments and PCM.
 *
 * The persistent video API mirrors rh264_video/rvp8_video so a demuxer
 * (e.g. the MP4 glue in rmp4_video.c) can dispatch H.265 the same way:
 *
 *   rh265_video *v = rh265_video_open();
 *   rh265_video_set_extradata(v, hvcc, hvcc_len);   (the hvcC box payload)
 *   for each frame:
 *      if (rh265_video_decode(v, data, len) < 0) fail;   (1 = picture ready)
 *      y = rh265_video_plane(v, 0, &ystride, &w, &h);    (also 1=U, 2=V)
 *   rh265_video_close(v);
 *
 * Frame data may be either Annex-B (start-code delimited) or the
 * length-prefixed HVCC form carried in MP4 'mdat'; the NAL length size
 * is taken from the hvcC extradata when present (default 4).  The
 * returned plane pointers stay valid until the next decode call. */
#ifndef __LIBRETRO_SDK_FORMAT_RH265_H__
#define __LIBRETRO_SDK_FORMAT_RH265_H__

#include <stdint.h>
#include <stddef.h>
#include <formats/video_hw_frame.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct rh265_video rh265_video;

typedef struct rh265_video_color
{
   unsigned primaries, transfer, matrix, full_range;
   int video_signal_present, description_present;
} rh265_video_color_t;

/* SPS VUI metadata for a parsed PPS. Unspecified CICP values are 2.
 * Failure clears out. This query does not decode or allocate pixels. */
int rh265_video_get_color(const rh265_video *v, unsigned pps_id,
      rh265_video_color_t *out);


/* Parameter metadata for hardware adapters; coefficients are effective
 * raster-order SPS/PPS values. 32x32 entries correspond to matrix IDs 0, 3. */
typedef struct rh265_video_params
{
   int pps_id, sps_id, chroma_format_idc, bit_depth;
   int width, height, crop_left, crop_right, crop_top, crop_bottom;
   int log2_min_cb, log2_ctb, log2_min_tb, log2_max_tb, log2_max_poc_lsb;
   int max_dec_pic_buffering, max_num_reorder_pics, max_sub_layers_minus1;
   int max_transform_hierarchy_depth_inter, max_transform_hierarchy_depth_intra;
   int num_st_rps, num_long_term_ref_pics, long_term_ref_pics_present;
   int scaling_list_enabled, amp_enabled, sao_enabled, pcm_enabled;
   int temporal_mvp_enabled, strong_intra_smoothing;
   int dependent_slice_segments_enabled, output_flag_present, num_extra_slice_header_bits;
   int sign_data_hiding, cabac_init_present;
   int num_ref_idx_l0_default, num_ref_idx_l1_default, init_qp;
   int constrained_intra_pred, transform_skip_enabled, cu_qp_delta_enabled;
   int diff_cu_qp_delta_depth, cb_qp_offset, cr_qp_offset;
   int slice_chroma_qp_offsets_present, weighted_pred, weighted_bipred;
   int transquant_bypass_enabled, tiles_enabled, entropy_coding_sync_enabled;
   int loop_filter_across_slices, deblocking_filter_control_present;
   int deblocking_filter_override_enabled, deblocking_filter_disabled;
   int beta_offset_div2, tc_offset_div2, lists_modification_present;
   int log2_parallel_merge_level, slice_segment_header_extension_present;
   uint8_t list4[6][16], list8[6][64], list16[6][64], list32[2][64];
   uint8_t dc16[6], dc32[2];
} rh265_video_params_t;

/* Copy a parsed PPS and its SPS on the decoder owner thread. No allocation
 * or pixel access. Crop offsets are in luma samples. Disabled scaling lists
 * are zero. Returns 0 on success, -1 for missing sets/invalid arguments;
 * failure clears out. Success does not establish hardware eligibility. */
int rh265_video_get_params(const rh265_video *v, unsigned pps_id,
      rh265_video_params_t *out);

typedef struct rh265_sequence_info
{
   uint32_t profile_compatibility_flags, constraint_flags_hi;
   uint32_t max_latency_increase_plus1[7], lt_poc_lsb[32];
   unsigned vps_id, profile_space, profile_idc, tier_flag, level_idc;
   unsigned constraint_flags_lo, temporal_id_nesting, max_sub_layers_minus1;
   unsigned field_seq; /* SPS VUI field_seq_flag; zero when absent */
   unsigned max_dec_pic_buffering[7], max_num_reorder_pics[7];
   unsigned num_short_term_sets, num_long_term_pics;
   uint8_t lt_used[32];
} rh265_sequence_info_t;

typedef struct rh265_short_term_set
{
   int32_t delta_before[16], delta_after[16];
   unsigned num_before, num_after;
   uint8_t used_before[16], used_after[16];
} rh265_short_term_set_t;

/* SPS metadata for hardware parameter creation, selected through a PPS.
 * level_idc is the raw HEVC value; constraint flags are the 48 coded bits
 * split into high 32 and low 16. Layer limits include inferred values.
 * Short-term sets are resolved to explicit deltas, closest POC first.
 * Owner thread only; no allocation or pixel access. Return 0 on success,
 * -1 for invalid/missing sets or index. Failure clears out. */
int rh265_video_get_sequence(const rh265_video *v, unsigned pps_id,
      rh265_sequence_info_t *out);
int rh265_video_get_short_term_set(const rh265_video *v, unsigned pps_id,
      unsigned index, rh265_short_term_set_t *out);

typedef struct rh265_vps_info
{
   rh265_sequence_info_t sequence;
   uint32_t num_units_in_tick, time_scale, num_ticks_poc_diff_one_minus1;
   unsigned timing_present, poc_proportional;
} rh265_vps_info_t;

/* Single-layer VPS metadata selected through a PPS. VPS with HRD or layer
 * sets are unavailable through this query; software decoding is unaffected.
 * Owner thread, no allocation/pixels. Return 0 on success, -1 unavailable
 * or invalid arguments. Failure clears out. */
int rh265_video_get_vps(const rh265_video *v, unsigned pps_id,
      rh265_vps_info_t *out);

typedef struct rh265_long_term_refs
{
   uint32_t poc_lsb[16], msb_cycle[16];
   uint8_t used[16], msb_present[16];
   unsigned count;
} rh265_long_term_refs_t;

typedef struct rh265_slice_info
{
   size_t header_bytes, header_bits;
   int nal_type, temporal_id, reference, irap, idr;
   int first_slice_in_pic, no_output_of_prior_pics, pps_id;
   int slice_segment_address, slice_type, pic_output_flag, poc_lsb;
   int short_term_rps_sps_idx, predictor_delta_pocs;
   unsigned short_term_rps_bits;
   int num_negative, num_positive;
   int32_t delta_poc_before[16], delta_poc_after[16];
   uint8_t used_before[16], used_after[16];
   rh265_long_term_refs_t long_term;
} rh265_slice_info_t;

/* Parse a borrowed complete VCL NAL (two-byte header, no start code or
 * length prefix) on the decoder owner thread. No allocation, payload copy
 * or picture/DPB advancement. Header bytes include the NAL header and escape
 * bytes; header bits count unescaped syntax including byte alignment.
 * Headers over 64 KiB are refused. Return 0, -1 malformed/missing input,
 * or -2 unsupported syntax. Failure clears out; payload validity is not
 * established. Long-term metadata includes SPS and slice-signaled entries. */
int rh265_video_get_slice(rh265_video *v, const uint8_t *nal, size_t len,
      rh265_slice_info_t *out);

typedef struct rh265_nal
{
   const uint8_t *data;
   size_t size;
} rh265_nal_t;

/* Borrow the next NAL. pos starts at zero. length_size is explicit: zero
 * for Annex-B, 1..4 for length prefixes. Annex-B trailing zeros are omitted.
 * Returns 1, 0 at end, -1 malformed. Errors leave pos unchanged and clear
 * out. Input storage must remain valid; no allocation or payload copy. */
int rh265_nal_next(const uint8_t *data, size_t size, unsigned length_size,
      size_t *pos, rh265_nal_t *out);

typedef struct rh265_access_unit
{
   rh265_slice_info_t first;
   size_t coded_bytes;
   unsigned num_slices;
   int intra;
} rh265_access_unit_t;

/* Describe one picture on the decoder owner thread without advancing its
 * DPB. Caller supplies separate output storage and retains immutable input until submission
 * accepts it (also across busy retries). Only VCL NALs are returned; parameter
 * sets must already be supplied; matching in-band SPS/PPS repeats are accepted.
 * Return 0, -1 malformed/capacity failure, -2 unsupported or changed SPS/PPS. Failure clears out; slice entries are
 * usable only on success. No allocations, payload copies or frame leases. */
int rh265_video_get_access_unit(rh265_video *v, const uint8_t *data,
      size_t size, unsigned length_size, rh265_nal_t *slices,
      unsigned capacity, rh265_access_unit_t *out);

/* Opt in before supplying parameter sets. Retained complete NALs include
 * their headers and escape bytes; software-only decoders allocate nothing.
 * Owner thread only. Returns 0 on success, -1 on failure or late opt-in. */
int rh265_video_retain_parameter_sets(rh265_video *v);

/* Borrow the selected PPS -> SPS -> VPS chain, in VPS/SPS/PPS order.
 * Valid until the next parameter update or close. No allocation. Failure
 * clears all three entries; missing/invalid/unretained chains return -1.
 * A failed copy or NAL over 64 KiB disables that chain until replacement,
 * without changing software parameter parsing. */
int rh265_video_get_parameter_sets(const rh265_video *v, unsigned pps_id,
      rh265_nal_t out[3]);

/* Create a decoder. Returns NULL on allocation failure. */
rh265_video *rh265_video_open(void);

/* Supply the hvcC (HEVCDecoderConfigurationRecord) extradata from the
 * sample description, which carries the VPS/SPS/PPS and the NAL length
 * size. Optional for Annex-B input that carries in-band parameter sets,
 * required for bare HVCC sample data. Returns 0 on success. Safe to call
 * once before decoding. */
int rh265_video_set_extradata(rh265_video *v, const uint8_t *hvcc, size_t len);

/* Decode one access unit (one coded picture worth of NAL units) to
 * internal I420 planes. Accepts Annex-B or length-prefixed HVCC data.
 * Returns 1 when a picture is ready through rh265_video_plane, 0 when
 * the data was consumed without producing a picture (parameter sets or
 * SEI only), and negative on malformed input or unsupported syntax. */
/* Hardware: 2 busy (retry identical packet), 3 picture ready while the
 * packet remains unconsumed (take picture, then retry identical packet).
 * No CPU planes are available on hardware. */
int rh265_video_decode(rh265_video *v, const uint8_t *data, size_t len);

/* Hand out the next pending picture in display order without feeding
 * more data, for end of stream. Returns 0 when a picture became
 * available through rh265_video_plane, -1 when nothing is pending.
 */
/* Hardware: 2 busy; retry without waiting. */
int rh265_video_drain(rh265_video *v);

/* Borrow a decoded plane (0=Y, 1=U, 2=V). Valid until the next decode call. */
/* Active luma bit depth of the stream (8 or 10).  At 10 bits the
 * plane pointers reference uint16_t samples: cast the returned byte
 * pointer and index with the sample stride. */
/* While @skip is set, a sub-layer non-reference picture (TRAIL_N and
 * its kin) in the highest sub-layer is consumed without being decoded:
 * the decode call returns 0 for it, as for any sample yielding no
 * picture, and the stream moves on. Nothing can reference it, so what
 * follows decodes unchanged. For a caller that has fallen behind its
 * clock and would rather drop a frame than show every one late. */
void rh265_video_set_skip_nonref(rh265_video *v, int skip);
/* Whether the last decode call passed a picture over under that
 * setting; its presentation slot has gone. */
int rh265_video_dropped(const rh265_video *v);

int rh265_video_bit_depth(const rh265_video *v);

const uint8_t *rh265_video_plane(const rh265_video *v, int plane,
      int *stride, int *width, int *height);

void rh265_video_close(rh265_video *v);

/* Decode pictures concurrently: with @pool (an rthreads tpool_t of at
 * least @threads - 1 threads) and threads > 1, each picture's slices
 * go to the pool when the next picture opens, up to @threads pictures
 * decoding at once, a picture waiting for the rows of the ones it
 * predicts from and no more. Output is the single-thread output to
 * the sample, whatever the content - WPP or not, one slice or many.
 * NULL or threads <= 1 restores single-threaded decoding. The pool is
 * the caller's and must outlive every decode made while it is set.
 * Declared here, defined below the knobs it uses. */
/* For the decoder's own samples: how many times a reference read found
 * the rows it needed not yet final. Zero on one thread by construction;
 * a sample asserts it. */
int rh265_video_ref_wait_misses(void);

/* How many decode contexts the decoder keeps in rotation, 1 to 8: a
 * new picture takes the next one round. Today the pictures still
 * decode one after the other, so this changes which memory a picture
 * uses and nothing else - which is what a sample proves the
 * per-picture state complete with, before pictures decode
 * concurrently. */
void rh265_video_set_contexts(rh265_video *v, int n);

/* Test knob: hold each row's publication for up to @max_yields thread
 * yields, drawn at random, so that pictures reading from a picture in
 * flight wait for their rows rather than nearly always finding them.
 * Output must be byte-exact under it. 0 is off. Debug only. */
void rh265_video_set_publish_delay(int max_yields);

void rh265_video_set_thread_pool(rh265_video *v, void *pool,
      unsigned threads);

/* Registry storage outlives every decoder that opens through it. NULL
 * withdraws the offer; existing sessions retain their callbacks. */
struct rh265_hw_backend;
void rh265_set_hw_backend(const struct rh265_hw_backend *backend);
/* Withdraw only this offer; never remove a replacement backend. */
int rh265_unset_hw_backend(const struct rh265_hw_backend *backend);
/* Opt in before the first picture, after parameter sets. Returns 1 on
 * hardware, 0 on software; disabling an active session requires reopen. */
int rh265_video_set_hw(rh265_video *v, int enable);
int rh265_video_is_hw(const rh265_video *v);
/* Transfer the displayed lease once; caller imports or abandons it. */
int rh265_video_hw_frame(rh265_video *v, video_hw_frame_t *out);

RETRO_END_DECLS

#endif
