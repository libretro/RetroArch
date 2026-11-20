/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rpyrowave.h>
#include <formats/rpyrowave_gpu.h>
#include <math.h>

typedef char rpy_gpu_block_wire_size[(sizeof(rpyrowave_gpu_block_t) == 16) ? 1 : -1];
typedef char rpy_gpu_parameters_wire_size[(sizeof(rpyrowave_packet_parameters_t) == 48) ? 1 : -1];
typedef char rpy_gpu_color_wire_size[(sizeof(rpyrowave_color_parameters_t) == 80) ? 1 : -1];

/* 128x128 4:2:0, sequence 5, two blocks, all usability flags set. */
static const uint8_t sequence_vector[8] =
{ 0x7f, 0xc0, 0x1f, 0xd0, 0x02, 0x00, 0x00, 0xf8 };

/* One 8x8 group, one bit per 4x2, +1 and -1 at its opposite corners.
 * Quantizer 32 gives scale 1; group scale is 1/4; deadzone adds 1/2. */
static const uint8_t block_vector[20] =
{
   0x01, 0x00, 0x05, 0x50, 0x20, 0x56, 0x34, 0x12,
   0x00, 0x00, 0x01,
   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
   0x02
};

static int failures;

#define CHECK(expr) do { if (!(expr)) { \
   if (failures < 16) \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
   failures++; } } while (0)

static void test_sequence(void)
{
   rpyrowave_sequence_t sequence;
   uint8_t bytes[8];
   size_t i;
   memset(&sequence, 0, sizeof(sequence));
   CHECK(rpyrowave_read_sequence(sequence_vector, 8, &sequence) == 0);
   CHECK(sequence.width == 128 && sequence.height == 128);
   CHECK(sequence.sequence == 5 && sequence.total_blocks == 2);
   CHECK(!sequence.chroma_444 && sequence.primaries_bt2020);
   CHECK(sequence.transfer_pq && sequence.transform_bt2020);
   CHECK(sequence.limited_range && sequence.chroma_left);
   for (i = 0; i < 8; i++)
      CHECK(rpyrowave_read_sequence(sequence_vector, i, &sequence)
            == RPYROWAVE_TRUNCATED);
   CHECK(sequence.width == 128);
   memcpy(bytes, sequence_vector, 8);
   bytes[0]--;
   CHECK(rpyrowave_read_sequence(bytes, 8, &sequence) == RPYROWAVE_INVALID);
   bytes[7] |= 4;
   CHECK(rpyrowave_read_sequence(bytes, 8, &sequence) == RPYROWAVE_OK);
   CHECK(sequence.width == 127 && sequence.chroma_444);
   bytes[7] |= 1;
   CHECK(rpyrowave_read_sequence(bytes, 8, &sequence)
         == RPYROWAVE_UNSUPPORTED);
   bytes[3] &= 0x7f;
   CHECK(rpyrowave_read_sequence(bytes, 8, &sequence) == RPYROWAVE_INVALID);
}

static void test_block(float *output)
{
   rpyrowave_block_t block;
   uint8_t bytes[20];
   size_t i;
   CHECK(rpyrowave_decode_block(block_vector, 20, output, &block) == 0);
   CHECK(block.index == 0x123456 && block.sequence == 5);
   CHECK(block.bytes == 20 && block.quant_code == 32 && block.ballot == 1);
   for (i = 0; i < 1024; i++)
      CHECK(output[i] == (i == 0 ? 0.375f : i == 231 ? -0.375f : 0.0f));
   for (i = 0; i < 20; i++)
   {
      output[0] = 19.0f;
      CHECK(rpyrowave_decode_block(block_vector, i, output, &block)
            == RPYROWAVE_TRUNCATED);
      CHECK(output[0] == 19.0f && block.index == 0x123456);
   }
   memcpy(bytes, block_vector, 20);
   bytes[2] = 0;
   CHECK(rpyrowave_decode_block(bytes, 20, output, &block)
         == RPYROWAVE_INVALID);
   bytes[2] = 4;
   CHECK(rpyrowave_decode_block(bytes, 20, output, &block)
         == RPYROWAVE_INVALID);
   CHECK(output[0] == 19.0f);
   bytes[2] = 5;
   bytes[10] = 15;
   CHECK(rpyrowave_decode_block(bytes, 20, output, &block)
         == RPYROWAVE_INVALID);
   bytes[3] |= 0x80;
   CHECK(rpyrowave_read_block(bytes, 20, &block) == RPYROWAVE_INVALID);
   memset(bytes, 0, 20);
   bytes[2] = 2;
   CHECK(rpyrowave_decode_block(bytes, 8, output, &block) == 0);
   for (i = 0; i < 1024; i++)
      CHECK(output[i] == 0.0f);
}

