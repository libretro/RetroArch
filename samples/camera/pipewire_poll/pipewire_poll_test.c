/* camera/drivers/pipewire.c polled against a scripted stream: the
 * dequeue/queue calls are link-wrapped, so each poll can be handed an
 * empty queue, a corrupted frame, a buffer without CPU data or a
 * valid frame, and the driver's handling of each is checked. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../camera/drivers/pipewire.c"

/* The core helpers of audio/common/pipewire.c: none runs on the poll
 * path, so they only need to link. */
bool pipewire_core_init(pipewire_core_t **pw, const char *loop_name,
      const struct pw_registry_events *events)
{
   (void)pw; (void)loop_name; (void)events;
   return false;
}
void pipewire_core_deinit(pipewire_core_t *pw) { (void)pw; }
bool pipewire_core_wait_resync(pipewire_core_t *pw) { (void)pw; return true; }
static unsigned script_active_calls;
static bool script_active;
bool pipewire_stream_set_active(struct pw_thread_loop *loop,
      struct pw_stream *stream, bool active)
{
   (void)loop; (void)stream;
   script_active_calls++;
   script_active = active;
   return true;
}

/* The scripted stream. */
static struct pw_buffer       *script_next;
static enum pw_stream_state    script_state   = PW_STREAM_STATE_STREAMING;
static unsigned                script_dequeues;
static unsigned                script_queued;
static struct pw_buffer       *script_queued_last;
static int                     script_stride;

int __wrap_pw_stream_update_params(struct pw_stream *stream,
      const struct spa_pod **params, uint32_t n_params)
{
   (void)stream;
   if (n_params > 0)
      spa_pod_parse_object(params[0], SPA_TYPE_OBJECT_ParamBuffers, NULL,
            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(&script_stride));
   return 0;
}

struct pw_buffer *__wrap_pw_stream_dequeue_buffer(struct pw_stream *stream)
{
   struct pw_buffer *b = script_next;
   (void)stream;
   script_dequeues++;
   script_next = NULL;
   return b;
}

int __wrap_pw_stream_queue_buffer(struct pw_stream *stream,
      struct pw_buffer *buffer)
{
   (void)stream;
   script_queued++;
   script_queued_last = buffer;
   return 0;
}

enum pw_stream_state __wrap_pw_stream_get_state(struct pw_stream *stream,
      const char **error)
{
   (void)stream;
   if (error)
      *error = NULL;
   return script_state;
}

uint64_t __wrap_pw_stream_get_nsec(struct pw_stream *stream)
{
   (void)stream;
   return 1000;
}

/* What the driver delivered. */
static unsigned frame_cb_count;
static uint32_t frame_cb_first_pixel;

static void frame_raw_cb(const uint32_t *buffer,
      unsigned width, unsigned height, size_t pitch)
{
   (void)width; (void)height; (void)pitch;
   frame_cb_count++;
   frame_cb_first_pixel = buffer[0];
}

#define W 8
#define H 4

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

static void script_reset(struct pw_buffer *next)
{
   script_next        = next;
   script_dequeues    = 0;
   script_queued      = 0;
   script_queued_last = NULL;
   frame_cb_count     = 0;
}

