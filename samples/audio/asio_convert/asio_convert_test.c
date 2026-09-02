/* ASIO sample conversion, every type, against the specification.
 *
 * audio/drivers/asio_convert.h turns interleaved float stereo into two
 * hardware halves in the device's ASIOSampleType. The old converter
 * wrote five types and memset the rest to silence, the Int32LSB24
 * family that pro interfaces report included. These fixtures are what
 * the specification says each type holds for a handful of values -
 * full scale both ways, zero, a half, a small value past the rounding
 * boundary - and the byte order of each; a value's expected bytes are
 * written by hand from the type's definition, not derived from the
 * code under test. Saturation, the silence fill for frames past what
 * was supplied, and the fades at an underrun's edges are checked
 * too. */

#include <stdio.h>
#include <string.h>

#include "../../../audio/drivers/asio_convert.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* Inputs: the left channel is the value under test, the right its
 * negation, so one frame exercises both signs. Frame 4 is beyond what
 * is supplied: it must be silence. */
static const float in_left[4] = { 1.0f, 0.0f, 0.5f, 0.000030517578125f /* 2^-15 */ };
#define FRAMES_IN  4
#define FRAMES_OUT 4

static void run(const char *name, ASIOSampleType type, size_t bps,
      const unsigned char *expect_l, const unsigned char *expect_r)
{
   float src[FRAMES_IN * 2];
   unsigned char out_l[FRAMES_OUT * 8], out_r[FRAMES_OUT * 8];
   size_t i;

   for (i = 0; i < FRAMES_IN; i++)
   {
      src[i * 2 + 0] = in_left[i];
      src[i * 2 + 1] = -in_left[i];
   }
   memset(out_l, 0xAA, sizeof(out_l));
   memset(out_r, 0xAA, sizeof(out_r));

   CHECK(asio_bytes_per_sample(type) == bps,
         "%s: bytes per sample %u, expected %u", name,
         (unsigned)asio_bytes_per_sample(type), (unsigned)bps);
   CHECK(asio_convert_known(type), "%s: reported unknown", name);

   asio_convert_frames(type, src, FRAMES_IN, FRAMES_OUT, out_l, out_r, false);

   for (i = 0; i < FRAMES_IN * bps; i++)
   {
      if (out_l[i] != expect_l[i] || out_r[i] != expect_r[i])
      {
         size_t f = i / bps;
         printf("FAIL %s frame %u: left ", name, (unsigned)f);
         { size_t k; for (k = 0; k < bps; k++) printf("%02x", out_l[f * bps + k]); }
         printf(" expected ");
         { size_t k; for (k = 0; k < bps; k++) printf("%02x", expect_l[f * bps + k]); }
         printf("; right ");
         { size_t k; for (k = 0; k < bps; k++) printf("%02x", out_r[f * bps + k]); }
         printf(" expected ");
         { size_t k; for (k = 0; k < bps; k++) printf("%02x", expect_r[f * bps + k]); }
         printf("\n");
         failures++;
         i = (f + 1) * bps - 1;
      }
   }
}

/* Fixtures. Values: +1.0 saturates to max, -1.0 is exactly min; 0 is 0;
 * 0.5 is half scale (2^(bits-2)), -0.5 its negation; 2^-15 is one
 * 16-bit step: at 16 bits exactly 1, at 24 bits 256, at 32 bits 65536,
 * and its negation. Little-endian bytes low first; big-endian high
 * first. */

/* Int16: max 7fff, min 8000, half 4000, step 0001. */
static const unsigned char i16le_l[] = { 0xff,0x7f, 0x00,0x00, 0x00,0x40, 0x01,0x00 };
static const unsigned char i16le_r[] = { 0x00,0x80, 0x00,0x00, 0x00,0xc0, 0xff,0xff };
static const unsigned char i16be_l[] = { 0x7f,0xff, 0x00,0x00, 0x40,0x00, 0x00,0x01 };
static const unsigned char i16be_r[] = { 0x80,0x00, 0x00,0x00, 0xc0,0x00, 0xff,0xff };