static void test_group_positions(float *output)
{
   uint8_t bytes[20];
   rpyrowave_block_t block;
   unsigned group;
   unsigned i;
   for (group = 0; group < 16; group++)
   {
      unsigned ballot = 1U << group;
      unsigned x      = (group % 4) * 8;
      unsigned y      = (group / 4) * 8;
      memcpy(bytes, block_vector, 20);
      bytes[0] = (uint8_t)ballot;
      bytes[1] = (uint8_t)(ballot >> 8);
      CHECK(rpyrowave_decode_block(bytes, 20, output, &block) == 0);
      for (i = 0; i < 1024; i++)
      {
         float expected = 0.0f;
         if (i == y * 32 + x)
            expected = 0.375f;
         if (i == (y + 7) * 32 + x + 7)
            expected = -0.375f;
         CHECK(output[i] == expected);
      }
   }
}

static void test_layout(void)
{
   rpyrowave_sequence_t sequence;
   rpyrowave_location_t location;
   CHECK(rpyrowave_read_sequence(sequence_vector, 8, &sequence) == 0);
   CHECK(rpyrowave_block_layout(&sequence, 0, &location) == 51);
   CHECK(!location.component && location.level == 4 && !location.band);
   CHECK(location.width == 4 && location.height == 4);
   CHECK(rpyrowave_block_layout(&sequence, 4, &location) == 51);
   CHECK(location.component == 1 && location.level == 4 && !location.band);
   CHECK(rpyrowave_block_layout(&sequence, 12, &location) == 51);
   CHECK(!location.component && location.level == 3 && location.band == 1);
   CHECK(rpyrowave_block_layout(&sequence, 50, &location) == 51);
   CHECK(!location.component && !location.level && location.band == 3);
   CHECK(location.x == 32 && location.y == 32 && location.width == 64);
   CHECK(rpyrowave_block_layout(&sequence, 51, &location) == 51);
   CHECK(location.x == 32 && location.y == 32);
   sequence.chroma_444 = 1;
   CHECK(rpyrowave_block_layout(&sequence, 74, &location) == 75);
   CHECK(location.component == 2 && !location.level && location.band == 3);
   sequence.width = sequence.height = 16384;
   CHECK(rpyrowave_block_layout(&sequence, 0, NULL) == 786432);
   sequence.width++;
   CHECK(!rpyrowave_block_layout(&sequence, 0, NULL));
   sequence.width = sequence.height = 1;
   CHECK(rpyrowave_block_layout(&sequence, 0, NULL) == 75);
   sequence.chroma_444 = 0;
   CHECK(!rpyrowave_block_layout(&sequence, 0, NULL));
}