static void check_formats(void)
{
   static const uint32_t formats[] = { SPA_VIDEO_FORMAT_ARGB,
      SPA_VIDEO_FORMAT_BGR, SPA_VIDEO_FORMAT_YUY2, SPA_VIDEO_FORMAT_ABGR };
   static const unsigned bytes_per_pixel[] = { 4, 3, 2, 4 };
   unsigned i;

   printf("negotiated packed formats:\n");
   for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
   {
      uint8_t storage[1024];
      struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(storage, sizeof(storage));
      struct spa_video_info_raw info;
      pipewire_camera_t camera;
      const struct spa_pod *param;

      memset(&camera, 0, sizeof(camera));
      memset(&info, 0, sizeof(info));
      info.format = formats[i];
      info.size = SPA_RECTANGLE(W, H);
      param = spa_format_video_raw_build(&builder, SPA_PARAM_Format, &info);
      script_stride = 0;
      stream_param_changed_cb(&camera, SPA_PARAM_Format, param);
      CHECK(camera.buffer_output != NULL, "format creates output buffer");
      CHECK(camera.scaler.in_stride == (int)(W * bytes_per_pixel[i]),
            "scaler stride matches negotiated pixel size");
      CHECK(script_stride == (int)(W * bytes_per_pixel[i]),
            "requested buffer stride matches negotiated pixel size");
      if ((formats[i] == SPA_VIDEO_FORMAT_ARGB
               || formats[i] == SPA_VIDEO_FORMAT_ABGR
               || formats[i] == SPA_VIDEO_FORMAT_BGR) && camera.buffer_output)
      {
         uint32_t pixels[W * H];
         uint32_t abgr[W * H];
         uint8_t bgr[W * H * 3];
         struct spa_chunk chunk;
         struct spa_data data;
         struct spa_buffer buffer;
         struct pw_buffer pwb;
         unsigned j;
         for (j = 0; j < W * H; j++)
         {
            bgr[j * 3] = j;
            bgr[j * 3 + 1] = j + 1;
            bgr[j * 3 + 2] = j + 2;
            abgr[j] = 0xff000000u | (j << 16);
            pixels[j] = formats[i] != SPA_VIDEO_FORMAT_BGR
               ? 0xff000000u | j
               : 0xff000000u | ((j + 2) << 16) | ((j + 1) << 8) | j;
         }
         memset(&chunk, 0, sizeof(chunk));
         memset(&data, 0, sizeof(data));
         memset(&buffer, 0, sizeof(buffer));
         memset(&pwb, 0, sizeof(pwb));
         chunk.size = formats[i] != SPA_VIDEO_FORMAT_BGR
            ? sizeof(pixels) : sizeof(bgr);
         data.data = formats[i] == SPA_VIDEO_FORMAT_ABGR ? (void*)abgr
            : formats[i] == SPA_VIDEO_FORMAT_ARGB ? (void*)pixels : (void*)bgr;
         data.maxsize = chunk.size;
         data.chunk = &chunk;
         buffer.n_datas = 1;
         buffer.datas = &data;
         pwb.buffer = &buffer;
         script_reset(&pwb);
         CHECK(preprocess_image(&camera), "negotiated packed frame converts");
         CHECK(memcmp(camera.buffer_output, pixels, sizeof(pixels)) == 0,
               "negotiated packed format preserves every pixel");
         CHECK(script_queued == 1, "negotiated frame returned once");
      }
      scaler_ctx_gen_reset(&camera.scaler);
      free(camera.buffer_output);
   }
}