/* Int24: max 7fffff, min 800000, half 400000, step 000100. */
static const unsigned char i24le_l[] = { 0xff,0xff,0x7f, 0,0,0, 0x00,0x00,0x40, 0x00,0x01,0x00 };
static const unsigned char i24le_r[] = { 0x00,0x00,0x80, 0,0,0, 0x00,0x00,0xc0, 0x00,0xff,0xff };
static const unsigned char i24be_l[] = { 0x7f,0xff,0xff, 0,0,0, 0x40,0x00,0x00, 0x00,0x01,0x00 };
static const unsigned char i24be_r[] = { 0x80,0x00,0x00, 0,0,0, 0xc0,0x00,0x00, 0xff,0xff,0x00 };

/* Int32: max 7fffffff, min 80000000, half 40000000, step 00010000. */
static const unsigned char i32le_l[] = { 0xff,0xff,0xff,0x7f, 0,0,0,0, 0x00,0x00,0x00,0x40, 0x00,0x00,0x01,0x00 };
static const unsigned char i32le_r[] = { 0x00,0x00,0x00,0x80, 0,0,0,0, 0x00,0x00,0x00,0xc0, 0x00,0x00,0xff,0xff };
static const unsigned char i32be_l[] = { 0x7f,0xff,0xff,0xff, 0,0,0,0, 0x40,0x00,0x00,0x00, 0x00,0x01,0x00,0x00 };
static const unsigned char i32be_r[] = { 0x80,0x00,0x00,0x00, 0,0,0,0, 0xc0,0x00,0x00,0x00, 0xff,0xff,0x00,0x00 };

/* Int32LSB24: 24 significant bits in the low bits of the word, sign
 * extended. max 007fffff, min ff800000, half 00400000, step 00000100;
 * negatives carry the extension: -half ffc00000, -step ffffff00. */
static const unsigned char i32l24le_l[] = { 0xff,0xff,0x7f,0x00, 0,0,0,0, 0x00,0x00,0x40,0x00, 0x00,0x01,0x00,0x00 };
static const unsigned char i32l24le_r[] = { 0x00,0x00,0x80,0xff, 0,0,0,0, 0x00,0x00,0xc0,0xff, 0x00,0xff,0xff,0xff };
static const unsigned char i32m24be_l[] = { 0x00,0x7f,0xff,0xff, 0,0,0,0, 0x00,0x40,0x00,0x00, 0x00,0x00,0x01,0x00 };
static const unsigned char i32m24be_r[] = { 0xff,0x80,0x00,0x00, 0,0,0,0, 0xff,0xc0,0x00,0x00, 0xff,0xff,0xff,0x00 };

/* Int32LSB16: 16 bits in the low bits. max 00007fff, min ffff8000,
 * half 00004000, step 00000001. */
static const unsigned char i32l16le_l[] = { 0xff,0x7f,0x00,0x00, 0,0,0,0, 0x00,0x40,0x00,0x00, 0x01,0x00,0x00,0x00 };
static const unsigned char i32l16le_r[] = { 0x00,0x80,0xff,0xff, 0,0,0,0, 0x00,0xc0,0xff,0xff, 0xff,0xff,0xff,0xff };

/* Int32LSB20: 20 bits. max 0007ffff, min fff80000, half 00040000,
 * step 2^-15 * 2^19 = 16 -> 00000010. */
static const unsigned char i32l20le_l[] = { 0xff,0xff,0x07,0x00, 0,0,0,0, 0x00,0x00,0x04,0x00, 0x10,0x00,0x00,0x00 };
static const unsigned char i32l20le_r[] = { 0x00,0x00,0xf8,0xff, 0,0,0,0, 0x00,0x00,0xfc,0xff, 0xf0,0xff,0xff,0xff };

/* Int32LSB18: 18 bits. max 0001ffff, min fffe0000, half 00010000,
 * step 2^-15 * 2^17 = 4 -> 00000004. */