static void test_file_header(void)
{
   static const uint8_t vector[24] =
   {
      'P', 'W', 'V', '1', 0x7f, 0xc0, 0x1f, 0,
      0, 0, 0, 0, 0x60, 0xea, 0, 0, 0xe9, 3, 0, 0,
      10, 1, 0, 0
   };
   rpyrowave_file_header_t header;
   uint8_t bytes[24];
   size_t i;
   CHECK(rpyrowave_read_file_header(vector, 24, &header) == 0);
   CHECK(header.sequence.width == 128 && header.sequence.height == 128);
   CHECK(header.sequence.sequence == 0 && header.sequence.total_blocks == 0);
   CHECK(header.frame_rate_num == 60000 && header.frame_rate_den == 1001);
   CHECK(header.reference_bit_depth == 10);
   for (i = 0; i < 24; i++)
      CHECK(rpyrowave_read_file_header(vector, i, &header)
            == RPYROWAVE_TRUNCATED);
   for (i = 0; i < 24; i++)
   {
      memcpy(bytes, vector, 24);
      if (i < 4 || i == 7 || (i >= 8 && i <= 10) || i >= 22)
      {
         bytes[i] |= i == 7 ? 0x10 : 1;
         if (i < 4)
            bytes[i] = 0;
         CHECK(rpyrowave_read_file_header(bytes, 24, &header)
               == RPYROWAVE_INVALID);
      }
   }
   memcpy(bytes, vector, 24);
   bytes[21] = 2;
   CHECK(rpyrowave_read_file_header(bytes, 24, &header)
         == RPYROWAVE_UNSUPPORTED);
   bytes[21] = 1;
   bytes[20] = 12;
   CHECK(rpyrowave_read_file_header(bytes, 24, &header) == RPYROWAVE_INVALID);
   bytes[20] = 0;
   memset(bytes + 16, 0, 4);
   CHECK(rpyrowave_read_file_header(bytes, 24, &header) == RPYROWAVE_INVALID);
   memset(bytes + 12, 0, 4);
   CHECK(rpyrowave_read_file_header(bytes, 24, &header) == 0);
}

static void test_encoder(float *output)
{
   static const uint8_t expected[16] =
   {
      1, 0, 4, 0x50, 0x20, 0x56, 0x34, 0x12,
      1, 0x40, 0, 1, 0x80, 2, 0, 0
   };
   int32_t *input = (int32_t*)calloc(1024, sizeof(*input));
   uint8_t *bytes = (uint8_t*)malloc(2488);
   rpyrowave_block_t block;
   size_t written = 99;
   unsigned i;
   CHECK(input && bytes);
   if (!input || !bytes)
   {
      free(input);
      free(bytes);
      return;
   }
   CHECK(rpyrowave_encode_block(input, 32, 0, 0, NULL, 0, &written) == 0);
   CHECK(!written);
   input[0] = 1;
   input[231] = -1;
   memset(bytes, 0xa5, 2488);
   CHECK(rpyrowave_encode_block(input, 32, 0x123456, 5,
            bytes, 15, &written) == RPYROWAVE_BUFFER_TOO_SMALL);
   CHECK(written == 16 && bytes[0] == 0xa5 && bytes[15] == 0xa5);
   CHECK(rpyrowave_encode_block(input, 32, 0x123456, 5,
            bytes, 16, &written) == 0);
   CHECK(written == 16 && !memcmp(bytes, expected, 16));
   for (i = 0; i < 1024; i++)
      input[i] = -262143;
   CHECK(rpyrowave_encode_block(input, 32, 0xffffff, 7,
            bytes, 2488, &written) == 0);
   CHECK(written == 2488 && bytes[8] == 255 && bytes[40] == 15);
   CHECK(rpyrowave_decode_block(bytes, written, output, &block) == 0);
   CHECK(block.sequence == 7 && block.index == 0xffffff);
   for (i = 0; i < 1024; i++)
      CHECK(output[i] == -65535.875f);
   input[0] = -262144;
   CHECK(rpyrowave_encode_block(input, 32, 0, 0, bytes, 2488, &written)
         == RPYROWAVE_INVALID);
   input[0] = 262144;
   CHECK(rpyrowave_encode_block(input, 32, 0, 0, bytes, 2488, &written)
         == RPYROWAVE_INVALID);
   free(input);
   free(bytes);
}

