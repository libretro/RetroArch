/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <formats/rav1_sequence.h>
#include <formats/rmp4.h>
#include <formats/rwebm.h>

static unsigned failures;
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)
static uint8_t bytes[4096];
static size_t used, config_at, media_at, mastering_at, light_at, float_at[10];
static const uint8_t config[] = {0x81, 0, 0x4c, 0, 0x0a, 1, 0xaa};
static const uint8_t packet[] = {0x32, 1, 0x55};

static void put(unsigned n)
{
   bytes[used++] = (uint8_t)n;
}
static void be32(uint32_t n)
{
   put(n >> 24);
   put(n >> 16);
   put(n >> 8);
   put(n);
}
static void zeros(size_t n)
{
   while (n--)
      put(0);
}
static void copy(const void *p, size_t n)
{
   memcpy(bytes + used, p, n);
   used += n;
}
static void patch32(size_t at, uint32_t n)
{
   bytes[at] = n >> 24;
   bytes[at + 1] = n >> 16;
   bytes[at + 2] = n >> 8;
   bytes[at + 3] = n;
}
static size_t box(const char *name)
{
   size_t at = used;
   be32(0);
   copy(name, 4);
   return at;
}
static void end_box(size_t at)
{
   patch32(at, (uint32_t)(used - at));
}

static void make_mp4(int with_config)
{
   size_t b, moov, trak, mdia, minf, stbl, stsd, entry, chunk;
   used = 0;
   config_at = 0;
   b = box("ftyp");
   copy("isom", 4);
   be32(0);
   copy("av01", 4);
   end_box(b);
   moov = box("moov");
   b = box("mvhd");
   zeros(12);
   be32(1000);
   be32(80);
   zeros(80);
   end_box(b);
   trak = box("trak");
   b = box("tkhd");
   zeros(12);
   be32(1);
   zeros(68);
   end_box(b);
   mdia = box("mdia");
   b = box("mdhd");
   zeros(12);
   be32(1000);
   be32(80);
   zeros(4);
   end_box(b);
   b = box("hdlr");
   zeros(8);
   copy("vide", 4);
   zeros(13);
   end_box(b);
   minf = box("minf");
   stbl = box("stbl");
   stsd = box("stsd");
   be32(0);
   be32(1);
   entry = box("av01");
   zeros(24);
   put(0);
   put(192);
   put(0);
   put(128);
   zeros(50);
   if (with_config)
   {
      b = box("av1C");
      config_at = used;
      copy(config, sizeof(config));
      end_box(b);
   }
   b = box("colr");
   copy("nclx", 4);
   put(0);
   put(1);
   put(0);
   put(16);
   put(0);
   put(9);
   put(128);
   end_box(b);
   b = box("mdcv");
   mastering_at = used;
   {
      static const unsigned xy[8] = {6250, 37500, 12500, 25000, 25000, 12500, 15625, 18750};
      unsigned j;
      for (j = 0; j < 8; j++)
      {
         put(xy[j] >> 8);
         put(xy[j]);
      }
   }
   be32(10240000); be32(0);
   end_box(b);
   b = box("clli"); light_at = used;
   put(4); put(0); put(1); put(0);
   end_box(b);
   end_box(entry);
   end_box(stsd);
   b = box("stts");
   be32(0);
   be32(1);
   be32(2);
   be32(40);
   end_box(b);
   b = box("stsc");
   be32(0);
   be32(1);
   be32(1);
   be32(2);
   be32(1);
   end_box(b);
   b = box("stsz");
   be32(0);
   be32(sizeof(packet));
   be32(2);
   end_box(b);
   b = box("stco");
   be32(0);
   be32(1);
   chunk = used;
   be32(0);
   end_box(b);
   b = box("stss");
   be32(0);
   be32(1);
   be32(1);
   end_box(b);
   end_box(stbl);
   end_box(minf);
   end_box(mdia);
   end_box(trak);
   end_box(moov);
   b = box("mdat");
   media_at = used;
   copy(packet, sizeof(packet));
   copy(packet, sizeof(packet));
   end_box(b);
   patch32(chunk, (uint32_t)media_at);
}

static size_t element(uint32_t id)
{
   size_t at;
   if (id > 0xffffff)
      put(id >> 24);
   if (id > 0xffff)
      put(id >> 16);
   if (id > 0xff)
      put(id >> 8);
   put(id);
   at = used;
   put(0x40);
   put(0);
   return at;
}
static void end_element(size_t at)
{
   size_t n = used - at - 2;
   bytes[at] = 0x40 | (uint8_t)(n >> 8);
   bytes[at + 1] = (uint8_t)n;
}
static void number(uint32_t id, unsigned n)
{
   size_t at = element(id);
   put(n);
   end_element(at);
}

