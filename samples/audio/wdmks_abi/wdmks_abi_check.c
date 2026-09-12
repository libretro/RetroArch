/* audio/drivers/wdmks.c declares the slice of the kernel-streaming
 * interface the WDM-KS driver uses, because ks.h and ksmedia.h ship
 * with the driver kit rather than the Platform SDK and the toolchains
 * this project still builds with do not all have them.
 *
 * That is only safe if what is declared is what the system declares.
 * A wrong layout does not fail to build and does not misbehave
 * visibly - it hands the kernel a structure whose fields are in the
 * wrong places and reads its answers from the wrong offsets. So this
 * includes both, and asserts ours against the system's: every size,
 * every offset that is read, every constant, and the IOCTL code.
 *
 * It earned its place immediately. KSDATAFORMAT is a union with a
 * LONGLONG in it, and that LONGLONG aligns the type to eight; declared
 * as the plain struct it looks like, KSDATARANGE_AUDIO came out 84
 * bytes against the system's 88, and every field of a data range past
 * the first would have been read four bytes off. This caught it.
 *
 * The GUIDs are checked by tools/wdmks_abi_guids.py, which compares
 * the literals in our header against the canonical strings in the
 * system ones - they cannot be compared here because ks.h declares
 * them in a form that needs linkage.
 *
 * Nothing runs: the assertions are the test, and a mismatch is a
 * build failure. */

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
/* The driver itself: the declarations live at the top of it, where
 * asio.c and openal.c keep theirs, and where the griffin build wants
 * them. Including the whole translation unit is deliberate - a header
 * could drift from what the driver actually compiles. */
#define RA_WDMKS_ABI_CHECK 1
#include "audio/drivers/wdmks.c"
/* The comparison is cast to long because both sides are frequently
 * unnamed enumerations, and comparing two of those directly is a
 * warning of its own that says nothing about whether the values
 * agree - which is the only question here. */
#define SAME(name, a, b) \
   typedef char assert_##name[(((long)(a)) == ((long)(b))) ? 1 : -1]
SAME(sz_property,   sizeof(ra_ksproperty_t),         sizeof(KSPROPERTY));
SAME(sz_ksp_pin,    sizeof(ra_ksp_pin_t),            sizeof(KSP_PIN));
SAME(sz_multiple,   sizeof(ra_ksmultiple_item_t),    sizeof(KSMULTIPLE_ITEM));
SAME(sz_dataformat, sizeof(ra_ksdataformat_t),       sizeof(KSDATAFORMAT));
SAME(sz_range,      sizeof(ra_ksdatarange_audio_t),  sizeof(KSDATARANGE_AUDIO));
/* Every field, not only the ones whose offset happens to differ from
 * a neighbour's: two ULONGs swapped between themselves change no size
 * and no other offset, and a check that asserted only sizes would let
 * that through. Tried it, and it did. */
