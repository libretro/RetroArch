// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#include "rp1_pio_backend.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../framebuffer-internal.h"
#include "../gpio.h"
#include "../hardware-mapping.h"

extern "C" {
#include "hardware/pio.h"
#include "piolib.h"
}

#include "piomatter/protomatter.pio.h"

namespace rgb_matrix {
namespace internal {
namespace {

// Pi 5 PIO path:
// - framebuffer.cc still decides row order and prepares one GPIO bitmap per
//   pixel clock in the software framebuffer.
// - this backend repackages those GPIO bitmaps into a compact command stream
//   for a single RP1 PIO state machine.
// - the PIO program owns the clock edge timing via side-set; the CPU only
//   describes which GPIO levels to present and how long each bitplane stays on.

static const uint32_t kPioOutputPinCount = 28;
static const uint32_t kMaxTransferBytes = 2 * 1024 * 1024;
// Command words use bit 31 as a tag:
//   1 = "the next N words are GPIO samples to clock out"
//   0 = "hold this GPIO sample for N encoded delay cycles"
static const uint32_t kCommandData = 1u << 31;
static const int kDelayOverheadClocks = 5;
static const int kClocksPerDataWord = 2;
static const int kPostAddressDelayClocks = 5;
static const double kBaseTargetPioClockHz = 27000000.0;

struct Rp1PioState {
  Rp1PioState()
      : active(false),
        panel_init_warned(false),
        pio(NULL),
        sm(-1),
        row_address_type(0),
        double_rows(0),
        gpio_slowdown(1),
        output_enable_bit(0),
        latch_bit(0),
        used_mask(0) {
  }