static void make_webm(void)
{
   size_t b, segment, tracks, track, video, cluster;
   unsigned i;
   used = 0;
   b = element(0x1a45dfa3);
   video = element(0x4282);
   copy("webm", 4);
   end_element(video);
   end_element(b);
   segment = element(0x18538067);
   tracks = element(0x1654ae6b);
   track = element(0xae);
   number(0xd7, 1);
   number(0x83, 1);
   /* CodecPrivate may precede CodecID. */
   b = element(0x63a2);
   config_at = used;
   copy(config, sizeof(config));
   end_element(b);
   b = element(0x86);
   copy("V_AV1", 5);
   end_element(b);
   video = element(0xe0);
   number(0xb0, 192);
   number(0xba, 128);
   b = element(0x55b0);
   {
      size_t master = element(0x55d0);
      unsigned j;
      for (j = 0; j < 10; j++)
      {
         size_t f = element(0x55d1 + j);
         float_at[j] = used;
         if (j == 9)
         {
            be32(0x3fc00000);
            be32(0);
         }
         else
            be32(j == 8 ? 0x44800000 : 0x3f000000);
         end_element(f);
      }
      end_element(master);
   }
   end_element(b);
   end_element(video);
   end_element(track);
   end_element(tracks);
   cluster = element(0x1f43b675);
   number(0xe7, 0);
   for (i = 0; i < 2; i++)
   {
      b = element(0xa3);
      put(0x81);
      put(0);
      put(i * 40);
      put(i ? 0 : 128);
      if (!i)
         media_at = used;
      copy(packet, sizeof(packet));
      end_element(b);
   }
   end_element(cluster);
   end_element(segment);
}

static void check_packet(const uint8_t *data, size_t size)
{
   rav1_obu_t obu;
   size_t offset = 0;
   CHECK(data >= bytes && data <= bytes + used &&
         size <= (size_t)(bytes + used - data));
   CHECK(size == sizeof(packet) && !memcmp(data, packet, sizeof(packet)));
   CHECK(rav1_obu_next(&obu, data, size, &offset, true) == 1 &&
         obu.payload == data + 2 && obu.type == 6 && obu.payload_size == 1 &&
         offset == size);
}

static void test_mp4(void)
{
   rmp4_t *m;
   const rmp4_track *track;
   rmp4_packet pkt;
   rav1_config_t cfg;
   unsigned i;
   int more = 0;
   make_mp4(1);
   m = rmp4_open_memory_avail(bytes, used, media_at, &more, NULL, NULL);
   CHECK(m != NULL);
   if (!m)
      return;
   track = rmp4_get_track(m, 0);
   CHECK(rmp4_num_tracks(m) == 1 && track && track->codec == RMP4_CODEC_AV1 &&
         !strcmp(track->codec_id, "av01") && track->width == 192 &&
         track->height == 128);
   CHECK(track && track->codec_private &&
         track->codec_private != bytes + config_at &&
         track->codec_private_size == sizeof(config) &&
         !memcmp(track->codec_private, config, sizeof(config)) &&
         rav1_config_parse(&cfg, track->codec_private,
                           track->codec_private_size));
   CHECK(track && track->color_primaries == 1 && track->matrix_coefficients == 9 &&
         track->transfer_characteristics == 16 && track->full_range &&
         track->color_description_present && track->range_present);
   /* Streaming callers may discard the consumed moov pages. Decoder
    * setup must survive while packet payloads still borrow the input. */
   memset(bytes + config_at, 0, sizeof(config));
   CHECK(track && track->codec_private &&
         track->codec_private_size == sizeof(config) &&
         !memcmp(track->codec_private, config, sizeof(config)) &&
         rav1_config_parse(&cfg, track->codec_private,
                           track->codec_private_size));
   CHECK(rmp4_read_packet(m, &pkt) == RMP4_READ_AGAIN);
   CHECK(rmp4_read_packet(m, &pkt) == RMP4_READ_AGAIN);
   rmp4_set_avail(m, used);
   for (i = 0; i < 2; i++)
   {
      CHECK(rmp4_read_packet(m, &pkt) == 1);
      CHECK(pkt.timestamp == (int64_t)i * 40000000 && pkt.keyframe == !i &&
            pkt.track == 0);
      check_packet(pkt.data, pkt.size);
      CHECK(pkt.data == bytes + media_at + i * sizeof(packet));
   }
   CHECK(rmp4_read_packet(m, &pkt) == 0);
   rmp4_close(m);
   make_mp4(0);
   m = rmp4_open_memory(bytes, used);
   CHECK(m && rmp4_get_track(m, 0)->codec == RMP4_CODEC_UNKNOWN);
   rmp4_close(m);
}