static const unsigned char i32l18le_l[] = { 0xff,0xff,0x01,0x00, 0,0,0,0, 0x00,0x00,0x01,0x00, 0x04,0x00,0x00,0x00 };
static const unsigned char i32l18le_r[] = { 0x00,0x00,0xfe,0xff, 0,0,0,0, 0x00,0x00,0xff,0xff, 0xfc,0xff,0xff,0xff };

/* Float32: IEEE bit patterns. 1.0 3f800000, -1.0 bf800000, 0 and its
 * negation -0.0 80000000 - a distinct pattern the converter must keep - 0.5
 * 3f000000, -0.5 bf000000, 2^-15 38000000, -2^-15 b8000000. */
static const unsigned char f32le_l[] = { 0x00,0x00,0x80,0x3f, 0,0,0,0, 0x00,0x00,0x00,0x3f, 0x00,0x00,0x00,0x38 };
static const unsigned char f32le_r[] = { 0x00,0x00,0x80,0xbf, 0,0,0,0x80, 0x00,0x00,0x00,0xbf, 0x00,0x00,0x00,0xb8 };
static const unsigned char f32be_l[] = { 0x3f,0x80,0x00,0x00, 0,0,0,0, 0x3f,0x00,0x00,0x00, 0x38,0x00,0x00,0x00 };
static const unsigned char f32be_r[] = { 0xbf,0x80,0x00,0x00, 0x80,0,0,0, 0xbf,0x00,0x00,0x00, 0xb8,0x00,0x00,0x00 };

/* Float64: 1.0 3ff0000000000000, -1.0 bff0..., 0.5 3fe0..., -0.5 bfe0...,
 * 2^-15 3f00000000000000, -2^-15 bf00.... */
static const unsigned char f64le_l[] = { 0,0,0,0,0,0,0xf0,0x3f, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0xe0,0x3f, 0,0,0,0,0,0,0x00,0x3f };
static const unsigned char f64le_r[] = { 0,0,0,0,0,0,0xf0,0xbf, 0,0,0,0,0,0,0,0x80, 0,0,0,0,0,0,0xe0,0xbf, 0,0,0,0,0,0,0x00,0xbf };
static const unsigned char f64be_l[] = { 0x3f,0xf0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0x3f,0xe0,0,0,0,0,0,0, 0x3f,0x00,0,0,0,0,0,0 };
static const unsigned char f64be_r[] = { 0xbf,0xf0,0,0,0,0,0,0, 0x80,0,0,0,0,0,0,0, 0xbf,0xe0,0,0,0,0,0,0, 0xbf,0x00,0,0,0,0,0,0 };