  bool active;
  bool panel_init_warned;
  PIO pio;
  int sm;
  // Cached configuration copied from the selected matrix mapping so the hot
  // dump loop does not need to rediscover backend-specific details each frame.
  int row_address_type;
  int double_rows;
  int gpio_slowdown;
  uint32_t output_enable_bit;
  uint32_t latch_bit;
  uint32_t used_mask;
  std::vector<int> bitplane_active_words;
  std::vector<uint32_t> transfer_buffer;
};

Rp1PioState s_pio_state;

static double TargetPioClockHz() {
  // Chained panels have tighter setup/hold margin; reuse the public slowdown
  // flag as a Pi 5 PIO pixel-clock divisor.
  const int divisor = std::max(1, s_pio_state.gpio_slowdown);
  return kBaseTargetPioClockHz / divisor;
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
  // Keep this intentionally narrower than the generic library mapping list.
  // The PIO backend assumes one clock side-set pin and RP1 GPIOs 0..27 only.
  return strcasecmp(hardware_mapping, "regular") == 0
      || strcasecmp(hardware_mapping, "regular-pi1") == 0
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
  if (parallel >= 4) {
    mask |= h.p3_r1 | h.p3_g1 | h.p3_b1 | h.p3_r2 | h.p3_g2 | h.p3_b2;
  }
  if (parallel >= 5) {
    mask |= h.p4_r1 | h.p4_g1 | h.p4_b1 | h.p4_r2 | h.p4_g2 | h.p4_b2;
  }
  if (parallel >= 6) {
    mask |= h.p5_r1 | h.p5_g1 | h.p5_b1 | h.p5_r2 | h.p5_g2 | h.p5_b2;
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

static bool FitsInPioWord(uint64_t bits) {
  const uint64_t kAllowedMask = (1ull << kPioOutputPinCount) - 1;
  return (bits & ~kAllowedMask) == 0;
}

static bool IsSingleBit(uint64_t bits) {
  return bits != 0 && (bits & (bits - 1)) == 0;
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

static void AppendDelay(std::vector<uint32_t> *buffer, uint32_t pins,
                        int clock_cycles) {
  int encoded_cycles = clock_cycles - kDelayOverheadClocks;
  if (encoded_cycles < 1) encoded_cycles = 1;
  buffer->push_back(static_cast<uint32_t>(encoded_cycles - 1));
  buffer->push_back(pins);
}

static void AppendDataHeader(std::vector<uint32_t> *buffer, int words) {
  buffer->push_back(kCommandData | static_cast<uint32_t>(words - 1));
}

static void Rp1PioAppendShiftRegisterRowAddress(
    std::vector<uint32_t> *buffer, const HardwareMapping &h, int double_rows,
    int row, uint32_t blank) {
  const uint32_t clock = h.a;
  const uint32_t data = h.b;
  uint32_t row_bits = 0;

  for (int activate = 0; activate < double_rows; ++activate) {
    AppendDelay(buffer, blank, 0);

    row_bits = (activate == double_rows - 1 - row) ? 0 : data;
    AppendDelay(buffer, blank | row_bits, 0);
    AppendDelay(buffer, blank | row_bits | clock, 0);
  }

  AppendDelay(buffer, blank | row_bits, 0);
  AppendDelay(buffer, blank | row_bits | clock, 0);
}

static void Rp1PioAppendAbcShiftRegisterRowAddress(
    std::vector<uint32_t> *buffer, const HardwareMapping &h, int double_rows,
    int row, uint32_t blank) {
  const uint32_t clock = h.a;
  const uint32_t data = h.c;
  uint32_t row_bits = 0;

  for (int activate = 0; activate < double_rows; ++activate) {
    AppendDelay(buffer, blank, 0);

    row_bits = (activate == double_rows - 1 - row) ? data : 0;
    AppendDelay(buffer, blank | row_bits, 0);
    AppendDelay(buffer, blank | row_bits | clock, 0);
  }

  AppendDelay(buffer, blank | row_bits, 0);
}

static void Rp1PioAppendSm5266RowAddress(
    std::vector<uint32_t> *buffer, const HardwareMapping &h, int row,
    uint32_t blank) {
  const uint32_t clock = h.a;
  const uint32_t data = h.b;
  const uint32_t enable = h.c;

  AppendDelay(buffer, blank | enable, 0);
  for (int r = 7; r >= 0; --r) {
    const uint32_t row_bits = ((row % 8) == r) ? data : 0;
    AppendDelay(buffer, blank | enable | row_bits, 0);
    AppendDelay(buffer, blank | enable | row_bits | clock, 0);
    AppendDelay(buffer, blank | enable | row_bits | clock, 0);
    AppendDelay(buffer, blank | enable | row_bits, 0);
  }

  AppendDelay(buffer, blank | Rp1Sm5266DirectAddressBits(h, row),
              kPostAddressDelayClocks);
}

static void Rp1PioAppendB707ShiftRegisterRowAddress(
    std::vector<uint32_t> *buffer, const HardwareMapping &h, int row,
    uint32_t blank) {
  const uint32_t clock = h.a;
  const uint32_t enable = h.b;
  const uint32_t row_bits = Rp1B707ShiftRegisterIdleBits(h, row);

  AppendDelay(buffer, blank | enable, 0);
  AppendDelay(buffer, blank | enable | row_bits, 0);
  AppendDelay(buffer, blank | enable | row_bits | clock, 0);
  AppendDelay(buffer, blank | enable | row_bits | clock, 0);
  AppendDelay(buffer, blank | row_bits, 0);
}

static void Rp1PioAppendRowAddress(
    std::vector<uint32_t> *buffer, const HardwareMapping &h, int double_rows,
    int row_address_type, int row, uint32_t blank) {
  switch (row_address_type) {
  case 1:
    Rp1PioAppendShiftRegisterRowAddress(buffer, h, double_rows, row, blank);
    break;
  case 3:
    Rp1PioAppendAbcShiftRegisterRowAddress(buffer, h, double_rows, row, blank);
    break;
  case 4:
    Rp1PioAppendSm5266RowAddress(buffer, h, row, blank);
    break;
  case 5:
    Rp1PioAppendB707ShiftRegisterRowAddress(buffer, h, row, blank);
    break;
  default:
    break;
  }
}

static int TransferLarge(PIO pio, int sm, const uint32_t *data,
                         size_t data_bytes) {
  // Keep normal frames in one transfer. Very large frame updates are still
  // streamed in chunks.
  while (data_bytes > 0) {
    const size_t chunk = std::min(data_bytes,
                                  static_cast<size_t>(kMaxTransferBytes));
    int rc = pio_sm_xfer_data(pio, sm, PIO_DIR_TO_SM, chunk,
                              const_cast<uint32_t *>(data));
    if (rc != 0) return rc;
    data += chunk / sizeof(*data);
    data_bytes -= chunk;
  }
  return 0;
}

static void InitPinDirection(PIO pio, int sm, uint32_t used_mask) {
  for (uint32_t pin = 0; pin < kPioOutputPinCount; ++pin) {
    if ((used_mask & (1u << pin)) == 0) continue;
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
  }
}

static void ReleasePinDirection(PIO pio, int sm, uint32_t used_mask) {
  for (uint32_t pin = 0; pin < kPioOutputPinCount; ++pin) {
    if ((used_mask & (1u << pin)) == 0) continue;
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
  }
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

  const double data_word_ns = (1e9 * kClocksPerDataWord) / TargetPioClockHz();
  active_words_out->clear();
  active_words_out->reserve(timings_ns.size());
  for (size_t i = 0; i < timings_ns.size(); ++i) {
    const int word_count = std::max(
        1, static_cast<int>(ceil(static_cast<double>(timings_ns[i]) /
                                 data_word_ns)));
    active_words_out->push_back(word_count);
  }
}

static void ConfigureStateMachineOrDie(const HardwareMapping &mapping) {
  Rp1PioState &state = s_pio_state;
  // One state machine drives the whole display. All GPIO samples are expanded
  // on the CPU side; the PIO program's job is to issue clock edges and honor
  // encoded delay commands at a stable cadence.
  state.pio = pio0;
  if (PIO_IS_ERR(state.pio)) {
    fprintf(stderr, "Opening /dev/pio0 failed with error %d\n",
            PIO_ERR_VAL(state.pio));
    abort();
  }

  state.sm = pio_claim_unused_sm(state.pio, true);
  if (state.sm < 0) {
    fprintf(stderr, "Could not claim an unused RP1 PIO state machine.\n");
    abort();
  }

  const int xfer_rc = pio_sm_config_xfer(state.pio, state.sm, PIO_DIR_TO_SM,
                                         kMaxTransferBytes, 3);
  if (xfer_rc != 0) {
    fprintf(stderr, "RP1 PIO DMA configuration failed: %d\n", xfer_rc);
    abort();
  }

  static const struct pio_program kProgram = {
      protomatter,
      32,
      -1,
      0,
  };
  const uint offset = pio_add_program(state.pio, &kProgram);
  if (offset == PIO_ORIGIN_INVALID) {
    fprintf(stderr, "Loading the HUB75 RP1 PIO program failed.\n");
    abort();
  }

  pio_sm_clear_fifos(state.pio, state.sm);
  pio_sm_set_clkdiv(state.pio, state.sm, 1.0f);

  pio_sm_config config = pio_get_default_sm_config();
  sm_config_set_wrap(&config, offset + protomatter_wrap_target,
                     offset + protomatter_wrap);
  // The config API expects the full encoded side-set field width. For
  // ".side_set 1 opt", that means 1 data bit plus the optional-enable bit.
  const uint encoded_sideset_bits =
      protomatter_sideset_pin_count + (protomatter_sideset_enable ? 1u : 0u);
  sm_config_set_sideset(&config, encoded_sideset_bits,
                        protomatter_sideset_enable, false);
  sm_config_set_out_shift(&config, false, true, 32);
  sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
  sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / TargetPioClockHz());
  sm_config_set_out_pins(&config, 0, kPioOutputPinCount);

  const unsigned clock_pin = __builtin_ctz(mapping.clock);
  sm_config_set_sideset_pins(&config, clock_pin);

  pio_sm_init(state.pio, state.sm, offset, &config);
  InitPinDirection(state.pio, state.sm, state.used_mask);
  pio_sm_set_enabled(state.pio, state.sm, true);
  pio_sm_set_pins_with_mask(state.pio, state.sm, state.output_enable_bit,
                            state.used_mask);
}

static void SendRawWordsOrDie(const std::vector<uint32_t> &pins) {
  if (pins.empty()) return;

  Rp1PioState &state = s_pio_state;
  // Panel init sequences reuse the same command format as the refresh loop,
  // but always run with OE blanked so the panel does not flash partial data.
  std::vector<uint32_t> buffer;
  buffer.reserve(pins.size() + 3);
  AppendDataHeader(&buffer, pins.size());
  for (size_t i = 0; i < pins.size(); ++i) {
    buffer.push_back(pins[i] | state.output_enable_bit);
  }
  AppendDelay(&buffer, state.output_enable_bit, 0);

  const int rc = TransferLarge(state.pio, state.sm, &buffer[0],
                               buffer.size() * sizeof(buffer[0]));
  if (rc != 0) {
    fprintf(stderr, "RP1 PIO transfer failed during panel init: %d\n", rc);
    abort();
  }
}

static void SendFM6126Init(const HardwareMapping &h, int columns) {
  const uint32_t bits_on =
      CollectColorMask(h, 6) | static_cast<uint32_t>(h.a);
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
  SendRawWordsOrDie(pins);
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
  SendRawWordsOrDie(pins);
}

}  // namespace

bool Rp1PioPlatformDetected() {
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
  return FileExists("/dev/pio0");
}

bool Rp1PioConfigSupported(const char *hardware_mapping, int row_address_type,
                           int parallel) {
  if (!Rp1PioPlatformDetected()) return false;
  if (!MappingNameSupported(hardware_mapping)) return false;
  if (!(row_address_type >= 0 && row_address_type <= 5)) return false;
  return parallel >= 1 && parallel <= 3;
}

bool Rp1PioShouldActivate(const char *hardware_mapping, int row_address_type,
                          int parallel) {
  return Rp1PioConfigSupported(hardware_mapping, row_address_type, parallel);
}

void Rp1PioSetGpioSlowdown(int slowdown) {
  s_pio_state.gpio_slowdown = slowdown <= 1 ? 1 : slowdown;
}

bool Rp1PioIsActive() { return s_pio_state.active; }

void Rp1PioInitOrDie(const HardwareMapping &mapping, int double_rows, int parallel,
                     int pwm_lsb_nanoseconds, int dither_bits,
                     int row_address_type) {
  Rp1PioState &state = s_pio_state;
  if (state.active) return;

  state.row_address_type = row_address_type;
  state.double_rows = double_rows;
  state.output_enable_bit = mapping.output_enable;
  state.latch_bit = mapping.strobe;
  state.used_mask = BuildUsedMask(mapping, state.double_rows, parallel,
                                  row_address_type);

  if (!FitsInPioWord(state.used_mask)) {
    fprintf(stderr,
            "The selected GPIO mapping uses pins outside RP1 PIO's 0..27 "
            "range.\n");
    abort();
  }
  if (!FitsInPioWord(mapping.clock) || !FitsInPioWord(mapping.strobe)
      || !FitsInPioWord(mapping.output_enable)
      || !FitsInPioWord(CollectAddressMask(mapping, state.double_rows,
                                           row_address_type))
      || !FitsInPioWord(CollectColorMask(mapping, parallel))) {
    fprintf(stderr, "The Pi 5-family RP1 backend only supports GPIO bits 0..27.\n");
    abort();
  }
  if (!IsSingleBit(mapping.clock)) {
    fprintf(stderr,
            "The Pi 5-family RP1 PIO backend requires the clock signal to map to "
            "exactly one GPIO pin.\n");
    abort();
  }

  PrepareBitplaneActiveWords(pwm_lsb_nanoseconds, dither_bits,
                             &state.bitplane_active_words);
  ConfigureStateMachineOrDie(mapping);
  state.active = true;
}

void Rp1PioInitializePanels(const HardwareMapping &mapping,
                            const char *panel_type, int columns) {
  if (!s_pio_state.active || panel_type == NULL || *panel_type == '\0') return;

  if (strncasecmp(panel_type, "fm6126", 6) == 0) {
    SendFM6126Init(mapping, columns);
  } else if (strncasecmp(panel_type, "fm6127", 6) == 0) {
    SendFM6127Init(mapping, columns);
  } else if (!s_pio_state.panel_init_warned) {
    fprintf(stderr,
            "Pi 5-family RP1 backend: panel init '%s' is not implemented; "
            "continuing without panel init.\n",
            panel_type);
    s_pio_state.panel_init_warned = true;
  }
}

void Rp1PioDumpFramebuffer(Framebuffer *framebuffer, int pwm_low_bit) {
  if (!s_pio_state.active || framebuffer == NULL) return;

  Rp1PioState &state = s_pio_state;
  // The framebuffer already contains per-column color GPIO bits. This pass only
  // adds row-address selection, latch/OE sequencing, and the active-time delay
  // that gives each PWM bitplane its brightness weight.
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

  const int plane_count = Framebuffer::kBitPlanes - start_bit;
  state.transfer_buffer.clear();
  state.transfer_buffer.reserve(
      double_rows * plane_count * (columns + 8) + 8);

  uint32_t previous_addr = CalcRowAddressBits(
      h, state.row_address_type,
      DisplayRowFromLoop(double_rows - 1, double_rows, scan_mode));
  int previous_active_words = 0;
  int last_shift_register_row = -1;

  for (int row_loop = 0; row_loop < double_rows; ++row_loop) {
    const int display_row =
        DisplayRowFromLoop(row_loop, double_rows, scan_mode);
    const uint32_t current_addr =
        CalcRowAddressBits(h, state.row_address_type, display_row);

    for (int bit = start_bit; bit < Framebuffer::kBitPlanes; ++bit) {
      const gpio_bits_t *row_data = framebuffer->RowDataAt(display_row, bit);
      AppendDataHeader(&state.transfer_buffer, columns);

      int remaining_overlap_words = previous_active_words;
      for (int col = 0; col < columns; ++col) {
        uint32_t pins = static_cast<uint32_t>(row_data[col]) | previous_addr;
        if (remaining_overlap_words <= 0) {
          pins |= state.output_enable_bit;
        }
        state.transfer_buffer.push_back(pins);
        if (remaining_overlap_words > 0) --remaining_overlap_words;
      }

      if (remaining_overlap_words > 0) {
        AppendDelay(&state.transfer_buffer, previous_addr,
                    remaining_overlap_words * kClocksPerDataWord);
      }

      if (uses_shift_register) {
        if (shifts_every_bitplane || display_row != last_shift_register_row) {
          Rp1PioAppendRowAddress(&state.transfer_buffer, h, double_rows,
                                 state.row_address_type, display_row,
                                 state.output_enable_bit);
          last_shift_register_row = display_row;
        }
      } else if (current_addr != previous_addr) {
        AppendDelay(&state.transfer_buffer,
                    current_addr | state.output_enable_bit,
                    kPostAddressDelayClocks);
      }
      AppendDelay(&state.transfer_buffer,
                  current_addr | state.output_enable_bit | state.latch_bit, 0);

      previous_addr = current_addr;
      previous_active_words = state.bitplane_active_words[bit];
    }
  }

  if (previous_active_words > 0) {
    AppendDelay(&state.transfer_buffer, previous_addr,
                previous_active_words * kClocksPerDataWord);
  }
  AppendDelay(&state.transfer_buffer, previous_addr | state.output_enable_bit,
              0);

  const int rc = TransferLarge(state.pio, state.sm, &state.transfer_buffer[0],
                               state.transfer_buffer.size()
                                   * sizeof(state.transfer_buffer[0]));
  if (rc != 0) {
    fprintf(stderr, "RP1 PIO framebuffer transfer failed: %d\n", rc);
    abort();
  }
}

void Rp1PioDeinit() {
  Rp1PioState &state = s_pio_state;
  if (!state.active) return;

  pio_sm_set_enabled(state.pio, state.sm, false);
  pio_sm_clear_fifos(state.pio, state.sm);
  ReleasePinDirection(state.pio, state.sm, state.used_mask);
  pio_sm_unclaim(state.pio, state.sm);
  pio_close(state.pio);

  state.active = false;
  state.panel_init_warned = false;
  state.pio = NULL;
  state.sm = -1;
  state.row_address_type = 0;
  state.double_rows = 0;
  state.gpio_slowdown = 1;
  state.output_enable_bit = 0;
  state.latch_bit = 0;
  state.used_mask = 0;
  state.bitplane_active_words.clear();
  state.transfer_buffer.clear();
}

}  // namespace internal
}  // namespace rgb_matrix
