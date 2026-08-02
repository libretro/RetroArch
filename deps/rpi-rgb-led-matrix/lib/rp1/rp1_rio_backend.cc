// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#include "rp1_rio_backend.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../framebuffer-internal.h"
#include "../gpio.h"
#include "../hardware-mapping.h"

namespace rgb_matrix {
namespace internal {
namespace {

// Pi 5 RIO path:
// - this backend bypasses the RP1 PIO engine entirely and toggles the RP1 RIO
//   GPIO fabric directly through MMIO.
// - it shares the same framebuffer data and row-order decisions as the PIO
//   path, but emits clock/latch/OE transitions from the CPU instead of from a
//   state machine.
// - because timing comes from busy-waits and MMIO stores, this path is opt-in
//   and validates its GPIO mapping more aggressively than the generic backend.

static const uint32_t kRioOutputPinCount = 28;
static const uint32_t kGpioFunctionSysRio = 5;
static const uint32_t kPadFastDrive = 0x15;
static const uint32_t kPadSlowDrive = 0x01;
static const off_t kPi5PeripheralBase = 0x1f000d0000ll;
static const size_t kMapSizeBytes = 0x40000;
static const uint32_t kGpioOffsetWords = 0x00000 / 4;
static const uint32_t kRioOffsetWords = 0x10000 / 4;
static const uint32_t kPadOffsetWords = 0x20000 / 4;
static const double kBaseTargetPixelClockHz = 20000000.0;

struct GpioCtrlRegs {
  volatile uint32_t status;
  volatile uint32_t ctrl;
};

struct RioRegs {
  volatile uint32_t Out;
  volatile uint32_t OE;
  volatile uint32_t In;
  volatile uint32_t InSync;
};

struct RioMapCandidate {
  const char *path;
  off_t offset;
};

struct Rp1RioState {
  Rp1RioState()
      : active(false),
        panel_init_warned(false),
        map_base(NULL),
        gpio_regs(NULL),
        pad_regs(NULL),
        rio_out(NULL),
        rio_set(NULL),
        rio_clr(NULL),
        row_address_type(0),
        double_rows(0),
        parallel(0),
        gpio_slowdown(1),
        output_enable_bit(0),
        clock_bit(0),
        latch_bit(0),
        used_mask(0),
        word_time_nanos(0.0),
        has_pending_active_words(false),
        pending_addr(0),
        pending_active_words(0) {
  }