int main(void)
{
   pipewire_camera_t       camera;
   pipewire_core_t         core;
   struct pw_buffer        pwb;
   struct spa_buffer       buf;
   struct spa_data         data;
   struct spa_chunk        chunk;
   struct spa_meta         meta;
   struct spa_meta_header  header;
   uint32_t                pixels[W * H];
   uint32_t                out[W * H];
   uint32_t                padded[4 + (W + 2) * H];
   unsigned                x, y;
   bool                    polled;

   memset(&camera, 0, sizeof(camera));
   memset(&core, 0, sizeof(core));
   camera.pw = &core;
   camera.stream = (struct pw_stream*)&camera;
   camera.size.width         = W;
   camera.size.height        = H;
   camera.buffer_output      = out;
   camera.scaler.in_width    = camera.scaler.out_width  = W;
   camera.scaler.in_height   = camera.scaler.out_height = H;
   camera.scaler.in_fmt      = SCALER_FMT_ARGB8888;
   camera.scaler.out_fmt     = SCALER_FMT_ARGB8888;
   camera.scaler.in_stride   = W * 4;
   camera.scaler.out_stride  = W * 4;
   camera.scaler.scaler_type = SCALER_TYPE_POINT;
   if (!scaler_ctx_gen_filter(&camera.scaler))
   {
      printf("scaler_ctx_gen_filter failed\n");
      return 1;
   }

   memset(&pwb, 0, sizeof(pwb));
   memset(&buf, 0, sizeof(buf));
   memset(&data, 0, sizeof(data));
   memset(&chunk, 0, sizeof(chunk));
   memset(&meta, 0, sizeof(meta));
   memset(&header, 0, sizeof(header));
   memset(pixels, 0x5a, sizeof(pixels));
   pixels[0]     = 0x11223344u;
   pwb.buffer    = &buf;
   buf.n_datas   = 1;
   buf.datas     = &data;
   buf.n_metas   = 1;
   buf.metas     = &meta;
   meta.type     = SPA_META_Header;
   meta.size     = sizeof(header);
   meta.data     = &header;
   data.data     = pixels;
   data.maxsize  = sizeof(pixels);
   data.chunk    = &chunk;
   chunk.size    = sizeof(pixels);
   chunk.stride  = W * 4;

   printf("empty queue:\n");
   script_reset(NULL);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(!polled, "poll reports no frame");
   CHECK(script_dequeues == 1, "exactly one dequeue attempt");
   CHECK(script_queued == 0, "nothing queued back");
   CHECK(frame_cb_count == 0, "no frame delivered");

   printf("stream not streaming:\n");
   script_state = PW_STREAM_STATE_PAUSED;
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(!polled, "poll reports no frame");
   CHECK(script_dequeues == 0, "no dequeue while paused");
   script_state = PW_STREAM_STATE_STREAMING;

   printf("corrupted frame:\n");
   header.flags = SPA_META_HEADER_FLAG_CORRUPTED;
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(!polled, "poll reports no frame");
   CHECK(script_queued == 1 && script_queued_last == &pwb,
         "buffer queued back once");
   CHECK(frame_cb_count == 0, "no frame delivered");
   header.flags = 0;

   printf("buffer without cpu data:\n");
   data.data = NULL;
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(!polled, "poll reports no frame");
   CHECK(script_queued == 1 && script_queued_last == &pwb,
         "buffer queued back once");
   CHECK(frame_cb_count == 0, "no frame delivered");
   data.data = pixels;

   printf("valid frame:\n");
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(polled, "poll reports a frame");
   CHECK(script_dequeues == 1, "exactly one dequeue");
   CHECK(script_queued == 1 && script_queued_last == &pwb,
         "buffer queued back once");
   CHECK(frame_cb_count == 1, "frame delivered once");
   CHECK(frame_cb_first_pixel == 0x11223344u, "frame pixels converted");
   printf("offset and padded rows:\n");
   memset(padded, 0xa5, sizeof(padded));
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         padded[4 + y * (W + 2) + x] = pixels[y * W + x]
            = 0xff000000u | (y << 16) | x;
   data.data = padded;
   data.maxsize = sizeof(padded);
   chunk.offset = 4 * sizeof(uint32_t);
   chunk.stride = (W + 2) * sizeof(uint32_t);
   chunk.size = (H - 1) * chunk.stride + W * sizeof(uint32_t);
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(polled && frame_cb_count == 1, "padded frame delivered once");
   CHECK(memcmp(out, pixels, sizeof(pixels)) == 0,
         "chunk offset and row stride preserved all pixels");
   CHECK(script_queued == 1, "padded buffer returned once");
   data.data = pixels;
   data.maxsize = sizeof(pixels);
   chunk.offset = 0;
   chunk.stride = W * sizeof(uint32_t);
   chunk.size = sizeof(pixels);

   printf("packed frame after padded frame:\n");
   chunk.stride = 0;
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, frame_raw_cb, NULL, NULL);
   CHECK(polled && frame_cb_count == 1, "packed frame delivered once");
   CHECK(memcmp(out, pixels, sizeof(pixels)) == 0,
         "zero stride uses restored negotiated layout");
   CHECK(script_queued == 1, "packed buffer returned once");

   printf("valid frame, no raw callback:\n");
   script_reset(&pwb);
   polled = pwire_camera_poll(&camera, NULL, NULL, NULL);
   CHECK(!polled, "poll reports no frame");
   CHECK(script_dequeues == 0, "no dequeue without a consumer");

   printf("stop streaming camera:\n");
   script_state = PW_STREAM_STATE_STREAMING;
   script_active_calls = 0;
   script_active = true;
   pwire_camera_stop(&camera);
   CHECK(script_active_calls == 1 && !script_active,
         "streaming stop requests deactivation once");
   printf("stop paused camera:\n");
   script_state = PW_STREAM_STATE_PAUSED;
   script_active_calls = 0;
   pwire_camera_stop(&camera);
   CHECK(script_active_calls == 0, "paused stop needs no transition");
   CHECK(pwire_camera_start(&camera), "paused camera can restart");
   CHECK(script_active_calls == 1 && script_active,
         "restart requests activation once");
   script_state = PW_STREAM_STATE_STREAMING;
   script_active_calls = 0;
   CHECK(pwire_camera_start(&camera), "streaming camera remains started");
   CHECK(script_active_calls == 0, "started camera needs no transition");

   scaler_ctx_gen_reset(&camera.scaler);
   check_formats();

   printf("repeated format changes:\n");
   {
      pipewire_camera_t changing;
      unsigned cycle;
      memset(&changing, 0, sizeof(changing));
      for (cycle = 0; cycle < 16; cycle++)
      {
         uint8_t storage[1024];
         struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(storage, sizeof(storage));
         struct spa_video_info_raw info;
         const struct spa_pod *param;
         memset(&info, 0, sizeof(info));
         info.format = cycle & 1 ? SPA_VIDEO_FORMAT_ABGR : SPA_VIDEO_FORMAT_ARGB;
         info.size = SPA_RECTANGLE(W + (cycle & 1) * 2, H);
         param = spa_format_video_raw_build(&builder, SPA_PARAM_Format, &info);
         stream_param_changed_cb(&changing, SPA_PARAM_Format, param);
         CHECK(changing.buffer_output != NULL
               && changing.scaler.in_width == (int)info.size.width,
               "replacement format creates matching output");
      }
      scaler_ctx_gen_reset(&changing.scaler);
      free(changing.buffer_output);
   }

   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}