static void test_image(const char *path, unsigned width, unsigned height,
      unsigned chroma_444, unsigned impulse)
{
   FILE *file = fopen(path, "rb");
   FILE *reference;
   char reference_path[256];
   rpyrowave_decoder_t *decoder;
   rpyrowave_image_t image;
   uint8_t *data;
   long length;
   unsigned component;
   CHECK(file != NULL);
   if (!file)
      return;
   CHECK(!fseek(file, 0, SEEK_END));
   length = ftell(file);
   CHECK(length > 8 && length < 262144);
   if (length <= 8 || length >= 262144)
   {
      fclose(file);
      return;
   }
   rewind(file);
   data = (uint8_t*)malloc((size_t)length);
   CHECK(data != NULL);
   if (!data)
   {
      fclose(file);
      return;
   }
   CHECK(fread(data, 1, (size_t)length, file) == (size_t)length);
   fclose(file);
   decoder = rpyrowave_decoder_new(width, height, chroma_444, 1048576);
   CHECK(decoder != NULL);
   if (!decoder)
   {
      free(data);
      return;
   }
   CHECK(rpyrowave_decode_frame(decoder, data, (size_t)length, 0, &image) == 0);
   CHECK(strlen(path) + 5 < sizeof(reference_path));
   strcpy(reference_path, path);
   strcat(reference_path, ".u16");
   reference = fopen(reference_path, "rb");
   CHECK(reference != NULL);
   if (!reference)
   {
      rpyrowave_decoder_free(decoder);
      free(data);
      return;
   }
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component && !chroma_444 ? 1 : 0;
      unsigned x;
      unsigned y;
      for (y = 0; y < height >> shift; y++)
         for (x = 0; x < width >> shift; x++)
         {
            unsigned value = impulse ? (!x && !y ? 255 : 128)
               : (x * 3 + y * 5 + component * 37) & 255;
            float expected = (float)value / 255.0f;
            float actual   = image.planes[component][y * image.stride + x];
            int low        = fgetc(reference);
            int high       = fgetc(reference);
            float golden   = (float)(low + high * 256) / 65535.0f;
            CHECK(low >= 0 && high >= 0);
            CHECK(actual > golden - 0.003f && actual < golden + 0.003f);
            CHECK(actual > expected - 0.008f && actual < expected + 0.008f);
         }
   }
   CHECK(fgetc(reference) == EOF);
   fclose(reference);
   CHECK(rpyrowave_decode_frame(decoder, data, 8, 0, &image)
         == RPYROWAVE_TRUNCATED);
   CHECK(rpyrowave_decode_frame(decoder, data, 8, 1, &image) == 0);
   CHECK(image.planes[0][0] == 0.5f && image.planes[1][0] == 0.5f);
   CHECK(rpyrowave_decode_frame(decoder, data, (size_t)length - 1, 0, &image)
         == RPYROWAVE_TRUNCATED);
   data[0] ^= 2;
   CHECK(rpyrowave_decode_frame(decoder, data, (size_t)length, 0, &image)
         == RPYROWAVE_UNSUPPORTED);
   rpyrowave_decoder_free(decoder);
   free(data);
}

