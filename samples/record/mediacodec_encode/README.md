# Android recording acceptance

`make check SANITIZER=address,undefined` runs the C89 recording contracts.
A JDK supplies JNI headers. These tests model native boundaries; they do not
prove Android hardware recording, pixel accuracy, or a playable A/V file.

The frontend backend is available only in an explicit acceptance build with
`HAVE_RECORD_MEDIACODEC=1`. `Makefile.common` requires Android, Vulkan, threads
and configuration-file support, adds the native recording objects once, and
sets the corresponding compiler define. The normal build leaves registration
disabled. All translation units in an acceptance build must use the same
feature defines. Encoder-only configurations also request the enabled Vulkan
AHB/FOREIGN/SYNC_FD prerequisites; a media decoder is not required.

Select the `mediacodec` recording driver in that build. It accepts final
viewport recording with the Vulkan driver, without streaming, Game Only, or
asynchronous core audio callbacks. The I/O worker admits the actual hardware
codec, dimensions, frame rate, explicit profile/level and color configuration.
The source must also pass the native capture format/extent checks. Unsupported
BGRA/packed ordering is refused rather than reinterpreted or copied unsafely.

Optional recording configuration keys are:

- `audio_enable`: boolean, default true. Native AAC currently admits S16 mono
  with the center speaker mask or S16 stereo with the stereo mask.
- `vcodec`: `h264` or `hevc`. SDR defaults to H.264; HDR requires HEVC.
- `format`: `mp4`.
- `video_bit_rate`: positive integer no greater than `INT_MAX`, default 16000000.
- `video_level`: explicit positive Android CodecProfileLevel constant. Unknown
  or unsupported native combinations are refused by codec admission.

Unknown configuration keys are refused. Default levels are AVC 4.2 / HEVC 4.1
for viewports through 1920x1080 and AVC 5.2 / HEVC 5.1 for larger viewports.
Native admission can refuse dimensions, rate or bitrate outside those levels;
this is not an assertion that every display mode has an encoder.
See the [Android constants](https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel).

Capture and PCM start together after the render-owner CREATE acknowledgment.
Recording timestamps use a rational monotonic microsecond clock independent of
movie position. A dropped video offer advances that clock; PCM BUSY fails the
recording without inventing samples. Shader repeats do not advance it.
PQ is preserved; scRGB is converted to PQ once on the independent GLES worker.
Mastering chromaticities stay unknown because the core metadata ABI does not
supply them. Known content light levels are copied without deriving them from
the panel peak or paper-white setting.

On stop, frame publication is withdrawn before source closure. Queued bindings,
partial startup, the input Surface, native reads and I/O ownership remain alive
until independently collected. Reset first acknowledges render-producer
quiescence, then reaches the existing Vulkan device drain. That acknowledgment
is not GPU completion, child retirement, or device-death proof. Failed drains
must retain unproven resources. New recording admission still waits for full
collection, even when reset is allowed to proceed.

Before default registration, run native acceptance for SDR, PQ and scRGB;
actual MP4/AAC A/V output, orientation and color; source drops/backpressure;
resize, video reset, stop during startup, cancellation and device loss.
Permanent native failures can still retain owners and need recovery evidence.
No-core menu-player recording, automatic codec-level selection across all
modes, and final recording history integration also require follow-up.