int main(void)
{
   run("Int16LSB",    ASIOSTInt16LSB,    2, i16le_l,    i16le_r);
   run("Int16MSB",    ASIOSTInt16MSB,    2, i16be_l,    i16be_r);
   run("Int24LSB",    ASIOSTInt24LSB,    3, i24le_l,    i24le_r);
   run("Int24MSB",    ASIOSTInt24MSB,    3, i24be_l,    i24be_r);
   run("Int32LSB",    ASIOSTInt32LSB,    4, i32le_l,    i32le_r);
   run("Int32MSB",    ASIOSTInt32MSB,    4, i32be_l,    i32be_r);
   run("Int32LSB24",  ASIOSTInt32LSB24,  4, i32l24le_l, i32l24le_r);
   run("Int32MSB24",  ASIOSTInt32MSB24,  4, i32m24be_l, i32m24be_r);
   run("Int32LSB16",  ASIOSTInt32LSB16,  4, i32l16le_l, i32l16le_r);
   run("Int32LSB20",  ASIOSTInt32LSB20,  4, i32l20le_l, i32l20le_r);
   run("Int32LSB18",  ASIOSTInt32LSB18,  4, i32l18le_l, i32l18le_r);
   run("Float32LSB",  ASIOSTFloat32LSB,  4, f32le_l,    f32le_r);
   run("Float32MSB",  ASIOSTFloat32MSB,  4, f32be_l,    f32be_r);
   run("Float64LSB",  ASIOSTFloat64LSB,  8, f64le_l,    f64le_r);
   run("Float64MSB",  ASIOSTFloat64MSB,  8, f64be_l,    f64be_r);

   /* Audio that runs out mid period: the frames past it are silence,
    * and the last ASIO_FADE_FRAMES of it ramp down to it - the last one
    * exactly zero, the first of the ramp near full - so the edge does
    * not click. Frames before the ramp are untouched. */
   {
      float src[100 * 2];
      int16_t l[128], r[128];
      int i;
      for (i = 0; i < 100; i++) { src[i * 2] = 0.5f; src[i * 2 + 1] = -0.5f; }
      asio_convert_frames(ASIOSTInt16LSB, src, 100, 128, l, r, false);
      CHECK(l[0] == 16384 && r[0] == -16384, "run-out: frames before the ramp were changed");
      CHECK(l[100 - ASIO_FADE_FRAMES - 1] == 16384, "run-out: frame before the ramp was changed");
      CHECK(l[99] == 0 && r[99] == 0, "run-out: the last supplied frame is not zero (%d)", l[99]);
      CHECK(l[100 - ASIO_FADE_FRAMES] > 15000, "run-out: the ramp does not start near full (%d)",
            l[100 - ASIO_FADE_FRAMES]);
      CHECK(l[100 - ASIO_FADE_FRAMES / 2] > 6000 && l[100 - ASIO_FADE_FRAMES / 2] < 10000,
            "run-out: midway through the ramp is not about half (%d)", l[100 - ASIO_FADE_FRAMES / 2]);
      for (i = 100; i < 128; i++)
         CHECK(l[i] == 0 && r[i] == 0, "run-out: frame %d past the supplied ones is not silence", i);
   }

   /* Audio returning after silence ramps up over its first frames. */
   {
      float src[64 * 2];
      int16_t l[64], r[64];
      int i;
      for (i = 0; i < 64; i++) { src[i * 2] = 0.5f; src[i * 2 + 1] = -0.5f; }
      asio_convert_frames(ASIOSTInt16LSB, src, 64, 64, l, r, true);
      CHECK(l[0] < 1000 && l[0] > 0, "fade-in: the first frame is not near zero (%d)", l[0]);
      CHECK(l[ASIO_FADE_FRAMES - 1] == 16384, "fade-in: the last ramp frame is not full (%d)",
            l[ASIO_FADE_FRAMES - 1]);
      CHECK(l[63] == 16384 && r[63] == -16384, "fade-in: frames after the ramp were changed");
   }

   /* A full period with no fade asked for is untouched at both ends. */
   {
      float src[64 * 2];
      int16_t l[64], r[64];
      int i;
      for (i = 0; i < 64; i++) { src[i * 2] = 0.5f; src[i * 2 + 1] = -0.5f; }
      asio_convert_frames(ASIOSTInt16LSB, src, 64, 64, l, r, false);
      CHECK(l[0] == 16384 && l[63] == 16384, "full period: a fade was applied (%d, %d)", l[0], l[63]);
   }

   /* Saturation: 1.5 clamps to max, -1.5 to min, at 16 bits. */
   {
      float src[2] = { 1.5f, -1.5f };
      unsigned char l[2], r[2];
      asio_convert_frames(ASIOSTInt16LSB, src, 1, 1, l, r, false);
      CHECK(l[0] == 0xff && l[1] == 0x7f, "saturation: +1.5 did not clamp to max");
      CHECK(r[0] == 0x00 && r[1] == 0x80, "saturation: -1.5 did not clamp to min");
   }

   /* An unknown type is silence for every frame, and says it is unknown. */
   {
      float src[2] = { 1.0f, -1.0f };
      unsigned char l[4], r[4];
      memset(l, 0xAA, 4); memset(r, 0xAA, 4);
      CHECK(!asio_convert_known(99L), "type 99 reported as convertible");
      asio_convert_frames(99L, src, 1, 1, l, r, false);
      CHECK(l[0] == 0 && l[3] == 0 && r[0] == 0 && r[3] == 0,
            "unknown type: not silence");
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("asio convert: every sample type the specification defines, in its byte layout\n");
   return 0;
}