SAME(off_prop_set,  offsetof(ra_ksproperty_t, Set),     offsetof(KSPROPERTY, Set));
SAME(off_prop_id,   offsetof(ra_ksproperty_t, Id),      offsetof(KSPROPERTY, Id));
SAME(off_prop_flag, offsetof(ra_ksproperty_t, Flags),   offsetof(KSPROPERTY, Flags));
SAME(off_pinid,     offsetof(ra_ksp_pin_t, PinId),      offsetof(KSP_PIN, PinId));
SAME(off_pinprop,   offsetof(ra_ksp_pin_t, Property),   offsetof(KSP_PIN, Property));
SAME(off_mi_size,   offsetof(ra_ksmultiple_item_t, Size),  offsetof(KSMULTIPLE_ITEM, Size));
SAME(off_mi_count,  offsetof(ra_ksmultiple_item_t, Count), offsetof(KSMULTIPLE_ITEM, Count));
SAME(off_fmtsize,   offsetof(ra_ksdataformat_t, f.FormatSize), offsetof(KSDATAFORMAT, FormatSize));
SAME(off_fmtflags,  offsetof(ra_ksdataformat_t, f.Flags),      offsetof(KSDATAFORMAT, Flags));
SAME(off_smpsize,   offsetof(ra_ksdataformat_t, f.SampleSize), offsetof(KSDATAFORMAT, SampleSize));
SAME(off_reserved,  offsetof(ra_ksdataformat_t, f.Reserved),   offsetof(KSDATAFORMAT, Reserved));
SAME(off_major,     offsetof(ra_ksdataformat_t, f.MajorFormat), offsetof(KSDATAFORMAT, MajorFormat));
SAME(off_range,     offsetof(ra_ksdatarange_audio_t, DataRange), offsetof(KSDATARANGE_AUDIO, DataRange));
SAME(off_minbits,   offsetof(ra_ksdatarange_audio_t, MinimumBitsPerSample), offsetof(KSDATARANGE_AUDIO, MinimumBitsPerSample));
SAME(off_maxbits,   offsetof(ra_ksdatarange_audio_t, MaximumBitsPerSample), offsetof(KSDATARANGE_AUDIO, MaximumBitsPerSample));
SAME(off_sub,       offsetof(ra_ksdataformat_t, f.SubFormat), offsetof(KSDATAFORMAT, SubFormat));
SAME(off_spec,      offsetof(ra_ksdataformat_t, f.Specifier), offsetof(KSDATAFORMAT, Specifier));
SAME(off_maxch,     offsetof(ra_ksdatarange_audio_t, MaximumChannels), offsetof(KSDATARANGE_AUDIO, MaximumChannels));
SAME(off_minfreq,   offsetof(ra_ksdatarange_audio_t, MinimumSampleFrequency), offsetof(KSDATARANGE_AUDIO, MinimumSampleFrequency));
SAME(off_maxfreq,   offsetof(ra_ksdatarange_audio_t, MaximumSampleFrequency), offsetof(KSDATARANGE_AUDIO, MaximumSampleFrequency));
SAME(sz_priority,   sizeof(ra_kspriority_t),         sizeof(KSPRIORITY));
SAME(sz_connect,    sizeof(ra_kspin_connect_t),      sizeof(KSPIN_CONNECT));
SAME(sz_wfx,        sizeof(ra_ksdataformat_wfx_t),   sizeof(KSDATAFORMAT_WAVEFORMATEX));
SAME(off_c_medium,  offsetof(ra_kspin_connect_t, Medium),      offsetof(KSPIN_CONNECT, Medium));
SAME(off_c_pinid,   offsetof(ra_kspin_connect_t, PinId),       offsetof(KSPIN_CONNECT, PinId));
SAME(off_c_tohnd,   offsetof(ra_kspin_connect_t, PinToHandle), offsetof(KSPIN_CONNECT, PinToHandle));
SAME(off_c_prio,    offsetof(ra_kspin_connect_t, Priority),    offsetof(KSPIN_CONNECT, Priority));
SAME(off_wfx_wf,    offsetof(ra_ksdataformat_wfx_t, WaveFormatEx), offsetof(KSDATAFORMAT_WAVEFORMATEX, WaveFormatEx));
SAME(k_iface,       RA_KSINTERFACE_STANDARD_STREAMING, KSINTERFACE_STANDARD_STREAMING);
SAME(k_medium,      RA_KSMEDIUM_TYPE_ANYINSTANCE,      KSMEDIUM_TYPE_ANYINSTANCE);
SAME(k_prio,        RA_KSPRIORITY_NORMAL,              KSPRIORITY_NORMAL);
SAME(k_connstate,   RA_KSPROPERTY_CONNECTION_STATE,    KSPROPERTY_CONNECTION_STATE);
SAME(sz_kstime,     sizeof(ra_kstime_t),             sizeof(KSTIME));
SAME(sz_streamhdr,  sizeof(ra_ksstream_header_t),    sizeof(KSSTREAM_HEADER));
SAME(off_sh_extent, offsetof(ra_ksstream_header_t, FrameExtent),  offsetof(KSSTREAM_HEADER, FrameExtent));
SAME(off_sh_used,   offsetof(ra_ksstream_header_t, DataUsed),     offsetof(KSSTREAM_HEADER, DataUsed));
SAME(off_sh_data,   offsetof(ra_ksstream_header_t, Data),         offsetof(KSSTREAM_HEADER, Data));
SAME(off_sh_opts,   offsetof(ra_ksstream_header_t, OptionsFlags), offsetof(KSSTREAM_HEADER, OptionsFlags));
SAME(off_sh_pres,   offsetof(ra_ksstream_header_t, PresentationTime), offsetof(KSSTREAM_HEADER, PresentationTime));
SAME(ioctl_write,   RA_IOCTL_KS_WRITE_STREAM,        IOCTL_KS_WRITE_STREAM);
SAME(sz_position,   sizeof(ra_ksaudio_position_t),   sizeof(KSAUDIO_POSITION));
SAME(off_play,      offsetof(ra_ksaudio_position_t, PlayOffset),  offsetof(KSAUDIO_POSITION, PlayOffset));
SAME(off_writeoff,  offsetof(ra_ksaudio_position_t, WriteOffset), offsetof(KSAUDIO_POSITION, WriteOffset));
SAME(k_audio_pos,   RA_KSPROPERTY_AUDIO_POSITION,    KSPROPERTY_AUDIO_POSITION);
SAME(k_audio_lat,   RA_KSPROPERTY_AUDIO_LATENCY,     KSPROPERTY_AUDIO_LATENCY);
SAME(ioctl_prop,    RA_IOCTL_KS_PROPERTY,            IOCTL_KS_PROPERTY);
SAME(p_ctypes,      RA_KSPROPERTY_PIN_CTYPES,        KSPROPERTY_PIN_CTYPES);
SAME(p_dataflow,    RA_KSPROPERTY_PIN_DATAFLOW,      KSPROPERTY_PIN_DATAFLOW);
SAME(p_ranges,      RA_KSPROPERTY_PIN_DATARANGES,    KSPROPERTY_PIN_DATARANGES);
SAME(p_comm,        RA_KSPROPERTY_PIN_COMMUNICATION, KSPROPERTY_PIN_COMMUNICATION);
SAME(p_name,        RA_KSPROPERTY_PIN_NAME,          KSPROPERTY_PIN_NAME);
SAME(p_propose,     RA_KSPROPERTY_PIN_PROPOSEDATAFORMAT, KSPROPERTY_PIN_PROPOSEDATAFORMAT);
SAME(f_in,          RA_KSPIN_DATAFLOW_IN,            KSPIN_DATAFLOW_IN);
SAME(f_out,         RA_KSPIN_DATAFLOW_OUT,           KSPIN_DATAFLOW_OUT);
SAME(c_sink,        RA_KSPIN_COMMUNICATION_SINK,     KSPIN_COMMUNICATION_SINK);
SAME(c_both,        RA_KSPIN_COMMUNICATION_BOTH,     KSPIN_COMMUNICATION_BOTH);
SAME(s_stop,        RA_KSSTATE_STOP,                 KSSTATE_STOP);
SAME(s_acquire,     RA_KSSTATE_ACQUIRE,              KSSTATE_ACQUIRE);
SAME(s_pause,       RA_KSSTATE_PAUSE,                KSSTATE_PAUSE);
SAME(s_run,         RA_KSSTATE_RUN,                  KSSTATE_RUN);
SAME(t_get,         RA_KSPROPERTY_TYPE_GET,          KSPROPERTY_TYPE_GET);
SAME(t_set,         RA_KSPROPERTY_TYPE_SET,          KSPROPERTY_TYPE_SET);
SAME(t_topo,        RA_KSPROPERTY_TYPE_TOPOLOGY,     KSPROPERTY_TYPE_TOPOLOGY);
int main(void){ return 0; }