static void test_frame_encoder(unsigned width, unsigned height, unsigned chroma_444)
{
   static const size_t budgets[6] = { 8, 64, 512, 4096, 65536, 262144 };
   rpyrowave_encoder_t *encoder = rpyrowave_encoder_new(width, height, chroma_444, 1048576);
   rpyrowave_decoder_t *decoder = rpyrowave_decoder_new(width, height, chroma_444, 1048576);
   float *planes = (float*)malloc((size_t)width * height * 3 * sizeof(float));
   uint8_t *bytes = (uint8_t*)malloc(262144);
   rpyrowave_image_t input;
   rpyrowave_image_t output;
   size_t written;
   unsigned component;
   unsigned budget;
   CHECK(encoder && decoder && planes && bytes);
   if (!encoder || !decoder || !planes || !bytes)
      goto end;
   memset(&input, 0, sizeof(input));
   input.stride              = width;
   input.sequence.width      = width;
   input.sequence.height     = height;
   input.sequence.chroma_444 = chroma_444;
   input.sequence.transfer_pq = input.sequence.primaries_bt2020 = 1;
   input.sequence.transform_bt2020 = input.sequence.limited_range = 1;
   input.sequence.chroma_left = 1;
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component && !chroma_444 ? 1 : 0;
      unsigned x;
      unsigned y;
      float *plane = planes + (size_t)component * width * height;
      input.planes[component] = plane;
      for (y = 0; y < height >> shift; y++)
         for (x = 0; x < width >> shift; x++)
            plane[y * width + x] = (float)((x * 3 + y * 5 + component * 37) & 255) / 255.0f;
   }
   for (budget = 0; budget < 6; budget++)
   {
      size_t required;
      input.sequence.sequence = budget;
      CHECK(rpyrowave_encode_frame(encoder, &input, budgets[budget], NULL, 0,
               &required) == RPYROWAVE_BUFFER_TOO_SMALL);
      CHECK(required >= 8 && required <= budgets[budget]);
      memset(bytes, 0xa5, 262144);
      CHECK(rpyrowave_encode_frame(encoder, &input, budgets[budget], bytes,
               required - 1, &written) == RPYROWAVE_BUFFER_TOO_SMALL);
      CHECK(bytes[0] == 0xa5 && bytes[required - 1] == 0xa5);
      CHECK(rpyrowave_encode_frame(encoder, &input, budgets[budget], bytes,
               262144, &written) == RPYROWAVE_OK);
      CHECK(written == required);
      CHECK(rpyrowave_decode_frame(decoder, bytes, written, 0, &output) == 0);
      CHECK(output.sequence.sequence == budget && output.sequence.transfer_pq);
      CHECK(output.sequence.chroma_left && output.sequence.limited_range);
      if (budget == 0)
         CHECK(written == 8 && output.planes[0][0] == 0.5f);
      if (budget == 5)
         for (component = 0; component < 3; component++)
         {
            unsigned shift = component && !chroma_444 ? 1 : 0;
            unsigned x;
            unsigned y;
            for (y = 0; y < height >> shift; y++)
               for (x = 0; x < width >> shift; x++)
               {
                  float a = input.planes[component][y * input.stride + x];
                  float b = output.planes[component][y * output.stride + x];
                  CHECK(a > b - 0.003f && a < b + 0.003f);
               }
         }
   }
   input.sequence.sequence = 8;
   CHECK(rpyrowave_encode_frame(encoder, &input, 262144, bytes, 262144,
            &written) == RPYROWAVE_INVALID);
   input.sequence.sequence = 0;
   CHECK(rpyrowave_encode_frame(encoder, &input, 7, bytes, 262144,
            &written) == RPYROWAVE_INVALID);
   input.stride = (size_t)-1;
   CHECK(rpyrowave_encode_frame(encoder, &input, 262144, bytes, 262144,
            &written) == RPYROWAVE_INVALID);
   input.stride = width;
   planes[0] = -0.1f;
   CHECK(rpyrowave_encode_frame(encoder, &input, 262144, bytes, 262144,
            &written) == RPYROWAVE_INVALID);
end:
   free(bytes);
   free(planes);
   rpyrowave_decoder_free(decoder);
   rpyrowave_encoder_free(encoder);
}

typedef struct rpy_plan_test
{
   unsigned calls;
   unsigned fail_at;
} rpy_plan_test_t;

static int test_plan_record(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_plan_test_t *test = (rpy_plan_test_t*)userdata;
   CHECK(source == (test->calls & 1));
   CHECK(parameters->image_width == 160 && parameters->image_height == 128);
   CHECK(parameters->reserved == 0 && parameters->level <= 4);
   CHECK(x == ((160U >> parameters->level) + 7) / 8);
   CHECK(y == ((128U >> parameters->level) + 7) / 8 && z == 3);
   test->calls++;
   return test->calls != test->fail_at;
}