static void test_webm(void)
{
   rwebm_t *m;
   const rwebm_track *track;
   rwebm_packet pkt;
   rav1_config_t cfg;
   unsigned i;
   int more = 0;
   make_webm();
   m = rwebm_open_memory_avail(bytes, used, media_at, &more);
   CHECK(m != NULL);
   if (!m)
      return;
   track = rwebm_get_track(m, 0);
   CHECK(rwebm_num_tracks(m) == 1 && track && track->codec == RWEBM_CODEC_AV1 &&
         !strcmp(track->codec_id, "V_AV1") && track->width == 192 &&
         track->height == 128);
   CHECK(track && track->codec_private == bytes + config_at &&
         track->codec_private_size == sizeof(config) &&
         rav1_config_parse(&cfg, track->codec_private,
                           track->codec_private_size));
   CHECK(rwebm_read_packet(m, &pkt) == RWEBM_READ_AGAIN);
   CHECK(rwebm_read_packet(m, &pkt) == RWEBM_READ_AGAIN);
   rwebm_set_avail(m, used);
   for (i = 0; i < 2; i++)
   {
      CHECK(rwebm_read_packet(m, &pkt) == 1);
      CHECK(pkt.timestamp == (int64_t)i * 40000000 && pkt.keyframe == !i &&
            pkt.track == 0);
      check_packet(pkt.data, pkt.size);
   }
   CHECK(rwebm_read_packet(m, &pkt) == 0);
   rwebm_close(m);
}

static void test_static_metadata(void)
{
   rmp4_t *mp4;
   rwebm_t *webm;
   const rmp4_track *m;
   const rwebm_track *w;
   unsigned i;
   static const uint32_t bad[] = {0x7fc00000, 0x7f800000, 0xbf800000};
   make_mp4(1);
   mp4 = rmp4_open_memory(bytes, used);
   m = mp4 ? rmp4_get_track(mp4, 0) : NULL;
   CHECK(m && m->max_cll == 1024 && m->max_fall == 256
         && m->mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
         && m->mastering.chromaticity[0] == 25000
         && m->mastering.chromaticity[2] == 6250
         && m->mastering.chromaticity[4] == 12500
         && m->mastering.luminance_max == 10240000
         && m->mastering.luminance_min == 0);
   rmp4_close(mp4);
   for (i = 0; i < 3; i++)
   {
      make_mp4(1);
      if (!i) bytes[mastering_at] = 0xff;
      else if (i == 1) patch32(mastering_at + 20, 10240001);
      else patch32(mastering_at - 8, 31);
      mp4 = rmp4_open_memory(bytes, used);
      m = mp4 ? rmp4_get_track(mp4, 0) : NULL;
      CHECK(m && !m->mastering.present);
      rmp4_close(mp4);
   }
   make_mp4(1); patch32(light_at - 8, 11);
   mp4 = rmp4_open_memory(bytes, used);
   m = mp4 ? rmp4_get_track(mp4, 0) : NULL;
   CHECK(m && !m->max_cll && !m->max_fall);
   rmp4_close(mp4);
   for (i = 0; i < 8; i++)
   {
      make_webm();
      if (i < 3) patch32(float_at[0], bad[i]);
      else if (i == 3) bytes[float_at[1] - 3] = 0xd1;
      else if (i == 4) bytes[float_at[0] - 1] = 0xff;
      else if (i == 5) bytes[float_at[0] - 3] = 0xe1;
      else if (i == 6)
      {
         patch32(float_at[8], 0);
         patch32(float_at[9], 0);
      }
      webm = rwebm_open_memory(bytes, used);
      w = webm ? rwebm_get_track(webm, 0) : NULL;
      CHECK(w != NULL);
      if (w)
      {
         if (i < 5) CHECK(!w->mastering.present);
         else CHECK(w->mastering.present == (i == 5 ? 0x3feu : 0x3ffu)
               && w->mastering.chromaticity[2] == 25000
               && w->mastering.luminance_max == (i == 6 ? 0 : 10240000)
               && w->mastering.luminance_min == (i == 6 ? 0 : 1250));
      }
      rwebm_close(webm);
   }
}