  bool active;
  bool panel_init_warned;
  // map_base spans one contiguous RP1 aperture containing GPIO control,
  // RIO data registers, and pad control registers.
  volatile uint32_t *map_base;
  GpioCtrlRegs *gpio_regs;
  volatile uint32_t *pad_regs;
  RioRegs *rio_out;
  RioRegs *rio_set;
  RioRegs *rio_clr;
  int row_address_type;
  int double_rows;
  int parallel;
  int gpio_slowdown;
  uint32_t output_enable_bit;
  uint32_t clock_bit;
  uint32_t latch_bit;
  uint32_t used_mask;
  double word_time_nanos;
  bool has_pending_active_words;
  uint32_t pending_addr;
  int pending_active_words;
  std::vector<int> bitplane_active_words;
};

Rp1RioState s_rio_state;
bool s_rio_requested = false;

static inline void ClockSetupDelay(int slowdown) {
  const int loops = std::max(1, slowdown);
  for (int i = 0; i < loops; ++i) {
    asm volatile("nop; nop;" ::: "memory");
  }
}

static inline void IoStoreBarrier() {
#if defined(__ARM_ARCH) && __ARM_ARCH >= 7
  asm volatile("dmb ishst" ::: "memory");
#endif
}

static uint64_t MonotonicNanos() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

static void BusyWaitNanos(uint64_t nanos) {
  if (nanos == 0) return;
  if (nanos <= 250) {
    for (int i = 0; i < 8; ++i) {
      asm volatile("nop" ::: "memory");
    }
    return;
  }

  const uint64_t start = MonotonicNanos();
  while ((MonotonicNanos() - start) < nanos) {
    asm volatile("" ::: "memory");
  }
}

static void BusyWaitWords(int word_count) {
  if (word_count <= 0) return;
  BusyWaitNanos(static_cast<uint64_t>(word_count * s_rio_state.word_time_nanos));
}

static double TargetPixelClockHz() {
  const int divisor = std::max(1, s_rio_state.gpio_slowdown);
  return kBaseTargetPixelClockHz / divisor;
}

static bool ReadSmallTextFile(const char *path, std::string *result) {
  const int fd = open(path, O_RDONLY);
  if (fd < 0) return false;

  char buffer[256];
  const ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (len <= 0) return false;

  buffer[len] = '\0';
  result->assign(buffer, buffer + len);
  return true;
}

static bool FileExists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static bool MappingNameSupported(const char *hardware_mapping) {
  if (hardware_mapping == NULL || *hardware_mapping == '\0') {
    hardware_mapping = "regular";
  }
  // This list reflects the mappings we have validated against the RP1 0..27
  // GPIO range and the one-bit-per-signal constraints below.
  return strcasecmp(hardware_mapping, "regular") == 0
      || strcasecmp(hardware_mapping, "classic") == 0
      || strcasecmp(hardware_mapping, "adafruit-hat") == 0
      || strcasecmp(hardware_mapping, "adafruit-hat-pwm") == 0;
}

static uint32_t CollectColorMask(const HardwareMapping &h, int parallel) {
  uint32_t mask = 0;
  mask |= h.p0_r1 | h.p0_g1 | h.p0_b1 | h.p0_r2 | h.p0_g2 | h.p0_b2;
  if (parallel >= 2) {
    mask |= h.p1_r1 | h.p1_g1 | h.p1_b1 | h.p1_r2 | h.p1_g2 | h.p1_b2;
  }
  if (parallel >= 3) {
    mask |= h.p2_r1 | h.p2_g1 | h.p2_b1 | h.p2_r2 | h.p2_g2 | h.p2_b2;
  }
  return mask;
}

static uint32_t CollectAddressMask(const HardwareMapping &h, int double_rows,
                                   int row_address_type) {
  switch (row_address_type) {
  case 0: {
    uint32_t mask = h.a;
    if (double_rows > 2) mask |= h.b;
    if (double_rows > 4) mask |= h.c;
    if (double_rows > 8) mask |= h.d;
    if (double_rows > 16) mask |= h.e;
    return mask;
  }
  case 1:
    return h.a | h.b;
  case 2:
    return h.a | h.b | h.c | h.d;
  case 3:
    return h.a | h.c;
  case 4: {
    uint32_t mask = h.a | h.b | h.c;
    if (double_rows > 8) mask |= h.d;
    if (double_rows > 16) mask |= h.e;
    return mask;
  }
  case 5:
    return h.a | h.b | h.c;
  default:
    return 0;
  }
}

static uint32_t BuildUsedMask(const HardwareMapping &h, int double_rows,
                              int parallel, int row_address_type) {
  uint32_t mask = h.output_enable | h.clock | h.strobe;
  mask |= CollectAddressMask(h, double_rows, row_address_type);
  mask |= CollectColorMask(h, parallel);
  return mask;
}

static bool FitsInRioWord(uint64_t bits) {
  const uint64_t allowed_mask = (1ull << kRioOutputPinCount) - 1;
  return (bits & ~allowed_mask) == 0;
}

static bool IsSingleBit(uint64_t bits) {
  return bits != 0 && (bits & (bits - 1)) == 0;
}

static bool ValidatePinBits(const char *label, uint64_t bits) {
  if (bits == 0) return true;
  if (!FitsInRioWord(bits)) {
    fprintf(stderr, "Pi 5-family RP1 RIO backend: %s uses GPIO bits outside 0..27.\n",
            label);
    return false;
  }
  if (!IsSingleBit(bits)) {
    fprintf(stderr,
            "Pi 5-family RP1 RIO backend: %s must map to exactly one GPIO pin.\n",
            label);
    return false;
  }
  return true;
}

static bool ValidateMappingSignals(const HardwareMapping &h, int double_rows,
                                   int parallel, int row_address_type) {
  // RIO writes exact GPIO bitmasks directly. Unlike the legacy bcm GPIO path,
  // there is no notion of "either of these pins is acceptable", so each signal
  // used by this backend must resolve to one concrete RP1 GPIO.
  if (!ValidatePinBits("clock", h.clock)
      || !ValidatePinBits("strobe", h.strobe)
      || !ValidatePinBits("output_enable", h.output_enable)
      || !ValidatePinBits("p0_r1", h.p0_r1)
      || !ValidatePinBits("p0_g1", h.p0_g1)
      || !ValidatePinBits("p0_b1", h.p0_b1)
      || !ValidatePinBits("p0_r2", h.p0_r2)
      || !ValidatePinBits("p0_g2", h.p0_g2)
      || !ValidatePinBits("p0_b2", h.p0_b2)) {
    return false;
  }
  if (parallel >= 2
      && (!ValidatePinBits("p1_r1", h.p1_r1)
          || !ValidatePinBits("p1_g1", h.p1_g1)
          || !ValidatePinBits("p1_b1", h.p1_b1)
          || !ValidatePinBits("p1_r2", h.p1_r2)
          || !ValidatePinBits("p1_g2", h.p1_g2)
          || !ValidatePinBits("p1_b2", h.p1_b2))) {
    return false;
  }
  if (parallel >= 3
      && (!ValidatePinBits("p2_r1", h.p2_r1)
          || !ValidatePinBits("p2_g1", h.p2_g1)
          || !ValidatePinBits("p2_b1", h.p2_b1)
          || !ValidatePinBits("p2_r2", h.p2_r2)
          || !ValidatePinBits("p2_g2", h.p2_g2)
          || !ValidatePinBits("p2_b2", h.p2_b2))) {
    return false;
  }

  if (row_address_type == 0) {
    if (!ValidatePinBits("address A", h.a)) return false;
    if (double_rows > 2 && !ValidatePinBits("address B", h.b)) return false;
    if (double_rows > 4 && !ValidatePinBits("address C", h.c)) return false;
    if (double_rows > 8 && !ValidatePinBits("address D", h.d)) return false;
    if (double_rows > 16 && !ValidatePinBits("address E", h.e)) return false;
  } else if (row_address_type == 1) {
    if (!ValidatePinBits("address A", h.a)
        || !ValidatePinBits("address B", h.b)) {
      return false;
    }
  } else if (row_address_type == 2) {
    if (!ValidatePinBits("address A", h.a)
        || !ValidatePinBits("address B", h.b)
        || !ValidatePinBits("address C", h.c)
        || !ValidatePinBits("address D", h.d)) {
      return false;
    }
  } else if (row_address_type == 3) {
    if (!ValidatePinBits("address A", h.a)
        || !ValidatePinBits("address C", h.c)) {
      return false;
    }
  } else if (row_address_type == 4) {
    if (!ValidatePinBits("address A", h.a)
        || !ValidatePinBits("address B", h.b)
        || !ValidatePinBits("address C", h.c)) {
      return false;
    }
    if (double_rows > 8 && !ValidatePinBits("address D", h.d)) return false;
    if (double_rows > 16 && !ValidatePinBits("address E", h.e)) return false;
  } else if (row_address_type == 5) {
    if (!ValidatePinBits("address A", h.a)
        || !ValidatePinBits("address B", h.b)
        || !ValidatePinBits("address C", h.c)) {
      return false;
    }
  }
  return true;
}

static bool Rp1UsesShiftRegisterRowAddress(int row_address_type) {
  return row_address_type == 1 || row_address_type == 3
      || row_address_type == 4 || row_address_type == 5;
}

static bool Rp1ShiftsRowAddressEveryBitplane(int row_address_type) {
  // The legacy ABC row-address setter does not cache last_row_; keep that
  // timing for row-address type 3 while the other shift-register types update
  // once per display row.
  return row_address_type == 3;
}

static uint32_t Rp1ShiftRegisterIdleBits(const HardwareMapping &h, int row) {
  return h.a | (row == 0 ? 0 : h.b);
}

static uint32_t Rp1AbcShiftRegisterIdleBits(const HardwareMapping &h, int row) {
  return row == 0 ? h.c : 0;
}

static uint32_t Rp1Sm5266DirectAddressBits(const HardwareMapping &h, int row) {
  uint32_t bits = 0;
  if (row & 0x08) bits |= h.d;
  if (row & 0x10) bits |= h.e;
  return bits;
}

static uint32_t Rp1B707ShiftRegisterIdleBits(const HardwareMapping &h,
                                             int row) {
  return row == 0 ? h.c : 0;
}

static uint32_t CalcRowAddressBits(const HardwareMapping &h,
                                   int row_address_type, int row) {
  switch (row_address_type) {
  case 0: {
    uint32_t bits = 0;
    if (row & 0x01) bits |= h.a;
    if (row & 0x02) bits |= h.b;
    if (row & 0x04) bits |= h.c;
    if (row & 0x08) bits |= h.d;
    if (row & 0x10) bits |= h.e;
    return bits;
  }
  case 1:
    return Rp1ShiftRegisterIdleBits(h, row);
  case 2:
    switch (row & 0x03) {
    case 0: return h.b | h.c | h.d;
    case 1: return h.a | h.c | h.d;
    case 2: return h.a | h.b | h.d;
    default: return h.a | h.b | h.c;
    }
  case 3:
    return Rp1AbcShiftRegisterIdleBits(h, row);
  case 4:
    return Rp1Sm5266DirectAddressBits(h, row);
  case 5:
    return Rp1B707ShiftRegisterIdleBits(h, row);
  default:
    return 0;
  }
}

static int DisplayRowFromLoop(int row_loop, int double_rows, int scan_mode) {
  if (scan_mode != 1) return row_loop;

  const int half_double = double_rows / 2;
  return (row_loop < half_double)
             ? (row_loop << 1)
             : (((row_loop - half_double) << 1) + 1);
}

static void PrepareBitplaneTimings(int pwm_lsb_nanoseconds, int dither_bits,
                                   std::vector<int> *timings_out) {
  timings_out->clear();
  int timing_ns = pwm_lsb_nanoseconds;
  for (int b = 0; b < Framebuffer::kBitPlanes; ++b) {
    timings_out->push_back(timing_ns);
    if (b >= dither_bits) timing_ns *= 2;
  }
}

static void PrepareBitplaneActiveWords(int pwm_lsb_nanoseconds, int dither_bits,
                                       std::vector<int> *active_words_out) {
  std::vector<int> timings_ns;
  PrepareBitplaneTimings(pwm_lsb_nanoseconds, dither_bits, &timings_ns);

  const double word_ns = 1e9 / TargetPixelClockHz();
  active_words_out->clear();
  active_words_out->reserve(timings_ns.size());
  for (size_t i = 0; i < timings_ns.size(); ++i) {
    const int word_count = std::max(
        1, static_cast<int>(ceil(static_cast<double>(timings_ns[i]) / word_ns)));
    active_words_out->push_back(word_count);
  }
  s_rio_state.word_time_nanos = word_ns;
}

static volatile uint32_t *TryMmapGpio(const RioMapCandidate &candidate) {
  const int fd = open(candidate.path, O_RDWR | O_SYNC);
  if (fd < 0) return NULL;

  void *map = mmap(NULL, kMapSizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   candidate.offset);
  close(fd);
  if (map == MAP_FAILED) return NULL;
  return static_cast<volatile uint32_t *>(map);
}

static volatile uint32_t *MapGpioOrDie() {
  static const RioMapCandidate kCandidates[] = {
      // Pi 5 user-space notes and the bitslip reference both use gpiomem0 for
      // the RP1 GPIO/RIO/PADS aperture. gpiomem4 appears suitable for generic
      // line access, but not for this contiguous MMIO window.
      {"/dev/gpiomem0", 0},
      {"/dev/gpiomem0", kPi5PeripheralBase},
      {"/dev/mem", kPi5PeripheralBase},
  };

  for (size_t i = 0; i < sizeof(kCandidates) / sizeof(kCandidates[0]); ++i) {
    volatile uint32_t *map = TryMmapGpio(kCandidates[i]);
    if (map != NULL) return map;
  }

  fprintf(stderr,
          "Pi 5-family RP1 RIO backend: could not map RP1 GPIO registers via "
          "/dev/gpiomem0 or /dev/mem.\n");
  abort();
}

static void ConfigureUsedPins() {
  Rp1RioState &state = s_rio_state;

  for (uint32_t pin = 0; pin < kRioOutputPinCount; ++pin) {
    const uint32_t bit = (1u << pin);
    if ((state.used_mask & bit) == 0) continue;

    state.gpio_regs[pin].ctrl = kGpioFunctionSysRio;
    // Clock and latch transitions are the sharpest edges in the interface, so
    // keep them on a gentler pad drive than the bulk color/address outputs.
    state.pad_regs[pin] =
        (bit == state.clock_bit || bit == state.latch_bit) ? kPadSlowDrive
                                                           : kPadFastDrive;
    state.rio_set->OE = bit;
  }

  IoStoreBarrier();
  state.rio_out->Out = state.output_enable_bit;
}

static void WriteClockedWord(uint32_t pins) {
  Rp1RioState &state = s_rio_state;
  // One framebuffer word corresponds to one panel clock period: present the
  // GPIO data first, then raise the clock bit, then return to idle-low clock.
  state.rio_out->Out = pins;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = pins | state.clock_bit;
  ClockSetupDelay(state.gpio_slowdown);
}

static void SendRawWordsBlanked(const std::vector<uint32_t> &pins) {
  if (pins.empty()) return;

  for (size_t i = 0; i < pins.size(); ++i) {
    WriteClockedWord(pins[i] | s_rio_state.output_enable_bit);
  }
  s_rio_state.rio_out->Out = s_rio_state.output_enable_bit;
}

static void Rp1RioShiftRegisterRowAddress(const HardwareMapping &h,
                                          int double_rows, int row) {
  Rp1RioState &state = s_rio_state;
  const uint32_t blank = state.output_enable_bit;
  const uint32_t clock = h.a;
  const uint32_t data = h.b;
  uint32_t row_bits = 0;

  for (int activate = 0; activate < double_rows; ++activate) {
    state.rio_out->Out = blank;
    ClockSetupDelay(state.gpio_slowdown);

    row_bits = (activate == double_rows - 1 - row) ? 0 : data;
    state.rio_out->Out = blank | row_bits;
    ClockSetupDelay(state.gpio_slowdown);

    state.rio_out->Out = blank | row_bits | clock;
    ClockSetupDelay(state.gpio_slowdown);
  }

  state.rio_out->Out = blank | row_bits;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = blank | row_bits | clock;
  ClockSetupDelay(state.gpio_slowdown);
}

static void Rp1RioShiftAbcRowAddress(const HardwareMapping &h, int double_rows,
                                     int row) {
  Rp1RioState &state = s_rio_state;
  const uint32_t blank = state.output_enable_bit;
  const uint32_t clock = h.a;
  const uint32_t data = h.c;
  uint32_t row_bits = 0;

  for (int activate = 0; activate < double_rows; ++activate) {
    state.rio_out->Out = blank;
    ClockSetupDelay(state.gpio_slowdown);

    row_bits = (activate == double_rows - 1 - row) ? data : 0;
    state.rio_out->Out = blank | row_bits;
    ClockSetupDelay(state.gpio_slowdown);

    state.rio_out->Out = blank | row_bits | clock;
    ClockSetupDelay(state.gpio_slowdown);
  }

  state.rio_out->Out = blank | row_bits;
  ClockSetupDelay(state.gpio_slowdown);
}

static void Rp1RioShiftSm5266RowAddress(const HardwareMapping &h, int row) {
  Rp1RioState &state = s_rio_state;
  const uint32_t blank = state.output_enable_bit;
  const uint32_t clock = h.a;
  const uint32_t data = h.b;
  const uint32_t enable = h.c;

  state.rio_out->Out = blank | enable;
  ClockSetupDelay(state.gpio_slowdown);
  for (int r = 7; r >= 0; --r) {
    const uint32_t row_bits = ((row % 8) == r) ? data : 0;
    state.rio_out->Out = blank | enable | row_bits;
    ClockSetupDelay(state.gpio_slowdown);
    state.rio_out->Out = blank | enable | row_bits | clock;
    ClockSetupDelay(state.gpio_slowdown);
    state.rio_out->Out = blank | enable | row_bits | clock;
    ClockSetupDelay(state.gpio_slowdown);
    state.rio_out->Out = blank | enable | row_bits;
    ClockSetupDelay(state.gpio_slowdown);
  }

  state.rio_out->Out = blank | Rp1Sm5266DirectAddressBits(h, row);
  ClockSetupDelay(state.gpio_slowdown);
}

static void Rp1RioShiftB707RowAddress(const HardwareMapping &h, int row) {
  Rp1RioState &state = s_rio_state;
  const uint32_t blank = state.output_enable_bit;
  const uint32_t clock = h.a;
  const uint32_t enable = h.b;
  const uint32_t row_bits = Rp1B707ShiftRegisterIdleBits(h, row);

  state.rio_out->Out = blank | enable;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = blank | enable | row_bits;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = blank | enable | row_bits | clock;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = blank | enable | row_bits | clock;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_out->Out = blank | row_bits;
  ClockSetupDelay(state.gpio_slowdown);
}

static void Rp1RioShiftRowAddress(const HardwareMapping &h, int double_rows,
                                  int row_address_type, int row) {
  switch (row_address_type) {
  case 1:
    Rp1RioShiftRegisterRowAddress(h, double_rows, row);
    break;
  case 3:
    Rp1RioShiftAbcRowAddress(h, double_rows, row);
    break;
  case 4:
    Rp1RioShiftSm5266RowAddress(h, row);
    break;
  case 5:
    Rp1RioShiftB707RowAddress(h, row);
    break;
  default:
    break;
  }
}

static void SendFM6126Init(const HardwareMapping &h, int columns) {
  const uint32_t bits_on = CollectColorMask(h, s_rio_state.parallel)
                           | static_cast<uint32_t>(h.a);
  const uint32_t bits_off = h.a;
  static const char *init_b12 = "0111111111111111";
  static const char *init_b13 = "0000000001000000";

  std::vector<uint32_t> pins;
  pins.reserve(columns * 2);
  for (int i = 0; i < columns; ++i) {
    uint32_t value = (init_b12[i % 16] == '0') ? bits_off : bits_on;
    if (i > columns - 12) value |= h.strobe;
    pins.push_back(value);
  }
  for (int i = 0; i < columns; ++i) {
    uint32_t value = (init_b13[i % 16] == '0') ? bits_off : bits_on;
    if (i > columns - 13) value |= h.strobe;
    pins.push_back(value);
  }
  SendRawWordsBlanked(pins);
}

static void SendFM6127Init(const HardwareMapping &h, int columns) {
  const uint32_t bits_r_on = h.p0_r1 | h.p0_r2;
  const uint32_t bits_g_on = h.p0_g1 | h.p0_g2;
  const uint32_t bits_b_on = h.p0_b1 | h.p0_b2;
  const uint32_t bits_on = bits_r_on | bits_g_on | bits_b_on;
  static const char *init_b12 = "1111111111001110";
  static const char *init_b13 = "1110000001100010";
  static const char *init_b11 = "0101111100000000";

  std::vector<uint32_t> pins;
  pins.reserve(columns * 3);
  for (int i = 0; i < columns; ++i) {
    uint32_t value = (init_b12[i % 16] == '0') ? 0 : bits_on;
    if (i > columns - 12) value |= h.strobe;
    pins.push_back(value);
  }
  for (int i = 0; i < columns; ++i) {
    uint32_t value = (init_b13[i % 16] == '0') ? 0 : bits_on;
    if (i > columns - 13) value |= h.strobe;
    pins.push_back(value);
  }
  for (int i = 0; i < columns; ++i) {
    uint32_t value = (init_b11[i % 16] == '0') ? 0 : bits_on;
    if (i > columns - 11) value |= h.strobe;
    pins.push_back(value);
  }
  SendRawWordsBlanked(pins);
}

}  // namespace

bool Rp1RioPlatformDetected() {
  if (!GPIO::IsPi5Family()) {
    std::string model;
    if (!ReadSmallTextFile("/proc/device-tree/model", &model)) {
      return false;
    }
    if (model.find("Raspberry Pi 5") == std::string::npos
        && model.find("Compute Module 5") == std::string::npos) {
      return false;
    }
  }
  return FileExists("/dev/gpiomem0") || FileExists("/dev/mem");
}

void Rp1RioSetEnabled(bool enabled) { s_rio_requested = enabled; }

bool Rp1RioBackendRequested() { return s_rio_requested; }

bool Rp1RioShouldActivate(const char *hardware_mapping, int row_address_type,
                          int parallel) {
  if (!s_rio_requested) return false;
  if (!Rp1RioPlatformDetected()) return false;
  if (!MappingNameSupported(hardware_mapping)) return false;
  if (!(row_address_type >= 0 && row_address_type <= 5)) return false;
  return parallel >= 1 && parallel <= 3;
}

void Rp1RioSetGpioSlowdown(int slowdown) {
  s_rio_state.gpio_slowdown = slowdown <= 1 ? 1 : slowdown;
}

bool Rp1RioIsActive() { return s_rio_state.active; }

void Rp1RioInitOrDie(const HardwareMapping &mapping, int double_rows,
                     int parallel, int pwm_lsb_nanoseconds, int dither_bits,
                     int row_address_type) {
  Rp1RioState &state = s_rio_state;
  if (state.active) return;

  state.row_address_type = row_address_type;
  state.double_rows = double_rows;
  state.parallel = parallel;
  state.output_enable_bit = mapping.output_enable;
  state.clock_bit = mapping.clock;
  state.latch_bit = mapping.strobe;
  state.used_mask = BuildUsedMask(mapping, double_rows, parallel,
                                  row_address_type);

  if (!FitsInRioWord(state.used_mask)) {
    fprintf(stderr,
            "Pi 5-family RP1 RIO backend: the selected GPIO mapping uses pins "
            "outside the RP1 0..27 GPIO range.\n");
    abort();
  }
  if (!ValidateMappingSignals(mapping, double_rows, parallel, row_address_type)) {
    abort();
  }

  PrepareBitplaneActiveWords(pwm_lsb_nanoseconds, dither_bits,
                             &state.bitplane_active_words);

  state.map_base = MapGpioOrDie();
  state.gpio_regs =
      reinterpret_cast<GpioCtrlRegs *>(const_cast<uint32_t *>(
          state.map_base + kGpioOffsetWords));
  state.pad_regs = const_cast<uint32_t *>(state.map_base + kPadOffsetWords + 1);
  state.rio_out = reinterpret_cast<RioRegs *>(
      const_cast<uint32_t *>(state.map_base + kRioOffsetWords));
  state.rio_set = reinterpret_cast<RioRegs *>(
      const_cast<uint32_t *>(state.map_base + kRioOffsetWords + (0x2000 / 4)));
  state.rio_clr = reinterpret_cast<RioRegs *>(
      const_cast<uint32_t *>(state.map_base + kRioOffsetWords + (0x3000 / 4)));

  ConfigureUsedPins();
  state.active = true;
}

void Rp1RioInitializePanels(const HardwareMapping &mapping,
                            const char *panel_type, int columns) {
  if (!s_rio_state.active || panel_type == NULL || *panel_type == '\0') return;

  if (strncasecmp(panel_type, "fm6126", 6) == 0) {
    SendFM6126Init(mapping, columns);
  } else if (strncasecmp(panel_type, "fm6127", 6) == 0) {
    SendFM6127Init(mapping, columns);
  } else if (!s_rio_state.panel_init_warned) {
    fprintf(stderr,
            "Pi 5-family RP1 RIO backend: panel init '%s' is not implemented; "
            "continuing without panel init.\n",
            panel_type);
    s_rio_state.panel_init_warned = true;
  }
}

void Rp1RioDumpFramebuffer(Framebuffer *framebuffer, int pwm_low_bit) {
  if (!s_rio_state.active || framebuffer == NULL) return;

  Rp1RioState &state = s_rio_state;
  // As in the PIO path, the framebuffer already holds the per-column color
  // bits. The RIO dump loop layers row addresses and panel control timing on
  // top of those words and uses busy-waits to hold the PWM bitplane on-time.
  const HardwareMapping &h = framebuffer->hardware_mapping();
  const int start_bit =
      std::max(pwm_low_bit, Framebuffer::kBitPlanes - framebuffer->pwmbits());
  const int double_rows = framebuffer->double_rows();
  const int columns = framebuffer->columns();
  const int scan_mode = framebuffer->scan_mode();
  const bool uses_shift_register =
      Rp1UsesShiftRegisterRowAddress(state.row_address_type);
  const bool shifts_every_bitplane =
      Rp1ShiftsRowAddressEveryBitplane(state.row_address_type);

  uint32_t previous_addr = state.has_pending_active_words
                               ? state.pending_addr
                               : CalcRowAddressBits(
                                     h, state.row_address_type,
                                     DisplayRowFromLoop(double_rows - 1,
                                                        double_rows,
                                                        scan_mode));
  int previous_active_words = state.has_pending_active_words
                                  ? state.pending_active_words
                                  : 0;
  int last_shift_register_row = -1;

  for (int row_loop = 0; row_loop < double_rows; ++row_loop) {
    const int display_row =
        DisplayRowFromLoop(row_loop, double_rows, scan_mode);
    const uint32_t current_addr =
        CalcRowAddressBits(h, state.row_address_type, display_row);

    for (int bit = start_bit; bit < Framebuffer::kBitPlanes; ++bit) {
      const gpio_bits_t *row_data = framebuffer->RowDataAt(display_row, bit);

      int remaining_overlap_words = previous_active_words;
      for (int col = 0; col < columns; ++col) {
        uint32_t pins = static_cast<uint32_t>(row_data[col]) | previous_addr;
        if (remaining_overlap_words <= 0) {
          pins |= state.output_enable_bit;
        }
        WriteClockedWord(pins);
        if (remaining_overlap_words > 0) --remaining_overlap_words;
      }

      if (remaining_overlap_words > 0) {
        state.rio_out->Out = previous_addr;
        BusyWaitWords(remaining_overlap_words);
      }

      if (uses_shift_register) {
        if (shifts_every_bitplane || display_row != last_shift_register_row) {
          Rp1RioShiftRowAddress(h, double_rows, state.row_address_type,
                                display_row);
          last_shift_register_row = display_row;
        }
      }

      state.rio_out->Out = current_addr | state.output_enable_bit;
      ClockSetupDelay(state.gpio_slowdown);
      state.rio_out->Out =
          current_addr | state.output_enable_bit | state.latch_bit;
      ClockSetupDelay(state.gpio_slowdown);
      state.rio_out->Out = current_addr | state.output_enable_bit;
      ClockSetupDelay(state.gpio_slowdown);

      previous_addr = current_addr;
      previous_active_words = state.bitplane_active_words[bit];
    }
  }

  // Keep the last row's final bitplane pending for the next refresh. This
  // preserves the overlap pipeline across the frame boundary instead of
  // displaying that row with a one-off end-of-frame dwell.
  state.pending_addr = previous_addr;
  state.pending_active_words = previous_active_words;
  state.has_pending_active_words = true;
  state.rio_out->Out = previous_addr | state.output_enable_bit;
}

void Rp1RioDeinit() {
  Rp1RioState &state = s_rio_state;
  if (!state.active) return;

  state.rio_out->Out = state.output_enable_bit;
  ClockSetupDelay(state.gpio_slowdown);
  state.rio_clr->OE = state.used_mask;
  IoStoreBarrier();

  if (state.map_base != NULL) {
    munmap(const_cast<uint32_t *>(state.map_base), kMapSizeBytes);
  }

  state.active = false;
  state.panel_init_warned = false;
  state.map_base = NULL;
  state.gpio_regs = NULL;
  state.pad_regs = NULL;
  state.rio_out = NULL;
  state.rio_set = NULL;
  state.rio_clr = NULL;
  state.row_address_type = 0;
  state.double_rows = 0;
  state.parallel = 0;
  state.gpio_slowdown = 1;
  state.output_enable_bit = 0;
  state.clock_bit = 0;
  state.latch_bit = 0;
  state.used_mask = 0;
  state.word_time_nanos = 0.0;
  state.has_pending_active_words = false;
  state.pending_addr = 0;
  state.pending_active_words = 0;
  state.bitplane_active_words.clear();
}

}  // namespace internal
}  // namespace rgb_matrix