static void test_plan(void)
{
   rpy_plan_test_t test;
   test.calls = test.fail_at = 0;
   CHECK(!rpyrowave_dwt_record(160, 128, 0, 0, test_plan_record, &test));
   CHECK(test.calls == 50);
   test.calls = 0;
   CHECK(!rpyrowave_dwt_record(160, 128, 1, 1, test_plan_record, &test));
   CHECK(test.calls == 50);
   test.calls = 0;
   test.fail_at = 17;
   CHECK(rpyrowave_dwt_record(160, 128, 1, 1, test_plan_record, &test)
         == RPYROWAVE_INVALID);
   CHECK(test.calls == 17);
   test.calls = 0;
   CHECK(rpyrowave_dwt_record(161, 128, 1, 0, test_plan_record, &test)
         == RPYROWAVE_INVALID);
   CHECK(!test.calls);
}

typedef struct rpy_packet_plan_test
{
   unsigned calls;
   unsigned fail_at;
   unsigned maximum_y;
} rpy_packet_plan_test_t;

static int test_packet_record(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_packet_plan_test_t *test = (rpy_packet_plan_test_t*)userdata;
   CHECK(x > 0 && x <= 65535 && y > 0 && y <= 65535);
   CHECK(parameters->operation <= RPYROWAVE_PACKET_UNPACK && !parameters->reserved);
   test->calls++;
   if (y > test->maximum_y)
      test->maximum_y = y;
   return test->calls != test->fail_at;
}

static void test_gpu_packets(void)
{
   static const uint8_t start[8] = { 127, 0xc0, 0x1f, 0xd0, 1, 0, 0, 4 };
   rpyrowave_sequence_t sequence;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_gpu_layout_t bad;
   rpyrowave_packet_parameters_t parameters;
   rpy_packet_plan_test_t plan;
   rpyrowave_gpu_block_t *blocks;
   uint8_t *workspace;
   uint8_t original[48];
   size_t offset;
   CHECK(!rpyrowave_read_sequence(start, 8, &sequence));
   CHECK(!rpyrowave_gpu_layout(&sequence, 262144, &layout));
   CHECK(layout.block_count == 75 && layout.wire_offset == 1280);
   CHECK(layout.workspace_bytes == 263424);
   workspace = (uint8_t*)calloc(1, layout.workspace_bytes);
   blocks = (rpyrowave_gpu_block_t*)malloc(layout.block_count * sizeof(*blocks));
   CHECK(workspace && blocks);
   if (!workspace || !blocks)
   {
      free(workspace);
      free(blocks);
      return;
   }
   CHECK(!rpyrowave_gpu_blocks(&layout, blocks, 75));
   CHECK(blocks[0].origin == 0 && blocks[0].width == 4 && blocks[0].height == 4);
   CHECK(blocks[4].origin == 16384 && blocks[4].index == 4);
   CHECK(rpyrowave_gpu_blocks(&layout, blocks, 74) == RPYROWAVE_INVALID);
   offset = layout.wire_offset;
   memcpy(workspace + offset, start, 8);
   memcpy(workspace + offset + 8, block_vector, 20);
   memset(workspace + offset + 13, 0, 3);
   memcpy(original, workspace + offset, 28);
   CHECK(!rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes, 28, 0, &parameters));
   CHECK(parameters.operation == RPYROWAVE_PACKET_UNPACK && parameters.maximum_bytes == 28);
   CHECK(workspace[64] == 20 && workspace[68] == 8 && workspace[8] == 1);
   CHECK(!memcmp(original, workspace + offset, 28));
   memcpy(workspace + offset + 28, workspace + offset + 8, 20);
   memcpy(original, workspace + offset, 48);
   CHECK(!rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes, 48, 0, &parameters));
   CHECK(workspace[8] == 1 && !memcmp(original, workspace + offset, 48));
   CHECK(rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes, 8, 0, &parameters)
         == RPYROWAVE_TRUNCATED);
   CHECK(!rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes, 8, 1, &parameters));
   CHECK(workspace[8] == 0 && workspace[64] == 0);
   CHECK(rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes, 27, 0, &parameters)
         == RPYROWAVE_TRUNCATED);
   bad = layout;
   bad.wire_offset = 0xffffffffU;
   CHECK(rpyrowave_gpu_index_frame(&bad, workspace, layout.workspace_bytes, 28, 0, &parameters)
         == RPYROWAVE_INVALID);
   memset(&plan, 0, sizeof(plan));
   CHECK(!rpyrowave_gpu_encode_record(&layout, &sequence, 4096, -1, test_packet_record, &plan));
   CHECK(plan.calls == 20 && plan.maximum_y == 1);
   plan.calls = 0;
   CHECK(!rpyrowave_gpu_encode_record(&layout, &sequence, 4096, 128, test_packet_record, &plan));
   CHECK(plan.calls == 4);
   plan.calls = 0;
   plan.fail_at = 7;
   CHECK(rpyrowave_gpu_encode_record(&layout, &sequence, 4096, -1, test_packet_record, &plan)
         == RPYROWAVE_INVALID);
   CHECK(plan.calls == 7);
   plan.calls = 0;
   CHECK(rpyrowave_gpu_encode_record(&layout, &sequence, 262145, -1, test_packet_record, &plan)
         == RPYROWAVE_INVALID);
   CHECK(!plan.calls);
   CHECK(rpyrowave_gpu_layout(&sequence, 0xffffffffU, &bad) == RPYROWAVE_INVALID);
   sequence.width = sequence.height = 16384;
   CHECK(!rpyrowave_gpu_layout(&sequence, 262144, &bad));
   CHECK(bad.block_count == 786432);
   CHECK(!rpyrowave_gpu_layout(&sequence, 0xfffffffcU - bad.wire_offset, &bad));
   memset(&plan, 0, sizeof(plan));
   CHECK(!rpyrowave_gpu_encode_record(&bad, &sequence, bad.maximum_bytes,
            -1, test_packet_record, &plan));
   CHECK(plan.maximum_y == 129);
   free(workspace);
   free(blocks);
}