static uint32_t hash_bytes(const uint8_t *data, size_t size)
{
   uint32_t hash = UINT32_C(2166136261);
   size_t i;
   for (i = 0; i < size; i++)
      hash = (hash ^ data[i]) * UINT32_C(16777619);
   return hash;
}

static int inspect_file(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   rmp4_t *mp4 = NULL;
   rwebm_t *webm = NULL;
   const uint8_t *private_data = NULL;
   size_t private_size = 0;
   long size;
   rav1_config_t cfg;
   static rav1_sequence_t seq;
   unsigned width = 0, height = 0, count = 0;
   int i, track = -1, ret = -1, ok = 0;
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET) ||
       !(data = (uint8_t *)malloc((size_t)size)))
      goto done;
   if (fread(data, 1, (size_t)size, file) != (size_t)size)
      goto done;
   mp4 = rmp4_open_memory(data, (size_t)size);
   if (mp4)
   {
      for (i = 0; i < rmp4_num_tracks(mp4); i++)
      {
         const rmp4_track *t = rmp4_get_track(mp4, i);
         if (t->codec != RMP4_CODEC_AV1)
            continue;
         track = i;
         width = t->width;
         height = t->height;
         private_data = t->codec_private;
         private_size = t->codec_private_size;
         break;
      }
   }
   else
   {
      webm = rwebm_open_memory(data, (size_t)size);
      if (!webm)
         goto done;
      for (i = 0; i < rwebm_num_tracks(webm); i++)
      {
         const rwebm_track *t = rwebm_get_track(webm, i);
         if (t->codec != RWEBM_CODEC_AV1)
            continue;
         track = i;
         width = t->width;
         height = t->height;
         private_data = t->codec_private;
         private_size = t->codec_private_size;
         break;
      }
   }
   if (track < 0 || !rav1_config_parse(&cfg, private_data, private_size))
      goto done;
   if (!rav1_config_sequence(&seq, &cfg) || seq.max_width != width ||
       seq.max_height != height)
      goto done;
   printf("S %u %u %u %u %u %u %u\n", seq.max_width, seq.max_height,
          seq.bit_depth, seq.color_primaries, seq.transfer_characteristics,
          seq.matrix_coefficients, seq.color_range);
   printf("C %u %u %u %u %lu %08lx\n", width, height, cfg.profile,
          cfg.bit_depth, (unsigned long)private_size,
          (unsigned long)hash_bytes(private_data, private_size));
   for (;;)
   {
      rmp4_packet p4;
      rwebm_packet pw;
      const uint8_t *payload;
      size_t length, offset = 0;
      int key, packet_track;
      int64_t timestamp;
      rav1_obu_t obu;
      if (mp4)
      {
         ret = rmp4_read_packet(mp4, &p4);
         if (ret != 1)
            break;
         payload = p4.data;
         length = p4.size;
         timestamp = p4.timestamp;
         key = p4.keyframe;
         packet_track = p4.track;
      }
      else
      {
         ret = rwebm_read_packet(webm, &pw);
         if (ret != 1)
            break;
         payload = pw.data;
         length = pw.size;
         timestamp = pw.timestamp;
         key = pw.keyframe;
         packet_track = pw.track;
      }
      if (packet_track != track)
         continue;
      while ((i = rav1_obu_next(&obu, payload, length, &offset, true)) == 1)
      {
         if (obu.type == 1)
         {
            rav1_config_t packet_config = cfg;
            packet_config.sequence = obu;
            if (!rav1_config_sequence(&seq, &packet_config))
               goto done;
         }
         if (obu.data < data || obu.data > data + size ||
             obu.size > (size_t)(data + size - obu.data))
            goto done;
      }
      if (i < 0 || !length || offset != length)
         goto done;
      printf("P %" PRId64 " %d %lu %08lx\n", timestamp, key,
             (unsigned long)length, (unsigned long)hash_bytes(payload, length));
      count++;
   }
   ok = !ret && count;
done:
   rmp4_close(mp4);
   rwebm_close(webm);
   free(data);
   fclose(file);
   return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
   if (argc == 2)
      return inspect_file(argv[1]);
   test_mp4();
   test_webm();
   test_static_metadata();
   printf("AV1 MP4/Matroska configuration lifetime and borrowed packets: %u failures\n",
          failures);
   return failures ? 1 : 0;
}