static void test_gpu_color(void)
{
   rpyrowave_sequence_t sequence;
   rpyrowave_color_parameters_t parameters;
   rpyrowave_color_parameters_t saved;
   memset(&sequence, 0, sizeof(sequence));
   sequence.width = 130;
   sequence.height = 98;
   CHECK(!rpyrowave_gpu_color_parameters(&sequence, 8, 0, 1, 134, 104, 2, 3, &parameters));
   CHECK(parameters.image_width == 160 && parameters.image_height == 128);
   CHECK(sizeof(parameters) == 80 && parameters.texture_mode == RPYROWAVE_COLOR_CODE && !parameters.colorimetry);
   CHECK(parameters.width == 130 && parameters.height == 98);
   CHECK(parameters.texture_x == 2 && parameters.texture_y == 3 && parameters.flip_y == 1);
   CHECK(fabs(parameters.chroma_center - 128.0 / 255.0) < 0.000001);
   CHECK(parameters.luma_offset == 0.0f && parameters.luma_scale == 1.0f);
   CHECK(!rpyrowave_gpu_color_parameters_ex(&sequence, 8, 0, 1, 65, 49, 0, 0, 1, &parameters));
   CHECK(parameters.width == 130 && parameters.height == 98 && parameters.flip_y == 1);
   CHECK(parameters.padding[0] == 0.5f && parameters.padding[1] == 0.5f);
   CHECK(!rpyrowave_gpu_color_parameters_ex(&sequence, 8, 0, 0, 260, 196, 0, 0, 1, &parameters));
   CHECK(parameters.padding[0] == 2.0f && parameters.padding[1] == 2.0f);
   CHECK(rpyrowave_gpu_color_parameters_ex(&sequence, 8, 0, 0, 0, 196, 0, 0, 1, &parameters) == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters_ex(&sequence, 8, 0, 0, 260, 196, 1, 0, 1, &parameters) == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters_ex(&sequence, 8, 0, 0, 260, 196, 0, 0, 2, &parameters) == RPYROWAVE_INVALID);
   sequence.limited_range = 1;
   sequence.transform_bt2020 = 1;
   sequence.primaries_bt2020 = sequence.transfer_pq = 1;
   CHECK(!rpyrowave_gpu_color_parameters(&sequence, 10, 1, 0, 130, 98, 0, 0, &parameters));
   CHECK(parameters.inverse == 1 && parameters.flip_y == 0);
   CHECK(parameters.colorimetry == 3);
   CHECK(fabs(parameters.red - 0.2627) < 0.000001);
   CHECK(fabs(parameters.blue - 0.0593) < 0.000001);
   CHECK(fabs(parameters.luma_offset - 64.0 / 1023.0) < 0.000001);
   CHECK(fabs(parameters.luma_scale - 876.0 / 1023.0) < 0.000001);
   CHECK(fabs(parameters.chroma_scale - 896.0 / 1023.0) < 0.000001);
   CHECK(fabs(parameters.chroma_center - 512.0 / 1023.0) < 0.000001);
   saved = parameters;
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 7, 1, 0, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 17, 1, 0, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 2, 0, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 1, 2, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 1, 0, 130, 98, 1, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 1, 0, 130, 98, 0, 1, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 1, 0, 130, 98, 0xffffffffU, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(rpyrowave_gpu_color_parameters(NULL, 10, 1, 0, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   CHECK(!memcmp(&saved, &parameters, sizeof(saved)));
   sequence.width = 129;
   CHECK(rpyrowave_gpu_color_parameters(&sequence, 10, 1, 0, 130, 98, 0, 0, &parameters)
         == RPYROWAVE_INVALID);
   sequence.chroma_444 = 1;
   CHECK(!rpyrowave_gpu_color_parameters(&sequence, 16, 1, 0, 130, 98, 0, 0, &parameters));
   CHECK(fabs(parameters.chroma_center - 32768.0 / 65535.0) < 0.000001);
}

static void test_reference_images(const char *directory)
{
   static const char *names[4] = {
      "ramp-130x98-420.pyro", "ramp-321x181-444.pyro",
      "impulse-2x2-420.pyro", "ramp-1x1-444.pyro"
   };
   static const unsigned widths[4] = {130, 321, 2, 1};
   static const unsigned heights[4] = {98, 181, 2, 1};
   char path[256];
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      if (strlen(directory) + strlen(names[i]) + 2 > sizeof(path))
      {
         CHECK(0);
         return;
      }
      strcpy(path, directory);
      strcat(path, "/");
      strcat(path, names[i]);
      test_image(path, widths[i], heights[i], i & 1, i == 2);
   }
}

int main(int argc, char **argv)
{
   float *output = (float*)malloc(1024 * sizeof(*output));
   if (!output)
      return EXIT_FAILURE;
   if (argc != 1 && (argc != 3 || strcmp(argv[1], "--reference")))
   {
      fprintf(stderr, "Usage: %s [--reference directory]\n", argv[0]);
      free(output);
      return EXIT_FAILURE;
   }
   test_sequence();
   test_block(output);
   test_group_positions(output);
   test_layout();
   test_file_header();
   test_encoder(output);
   CHECK(!rpyrowave_decoder_new(0, 128, 1, 16384));
   CHECK(!rpyrowave_decoder_new(128, 128, 1, 16383));
   CHECK(!rpyrowave_decoder_new(127, 128, 0, 16384));
   if (argc == 3)
      test_reference_images(argv[2]);
   test_frame_encoder(1, 1, 1);
   test_frame_encoder(130, 98, 0);
   test_frame_encoder(321, 181, 1);
   test_plan();
   test_gpu_packets();
   test_gpu_color();
   free(output);
   if (failures)
   {
      fprintf(stderr, "%d checks failed\n", failures);
      return EXIT_FAILURE;
   }
   puts("PyroWave specification vectors and bounds: PASS");
   return EXIT_SUCCESS;
}
