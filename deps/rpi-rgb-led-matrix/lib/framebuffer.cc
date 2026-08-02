// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

// The framebuffer is the workhorse: it represents the frame in some internal
// format that is friendly to be dumped to the matrix quickly. Provides methods
// to manipulate the content.

#include "framebuffer-internal.h"

#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "gpio.h"
#include "rp1/rp1_pio_backend.h"
#include "rp1/rp1_rio_backend.h"
#include "../include/graphics.h"

namespace rgb_matrix {
namespace internal {
// We need one global instance of a timing correct pulser. There are different
// implementations depending on the context.
static PinPulser *sOutputEnablePulser = NULL;

#ifdef ONLY_SINGLE_SUB_PANEL
#  define SUB_PANELS_ 1
#else
#  define SUB_PANELS_ 2
#endif

PixelDesignator *PixelDesignatorMap::get(int x, int y) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_)
    return NULL;
  return &buffer_[(y*width_) + x];
}

PixelDesignatorMap::PixelDesignatorMap(int width, int height,
                                       const PixelDesignator &fill_bits)
  : width_(width),
    height_(height),
    fill_bits_(fill_bits),
    buffer_(width * height) {
}

// Different panel types use different techniques to set the row address.
// We abstract that away with different implementations of RowAddressSetter
class RowAddressSetter {
public:
  virtual ~RowAddressSetter() {}
  virtual gpio_bits_t need_bits() const = 0;
  virtual void SetRowAddress(GPIO *io, int row) = 0;
};

namespace {

// The default DirectRowAddressSetter just sets the address in parallel
// output lines ABCDE with A the LSB and E the MSB.
class DirectRowAddressSetter : public RowAddressSetter {
public:
  DirectRowAddressSetter(int double_rows, const HardwareMapping &h)
    : row_mask_(0), last_row_(-1) {
    assert(double_rows <= 32);  // need to resize row_lookup_
    if (double_rows > 16) row_mask_ |= h.e;
    if (double_rows > 8)  row_mask_ |= h.d;
    if (double_rows > 4)  row_mask_ |= h.c;
    if (double_rows > 2)  row_mask_ |= h.b;
    row_mask_ |= h.a;
    for (int i = 0; i < double_rows; ++i) {
      // To avoid the bit-fiddle in the critical path, utilize
      // a lookup-table for all possible rows.
      gpio_bits_t row_address = (i & 0x01) ? h.a : 0;
      row_address |= (i & 0x02) ? h.b : 0;
      row_address |= (i & 0x04) ? h.c : 0;
      row_address |= (i & 0x08) ? h.d : 0;
      row_address |= (i & 0x10) ? h.e : 0;
      row_lookup_[i] = row_address;
    }
  }

  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    if (row == last_row_) return;
    io->WriteMaskedBits(row_lookup_[row], row_mask_);
    last_row_ = row;
  }

private:
  gpio_bits_t row_mask_;
  gpio_bits_t row_lookup_[32];
  int last_row_;
};

// The SM5266RowAddressSetter (ABC Shifter + DE direct) sets bits ABC using
// a 8 bit shifter and DE directly. The panel this works with has 8 SM5266
// shifters (4 for the top 32 rows and 4 for the bottom 32 rows).
// DE is used to select the active shifter
// (rows 1-8/33-40, 9-16/41-48, 17-24/49-56, 25-32/57-64).
// Rows are enabled by shifting in 8 bits (high bit first) with a high bit
// enabling that row. This allows up to 8 rows per group to be active at the
// same time (if they have the same content), but that isn't implemented here.
// BK, DIN and DCK are the designations on the SM5266P datasheet.
// BK = Enable Input, DIN = Serial In, DCK = Clock
class SM5266RowAddressSetter : public RowAddressSetter {
public:
  SM5266RowAddressSetter(int double_rows, const HardwareMapping &h)
    : row_mask_(h.a | h.b | h.c),
      last_row_(-1),
      bk_(h.c),
      din_(h.b),
      dck_(h.a) {
    assert(double_rows <= 32); // designed for up to 1/32 panel
    if (double_rows > 8)  row_mask_ |= h.d;
    if (double_rows > 16) row_mask_ |= h.e;
    for (int i = 0; i < double_rows; ++i) {
      gpio_bits_t row_address = 0;
      row_address |= (i & 0x08) ? h.d : 0;
      row_address |= (i & 0x10) ? h.e : 0;
      row_lookup_[i] = row_address;
    }
  }

  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    if (row == last_row_) return;
    io->SetBits(bk_);  // Enable serial input for the shifter
    for (int r = 7; r >= 0; r--) {
      if (row % 8 == r) {
        io->SetBits(din_);
      } else {
        io->ClearBits(din_);
      }
      io->SetBits(dck_);
      io->SetBits(dck_);  // Longer clock time; tested with Pi3
      io->ClearBits(dck_);
    }
    io->ClearBits(bk_);  // Disable serial input to keep unwanted bits out of the shifters
    last_row_ = row;
    // Set bits D and E to enable the proper shifter to display the selected
    // row.
    io->WriteMaskedBits(row_lookup_[row], row_mask_);
  }

private:
  gpio_bits_t row_mask_;
  int last_row_;
  const gpio_bits_t bk_;
  const gpio_bits_t din_;
  const gpio_bits_t dck_;
  gpio_bits_t row_lookup_[32];
};

class B707ShiftRegisterRowAddressSetter : public RowAddressSetter {
public:
  B707ShiftRegisterRowAddressSetter(int double_rows, const HardwareMapping &h)
    : row_mask_(h.a | h.b | h.c),
      last_row_(-1),
      bk_(h.b),
      din_(h.c),
      dck_(h.a) {
    assert(double_rows <= 32); // designed for up to 1/32 panel
  }

  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    if (row == last_row_) return;
    io->SetBits(bk_);  // Enable serial input for the shifter
    if (row == 0) {
        io->SetBits(din_);
      } else {
        io->ClearBits(din_);
      }
    io->SetBits(dck_);
    io->SetBits(dck_);  // Longer clock time; tested with Pi3
    io->ClearBits(dck_);
    io->ClearBits(bk_);  // Disable serial input to keep unwanted bits out of the shifters
    last_row_ = row;
  }

private:
  gpio_bits_t row_mask_;
  int last_row_;
  const gpio_bits_t bk_;
  const gpio_bits_t din_;
  const gpio_bits_t dck_;
};


class ShiftRegisterRowAddressSetter : public RowAddressSetter {
public:
  ShiftRegisterRowAddressSetter(int double_rows, const HardwareMapping &h)
    : double_rows_(double_rows),
      row_mask_(h.a | h.b), clock_(h.a), data_(h.b),
      last_row_(-1) {
  }
  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    if (row == last_row_) return;
    for (int activate = 0; activate < double_rows_; ++activate) {
      io->ClearBits(clock_);
      if (activate == double_rows_ - 1 - row) {
        io->ClearBits(data_);
      } else {
        io->SetBits(data_);
      }
      io->SetBits(clock_);
    }
    io->ClearBits(clock_);
    io->SetBits(clock_);
    last_row_ = row;
  }

private:
  const int double_rows_;
  const gpio_bits_t row_mask_;
  const gpio_bits_t clock_;
  const gpio_bits_t data_;
  int last_row_;
};

// Issue #823
// An shift register row address setter that does not use B but C for the
// data. Clock is inverted.
class ABCShiftRegisterRowAddressSetter : public RowAddressSetter {
public:
  ABCShiftRegisterRowAddressSetter(int double_rows, const HardwareMapping &h)
    : double_rows_(double_rows),
      row_mask_(h.a | h.c),
      clock_(h.a),
      data_(h.c),
      last_row_(-1) {
  }
  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    for (int activate = 0; activate < double_rows_; ++activate) {
      io->ClearBits(clock_);
      if (activate == double_rows_ - 1 - row) {
        io->SetBits(data_);
      } else {
        io->ClearBits(data_);
      }
      io->SetBits(clock_);
    }
    io->SetBits(clock_);
    io->ClearBits(clock_);
    last_row_ = row;
  }

private:
  const int double_rows_;
  const gpio_bits_t row_mask_;
  const gpio_bits_t clock_;
  const gpio_bits_t data_;
  int last_row_;
};

// The DirectABCDRowAddressSetter sets the address by one of
// row pin ABCD for 32х16 matrix 1:4 multiplexing. The matrix has
// 4 addressable rows. Row is selected by a low level on the
// corresponding row address pin. Other row address pins must be in high level.
//
// Row addr| 0 | 1 | 2 | 3
// --------+---+---+---+---
// Line A  | 0 | 1 | 1 | 1
// Line B  | 1 | 0 | 1 | 1
// Line C  | 1 | 1 | 0 | 1
// Line D  | 1 | 1 | 1 | 0
class DirectABCDLineRowAddressSetter : public RowAddressSetter {
public:
  DirectABCDLineRowAddressSetter(int double_rows, const HardwareMapping &h)
    : last_row_(-1) {
	row_mask_ = h.a | h.b | h.c | h.d;

	row_lines_[0] = /*h.a |*/ h.b | h.c | h.d;
	row_lines_[1] = h.a /*| h.b*/ | h.c | h.d;
	row_lines_[2] = h.a | h.b /*| h.c */| h.d;
	row_lines_[3] = h.a | h.b | h.c /*| h.d*/;
  }

  virtual gpio_bits_t need_bits() const { return row_mask_; }

  virtual void SetRowAddress(GPIO *io, int row) {
    if (row == last_row_) return;

    gpio_bits_t row_address = row_lines_[row % 4];

    io->WriteMaskedBits(row_address, row_mask_);
    last_row_ = row;
  }

private:
  gpio_bits_t row_lines_[4];
  gpio_bits_t row_mask_;
  int last_row_;
};

}

const struct HardwareMapping *Framebuffer::hardware_mapping_ = NULL;
RowAddressSetter *Framebuffer::row_setter_ = NULL;

Framebuffer::Framebuffer(int rows, int columns, int parallel,
                         int scan_mode,
                         const char *led_sequence, bool inverse_color,
                         PixelDesignatorMap **mapper)
  : rows_(rows),
    parallel_(parallel),
    height_(rows * parallel),
    columns_(columns),
    scan_mode_(scan_mode),
    inverse_color_(inverse_color),
    pwm_bits_(kBitPlanes), do_luminance_correct_(true), brightness_(100),
    double_rows_(rows / SUB_PANELS_),
    buffer_size_(double_rows_ * columns_ * kBitPlanes * sizeof(gpio_bits_t)),
    shared_mapper_(mapper) {
  assert(hardware_mapping_ != NULL);   // Called InitHardwareMapping() ?
  assert(shared_mapper_ != NULL);  // Storage should be provided by RGBMatrix.
  assert(rows_ >=4 && rows_ <= 64 && rows_ % 2 == 0);
  if (parallel > hardware_mapping_->max_parallel_chains) {
    fprintf(stderr, "The %s GPIO mapping only supports %d parallel chain%s, "
            "but %d was requested.\n", hardware_mapping_->name,
            hardware_mapping_->max_parallel_chains,
            hardware_mapping_->max_parallel_chains > 1 ? "s" : "", parallel);
    abort();
  }
  assert(parallel >= 1 && parallel <= 6);

  bitplane_buffer_ = new gpio_bits_t[double_rows_ * columns_ * kBitPlanes];

  // If we're the first Framebuffer created, the shared PixelMapper is
  // still NULL, so create one.
  // The first PixelMapper represents the physical layout of a standard matrix
  // with the specific knowledge of the framebuffer, setting up PixelDesignators
  // in a way that they are useful for this Framebuffer.
  //
  // Newly created PixelMappers then can just re-arrange PixelDesignators
  // from the parent PixelMapper opaquely without having to know the details.
  if (*shared_mapper_ == NULL) {
    // Gather all the bits for given color for fast Fill()s and use the right
    // bits according to the led sequence
    const struct HardwareMapping &h = *hardware_mapping_;
    gpio_bits_t r = h.p0_r1 | h.p0_r2 | h.p1_r1 | h.p1_r2 | h.p2_r1 | h.p2_r2 | h.p3_r1 | h.p3_r2 | h.p4_r1 | h.p4_r2 | h.p5_r1 | h.p5_r2;
    gpio_bits_t g = h.p0_g1 | h.p0_g2 | h.p1_g1 | h.p1_g2 | h.p2_g1 | h.p2_g2 | h.p3_g1 | h.p3_g2 | h.p4_g1 | h.p4_g2 | h.p5_g1 | h.p5_g2;
    gpio_bits_t b = h.p0_b1 | h.p0_b2 | h.p1_b1 | h.p1_b2 | h.p2_b1 | h.p2_b2 | h.p3_b1 | h.p3_b2 | h.p4_b1 | h.p4_b2 | h.p5_b1 | h.p5_b2;
    PixelDesignator fill_bits;
    fill_bits.r_bit = GetGpioFromLedSequence('R', led_sequence, r, g, b);
    fill_bits.g_bit = GetGpioFromLedSequence('G', led_sequence, r, g, b);
    fill_bits.b_bit = GetGpioFromLedSequence('B', led_sequence, r, g, b);

    *shared_mapper_ = new PixelDesignatorMap(columns_, height_, fill_bits);
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < columns_; ++x) {
        InitDefaultDesignator(x, y, led_sequence, (*shared_mapper_)->get(x, y));
      }
    }
  }

  Clear();
}

Framebuffer::~Framebuffer() {
  delete [] bitplane_buffer_;
}

// TODO: this should also be parsed from some special formatted string, e.g.
// {addr={22,23,24,25,15},oe=18,clk=17,strobe=4, p0={11,27,7,8,9,10},...}
/* static */ void Framebuffer::InitHardwareMapping(const char *named_hardware) {
  if (named_hardware == NULL || *named_hardware == '\0') {
    named_hardware = "regular";
  }

  struct HardwareMapping *mapping = NULL;
  for (HardwareMapping *it = matrix_hardware_mappings; it->name; ++it) {
    if (strcasecmp(it->name, named_hardware) == 0) {
      mapping = it;
      break;
    }
  }

  if (!mapping) {
    fprintf(stderr, "There is no hardware mapping named '%s'.\nAvailable: ",
            named_hardware);
    for (HardwareMapping *it = matrix_hardware_mappings; it->name; ++it) {
      if (it != matrix_hardware_mappings) fprintf(stderr, ", ");
      fprintf(stderr, "'%s'", it->name);
    }
    fprintf(stderr, "\n");
    abort();
  }

  if (mapping->max_parallel_chains == 0) {
    // Auto determine.
    struct HardwareMapping *h = mapping;
    if ((h->p0_r1 | h->p0_g1 | h->p0_g1 | h->p0_r2 | h->p0_g2 | h->p0_g2) > 0)
      ++mapping->max_parallel_chains;
    if ((h->p1_r1 | h->p1_g1 | h->p1_g1 | h->p1_r2 | h->p1_g2 | h->p1_g2) > 0)
      ++mapping->max_parallel_chains;
    if ((h->p2_r1 | h->p2_g1 | h->p2_g1 | h->p2_r2 | h->p2_g2 | h->p2_g2) > 0)
      ++mapping->max_parallel_chains;
    if ((h->p3_r1 | h->p3_g1 | h->p3_g1 | h->p3_r2 | h->p3_g2 | h->p3_g2) > 0)
      ++mapping->max_parallel_chains;
    if ((h->p4_r1 | h->p4_g1 | h->p4_g1 | h->p4_r2 | h->p4_g2 | h->p4_g2) > 0)
      ++mapping->max_parallel_chains;
    if ((h->p5_r1 | h->p5_g1 | h->p5_g1 | h->p5_r2 | h->p5_g2 | h->p5_g2) > 0)
      ++mapping->max_parallel_chains;
  }
  hardware_mapping_ = mapping;
}

/* static */ void Framebuffer::InitGPIO(GPIO *io, int rows, int parallel,
                                        bool allow_hardware_pulsing,
                                        int pwm_lsb_nanoseconds,
                                        int dither_bits,
                                        int row_address_type) {
  if (sOutputEnablePulser != NULL)
    return;  // already initialized.

  const struct HardwareMapping &h = *hardware_mapping_;
  const int double_rows = rows / SUB_PANELS_;

  if (Rp1RioShouldActivate(h.name, row_address_type, parallel)) {
    Rp1RioInitOrDie(h, double_rows, parallel, pwm_lsb_nanoseconds, dither_bits,
                    row_address_type);
    return;
  }

  if (Rp1PioShouldActivate(h.name, row_address_type, parallel)) {
    Rp1PioInitOrDie(h, double_rows, parallel, pwm_lsb_nanoseconds, dither_bits,
                    row_address_type);
    return;
  }

  // Tell GPIO about all bits we intend to use.
  gpio_bits_t all_used_bits = 0;

  all_used_bits |= h.output_enable | h.clock | h.strobe;

  all_used_bits |= h.p0_r1 | h.p0_g1 | h.p0_b1 | h.p0_r2 | h.p0_g2 | h.p0_b2;
  if (parallel >= 2) {
    all_used_bits |= h.p1_r1 | h.p1_g1 | h.p1_b1 | h.p1_r2 | h.p1_g2 | h.p1_b2;
  }
  if (parallel >= 3) {
    all_used_bits |= h.p2_r1 | h.p2_g1 | h.p2_b1 | h.p2_r2 | h.p2_g2 | h.p2_b2;
  }
  if (parallel >= 4) {
    all_used_bits |= h.p3_r1 | h.p3_g1 | h.p3_b1 | h.p3_r2 | h.p3_g2 | h.p3_b2;
  }
  if (parallel >= 5) {
    all_used_bits |= h.p4_r1 | h.p4_g1 | h.p4_b1 | h.p4_r2 | h.p4_g2 | h.p4_b2;
  }
  if (parallel >= 6) {
    all_used_bits |= h.p5_r1 | h.p5_g1 | h.p5_b1 | h.p5_r2 | h.p5_g2 | h.p5_b2;
  }

  switch (row_address_type) {
  case 0:
    row_setter_ = new DirectRowAddressSetter(double_rows, h);
    break;
  case 1:
    row_setter_ = new ShiftRegisterRowAddressSetter(double_rows, h);
    break;
  case 2:
    row_setter_ = new DirectABCDLineRowAddressSetter(double_rows, h);
    break;
  case 3:
    row_setter_ = new ABCShiftRegisterRowAddressSetter(double_rows, h);
    break;
  case 4:
    row_setter_ = new SM5266RowAddressSetter(double_rows, h);
    break;
  case 5:
    row_setter_ = new B707ShiftRegisterRowAddressSetter(double_rows, h);
    break;


  default:
    assert(0);  // unexpected type.
  }

  all_used_bits |= row_setter_->need_bits();

  // Adafruit HAT identified by the same prefix.
  const bool is_some_adafruit_hat = (0 == strncmp(h.name, "adafruit-hat",
                                                  strlen("adafruit-hat")));
  // Initialize outputs, make sure that all of these are supported bits.
  const gpio_bits_t result = io->InitOutputs(all_used_bits,
                                             is_some_adafruit_hat);
  assert(result == all_used_bits);  // Impl: all bits declared in gpio.cc ?

  std::vector<int> bitplane_timings;
  uint32_t timing_ns = pwm_lsb_nanoseconds;
  for (int b = 0; b < kBitPlanes; ++b) {
    bitplane_timings.push_back(timing_ns);
    if (b >= dither_bits) timing_ns *= 2;
  }
  sOutputEnablePulser = PinPulser::Create(io, h.output_enable,
                                          allow_hardware_pulsing,
                                          bitplane_timings);
}

// NOTE: first version for panel initialization sequence, need to refine
// until it is more clear how different panel types are initialized to be
// able to abstract this more.

static void InitFM6126(GPIO *io, const struct HardwareMapping &h, int columns) {
  const gpio_bits_t bits_on
    = h.p0_r1 | h.p0_g1 | h.p0_b1 | h.p0_r2 | h.p0_g2 | h.p0_b2
    | h.p1_r1 | h.p1_g1 | h.p1_b1 | h.p1_r2 | h.p1_g2 | h.p1_b2
    | h.p2_r1 | h.p2_g1 | h.p2_b1 | h.p2_r2 | h.p2_g2 | h.p2_b2
    | h.p3_r1 | h.p3_g1 | h.p3_b1 | h.p3_r2 | h.p3_g2 | h.p3_b2
    | h.p4_r1 | h.p4_g1 | h.p4_b1 | h.p4_r2 | h.p4_g2 | h.p4_b2
    | h.p5_r1 | h.p5_g1 | h.p5_b1 | h.p5_r2 | h.p5_g2 | h.p5_b2
    | h.a;  // Address bit 'A' is always on.
  const gpio_bits_t bits_off = h.a;
  const gpio_bits_t mask = bits_on | h.strobe;

  // Init bits. TODO: customize, as we can do things such as brightness here,
  // which would allow more higher quality output.
  static const char* init_b12 = "0111111111111111";  // full bright
  static const char* init_b13 = "0000000001000000";  // panel on.

  io->ClearBits(h.clock | h.strobe);

  for (int i = 0; i < columns; ++i) {
    gpio_bits_t value = init_b12[i % 16] == '0' ? bits_off : bits_on;
    if (i > columns - 12) value |= h.strobe;
    io->WriteMaskedBits(value, mask);
    io->SetBits(h.clock);
    io->ClearBits(h.clock);
  }
  io->ClearBits(h.strobe);

  for (int i = 0; i < columns; ++i) {
    gpio_bits_t value = init_b13[i % 16] == '0' ? bits_off : bits_on;
    if (i > columns - 13) value |= h.strobe;
    io->WriteMaskedBits(value, mask);
    io->SetBits(h.clock);
    io->ClearBits(h.clock);
  }
  io->ClearBits(h.strobe);
}

// The FM6217 is very similar to the FM6216.
// FM6217 adds Register 3 to allow for automatic bad pixel suppression.
static void InitFM6127(GPIO *io, const struct HardwareMapping &h, int columns) {
  const gpio_bits_t bits_r_on= h.p0_r1 | h.p0_r2;
  const gpio_bits_t bits_g_on= h.p0_g1 | h.p0_g2;
  const gpio_bits_t bits_b_on= h.p0_b1 | h.p0_b2;
  const gpio_bits_t bits_on= bits_r_on | bits_g_on | bits_b_on;
  const gpio_bits_t bits_off = 0;

  const gpio_bits_t mask = bits_on | h.strobe;

  static const char* init_b12 = "1111111111001110";  // register 1
  static const char* init_b13 = "1110000001100010";  // register 2.
  static const char* init_b11 = "0101111100000000";  // register 3.
  io->ClearBits(h.clock | h.strobe);
  for (int i = 0; i < columns; ++i) {
    gpio_bits_t value = init_b12[i % 16] == '0' ? bits_off : bits_on;
    if (i > columns - 12) value |= h.strobe;
    io->WriteMaskedBits(value, mask);
    io->SetBits(h.clock);
    io->ClearBits(h.clock);
  }
  io->ClearBits(h.strobe);

  for (int i = 0; i < columns; ++i) {
    gpio_bits_t value = init_b13[i % 16] == '0' ? bits_off : bits_on;
    if (i > columns - 13) value |= h.strobe;
    io->WriteMaskedBits(value, mask);
    io->SetBits(h.clock);
    io->ClearBits(h.clock);
  }
  io->ClearBits(h.strobe);

  for (int i = 0; i < columns; ++i) {
    gpio_bits_t value = init_b11[i % 16] == '0' ? bits_off : bits_on;
    if (i > columns - 11) value |= h.strobe;
    io->WriteMaskedBits(value, mask);
    io->SetBits(h.clock);
    io->ClearBits(h.clock);
  }
  io->ClearBits(h.strobe);
}

/*static*/ void Framebuffer::InitializePanels(GPIO *io,
                                              const char *panel_type,
                                              int columns) {
  if (!panel_type || panel_type[0] == '\0') return;
  if (Rp1RioIsActive()) {
    Rp1RioInitializePanels(*hardware_mapping_, panel_type, columns);
    return;
  }
  if (Rp1PioIsActive()) {
    Rp1PioInitializePanels(*hardware_mapping_, panel_type, columns);
    return;
  }
  if (strncasecmp(panel_type, "fm6126", 6) == 0) {
    InitFM6126(io, *hardware_mapping_, columns);
  }
  else if (strncasecmp(panel_type, "fm6127", 6) == 0) {
    InitFM6127(io, *hardware_mapping_, columns);
  }
  // else if (strncasecmp(...))  // more init types
  else {
    fprintf(stderr, "Unknown panel type '%s'; typo ?\n", panel_type);
  }
}

bool Framebuffer::SetPWMBits(uint8_t value) {
  if (value < 1 || value > kBitPlanes)
    return false;
  pwm_bits_ = value;
  return true;
}

inline gpio_bits_t *Framebuffer::ValueAt(int double_row, int column, int bit) {
  return &bitplane_buffer_[ double_row * (columns_ * kBitPlanes)
                            + bit * columns_
                            + column ];
}

void Framebuffer::Clear() {
  if (inverse_color_) {
    Fill(0, 0, 0);
  } else  {
    // Cheaper.
    memset(bitplane_buffer_, 0,
           sizeof(*bitplane_buffer_) * double_rows_ * columns_ * kBitPlanes);
  }
}

struct ColorLookup {
  uint16_t color[256];
};

class ColorLookupTable {
  public:
    static const ColorLookup &GetLookup(uint8_t brightness) {
      static ColorLookupTable instance;
      return instance.lookups_[brightness - 1];
    }


  private:
    // Do CIE1931 luminance correction and scale to output bitplanes
    static uint16_t luminance_cie1931(uint8_t c, uint8_t brightness) {
      float out_factor = ((1 << internal::Framebuffer::kBitPlanes) - 1);
      float v = (float) c * brightness / 255.0;
      return roundf(out_factor * ((v <= 8) ? v / 902.3 : pow((v + 16) / 116.0, 3)));
    }

    ColorLookupTable() {
      for (int c = 0; c < 256; ++c)
        for (int b = 0; b < 100; ++b)
          lookups_[b].color[c] = luminance_cie1931(c, b + 1);
    }

    ColorLookup lookups_[100]{};
  };

  static inline uint16_t CIEMapColor(uint8_t brightness, uint8_t c) {
    return ColorLookupTable::GetLookup(brightness).color[c];
  }

// Non luminance correction. TODO: consider getting rid of this.
static inline uint16_t DirectMapColor(uint8_t brightness, uint8_t c) {
  // simple scale down the color value
  c = c * brightness / 100;

  // shift to be left aligned with top-most bits.
  constexpr int shift = internal::Framebuffer::kBitPlanes - 8;
  return (shift > 0) ? (c << shift) : (c >> -shift);
}

inline void Framebuffer::MapColors(
  uint8_t r, uint8_t g, uint8_t b,
  uint16_t *red, uint16_t *green, uint16_t *blue) {

  if (do_luminance_correct_) {
    *red   = CIEMapColor(brightness_, r);
    *green = CIEMapColor(brightness_, g);
    *blue  = CIEMapColor(brightness_, b);
  } else {
    *red   = DirectMapColor(brightness_, r);
    *green = DirectMapColor(brightness_, g);
    *blue  = DirectMapColor(brightness_, b);
  }

  if (inverse_color_) {
    *red = ~(*red);
    *green = ~(*green);
    *blue = ~(*blue);
  }
}

void Framebuffer::Fill(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t red, green, blue;
  MapColors(r, g, b, &red, &green, &blue);
  const PixelDesignator &fill = (*shared_mapper_)->GetFillColorBits();

  for (int bits = kBitPlanes - pwm_bits_; bits < kBitPlanes; ++bits) {
    uint16_t mask = 1 << bits;
    gpio_bits_t plane_bits = 0;
    plane_bits |= ((red & mask) == mask)   ? fill.r_bit : 0;
    plane_bits |= ((green & mask) == mask) ? fill.g_bit : 0;
    plane_bits |= ((blue & mask) == mask)  ? fill.b_bit : 0;

    for (int row = 0; row < double_rows_; ++row) {
      gpio_bits_t *row_data = ValueAt(row, 0, bits);
      for (int col = 0; col < columns_; ++col) {
        *row_data++ = plane_bits;
      }
    }
  }
}

void Framebuffer::SubFill(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b) {

  uint16_t red, green, blue;
  MapColors(r, g, b, &red, &green, &blue);

  int safe_y = std::max(0, y);
  int safe_y_max = std::min((*shared_mapper_)->height(), y + height);
  int safe_x = std::max(0, x);
  int safe_x_max = std::min((*shared_mapper_)->width(), x + width);

  for (int row = safe_y; row < safe_y_max; row++)
  {
    const PixelDesignator* designator = (*shared_mapper_)->get(safe_x, row);

    for (int col = safe_x; col < safe_x_max; col++)
    {
      if (designator == NULL) continue;
      const long pos = designator->gpio_word;
      if (pos < 0) continue;  // non-used pixel marker.

      gpio_bits_t* bits = bitplane_buffer_ + pos;
      const int min_bit_plane = kBitPlanes - pwm_bits_;
      bits += (columns_ * min_bit_plane);
      const gpio_bits_t r_bits = designator->r_bit;
      const gpio_bits_t g_bits = designator->g_bit;
      const gpio_bits_t b_bits = designator->b_bit;
      const gpio_bits_t designator_mask = designator->mask;
      for (uint16_t mask = 1 << min_bit_plane; mask != 1 << kBitPlanes; mask <<= 1) {
        gpio_bits_t color_bits = 0;
        if (red & mask)   color_bits |= r_bits;
        if (green & mask) color_bits |= g_bits;
        if (blue & mask)  color_bits |= b_bits;
        *bits = (*bits & designator_mask) | color_bits;
        bits += columns_;
      }
      designator++;
    }
  }
}

int Framebuffer::width() const { return (*shared_mapper_)->width(); }
int Framebuffer::height() const { return (*shared_mapper_)->height(); }

void Framebuffer::SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  const PixelDesignator *designator = (*shared_mapper_)->get(x, y);
  if (designator == NULL) return;
  const long pos = designator->gpio_word;
  if (pos < 0) return;  // non-used pixel marker.

  uint16_t red, green, blue;
  MapColors(r, g, b, &red, &green, &blue);

  gpio_bits_t *bits = bitplane_buffer_ + pos;
  const int min_bit_plane = kBitPlanes - pwm_bits_;
  bits += (columns_ * min_bit_plane);
  const gpio_bits_t r_bits = designator->r_bit;
  const gpio_bits_t g_bits = designator->g_bit;
  const gpio_bits_t b_bits = designator->b_bit;
  const gpio_bits_t designator_mask = designator->mask;
  for (uint16_t mask = 1<<min_bit_plane; mask != 1<<kBitPlanes; mask <<=1 ) {
    gpio_bits_t color_bits = 0;
    if (red & mask)   color_bits |= r_bits;
    if (green & mask) color_bits |= g_bits;
    if (blue & mask)  color_bits |= b_bits;
    *bits = (*bits & designator_mask) | color_bits;
    bits += columns_;
  }
}

void Framebuffer::SetPixels(int x, int y, int width, int height, Color *colors) {
  for (int iy = 0; iy < height; ++iy) {
    for (int ix = 0; ix < width; ++ix) {
      SetPixel(x + ix, y + iy, colors->r, colors->g, colors->b);
      ++colors;
    }
  }
}
// Strange LED-mappings such as RBG or so are handled here.
gpio_bits_t Framebuffer::GetGpioFromLedSequence(char col,
                                                const char *led_sequence,
                                                gpio_bits_t default_r,
                                                gpio_bits_t default_g,
                                                gpio_bits_t default_b) {
  const char *pos = strchr(led_sequence, col);
  if (pos == NULL) pos = strchr(led_sequence, tolower(col));
  if (pos == NULL) {
    fprintf(stderr, "LED sequence '%s' does not contain any '%c'.\n",
            led_sequence, col);
    abort();
  }
  switch (pos - led_sequence) {
  case 0: return default_r;
  case 1: return default_g;
  case 2: return default_b;
  }
  return default_r;  // String too long, should've been caught earlier.
}

void Framebuffer::InitDefaultDesignator(int x, int y, const char *seq,
                                        PixelDesignator *d) {
  const struct HardwareMapping &h = *hardware_mapping_;
  gpio_bits_t *bits = ValueAt(y % double_rows_, x, 0);
  d->gpio_word = bits - bitplane_buffer_;
  d->r_bit = d->g_bit = d->b_bit = 0;
  if (y < rows_) {
    if (y < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p0_r1, h.p0_g1, h.p0_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p0_r1, h.p0_g1, h.p0_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p0_r1, h.p0_g1, h.p0_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p0_r2, h.p0_g2, h.p0_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p0_r2, h.p0_g2, h.p0_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p0_r2, h.p0_g2, h.p0_b2);
    }
  }
  else if (y >= rows_ && y < 2 * rows_) {
    if (y - rows_ < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p1_r1, h.p1_g1, h.p1_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p1_r1, h.p1_g1, h.p1_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p1_r1, h.p1_g1, h.p1_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p1_r2, h.p1_g2, h.p1_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p1_r2, h.p1_g2, h.p1_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p1_r2, h.p1_g2, h.p1_b2);
    }
  }
  else if (y >= 2*rows_ && y < 3 * rows_) {
    if (y - 2*rows_ < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p2_r1, h.p2_g1, h.p2_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p2_r1, h.p2_g1, h.p2_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p2_r1, h.p2_g1, h.p2_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p2_r2, h.p2_g2, h.p2_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p2_r2, h.p2_g2, h.p2_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p2_r2, h.p2_g2, h.p2_b2);
    }
  }
  else if (y >= 3*rows_ && y < 4 * rows_) {
    if (y - 3*rows_ < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p3_r1, h.p3_g1, h.p3_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p3_r1, h.p3_g1, h.p3_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p3_r1, h.p3_g1, h.p3_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p3_r2, h.p3_g2, h.p3_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p3_r2, h.p3_g2, h.p3_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p3_r2, h.p3_g2, h.p3_b2);
    }
  }
  else if (y >= 4*rows_ && y < 5 * rows_){
    if (y - 4*rows_ < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p4_r1, h.p4_g1, h.p4_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p4_r1, h.p4_g1, h.p4_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p4_r1, h.p4_g1, h.p4_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p4_r2, h.p4_g2, h.p4_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p4_r2, h.p4_g2, h.p4_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p4_r2, h.p4_g2, h.p4_b2);
    }

  }
  else {
    if (y - 5*rows_ < double_rows_) {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p5_r1, h.p5_g1, h.p5_b1);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p5_r1, h.p5_g1, h.p5_b1);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p5_r1, h.p5_g1, h.p5_b1);
    } else {
      d->r_bit = GetGpioFromLedSequence('R', seq, h.p5_r2, h.p5_g2, h.p5_b2);
      d->g_bit = GetGpioFromLedSequence('G', seq, h.p5_r2, h.p5_g2, h.p5_b2);
      d->b_bit = GetGpioFromLedSequence('B', seq, h.p5_r2, h.p5_g2, h.p5_b2);
    }
  }

  d->mask = ~(d->r_bit | d->g_bit | d->b_bit);
}

void Framebuffer::Serialize(const char **data, size_t *len) const {
  *data = reinterpret_cast<const char*>(bitplane_buffer_);
  *len = buffer_size_;
}

bool Framebuffer::Deserialize(const char *data, size_t len) {
  if (len != buffer_size_) return false;
  memcpy(bitplane_buffer_, data, len);
  return true;
}

void Framebuffer::CopyFrom(const Framebuffer *other) {
  if (other == this) return;
  memcpy(bitplane_buffer_, other->bitplane_buffer_, buffer_size_);
}

void Framebuffer::DumpToMatrix(GPIO *io, int pwm_low_bit) {
  if (Rp1RioIsActive()) {
    Rp1RioDumpFramebuffer(this, pwm_low_bit);
    return;
  }
  if (Rp1PioIsActive()) {
    Rp1PioDumpFramebuffer(this, pwm_low_bit);
    return;
  }

  const struct HardwareMapping &h = *hardware_mapping_;
  gpio_bits_t color_clk_mask = 0;  // Mask of bits while clocking in.
  color_clk_mask |= h.p0_r1 | h.p0_g1 | h.p0_b1 | h.p0_r2 | h.p0_g2 | h.p0_b2;
  if (parallel_ >= 2) {
    color_clk_mask |= h.p1_r1 | h.p1_g1 | h.p1_b1 | h.p1_r2 | h.p1_g2 | h.p1_b2;
  }
  if (parallel_ >= 3) {
    color_clk_mask |= h.p2_r1 | h.p2_g1 | h.p2_b1 | h.p2_r2 | h.p2_g2 | h.p2_b2;
  }
  if (parallel_ >= 4) {
    color_clk_mask |= h.p3_r1 | h.p3_g1 | h.p3_b1 | h.p3_r2 | h.p3_g2 | h.p3_b2;
  }
  if (parallel_ >= 5) {
    color_clk_mask |= h.p4_r1 | h.p4_g1 | h.p4_b1 | h.p4_r2 | h.p4_g2 | h.p4_b2;
  }
  if (parallel_ >= 6) {
    color_clk_mask |= h.p5_r1 | h.p5_g1 | h.p5_b1 | h.p5_r2 | h.p5_g2 | h.p5_b2;
  }

  color_clk_mask |= h.clock;

  // Depending if we do dithering, we might not always show the lowest bits.
  const int start_bit = std::max(pwm_low_bit, kBitPlanes - pwm_bits_);

  const uint8_t half_double = double_rows_/2;
  for (uint8_t row_loop = 0; row_loop < double_rows_; ++row_loop) {
    uint8_t d_row;
    switch (scan_mode_) {
    case 0:  // progressive
    default:
      d_row = row_loop;
      break;

    case 1:  // interlaced
      d_row = ((row_loop < half_double)
               ? (row_loop << 1)
               : ((row_loop - half_double) << 1) + 1);
    }

    // Rows can't be switched very quickly without ghosting, so we do the
    // full PWM of one row before switching rows.
    for (int b = start_bit; b < kBitPlanes; ++b) {
      gpio_bits_t *row_data = ValueAt(d_row, 0, b);
      // While the output enable is still on, we can already clock in the next
      // data.
      for (int col = 0; col < columns_; ++col) {
        const gpio_bits_t &out = *row_data++;
        io->WriteMaskedBits(out, color_clk_mask);  // col + reset clock
        io->SetBits(h.clock);               // Rising edge: clock color in.
      }
      io->ClearBits(color_clk_mask);    // clock back to normal.

      // OE of the previous row-data must be finished before strobe.
      sOutputEnablePulser->WaitPulseFinished();

      // Setting address and strobing needs to happen in dark time.
      row_setter_->SetRowAddress(io, d_row);

      io->SetBits(h.strobe);   // Strobe in the previously clocked in row.
      io->ClearBits(h.strobe);

      // Now switch on for the sleep time necessary for that bit-plane.
      sOutputEnablePulser->SendPulse(b);
    }
  }
}
}  // namespace internal
}  // namespace rgb_matrix
namespace rgb_matrix {
namespace internal {
  void Framebuffer::ResetGlobals() {
    if (sOutputEnablePulser != NULL) {
      delete sOutputEnablePulser;
      sOutputEnablePulser = NULL;
    }
    Rp1RioDeinit();
    Rp1PioDeinit();
    if (row_setter_ != NULL) {
      delete row_setter_;
      row_setter_ = NULL;
    }
  }
}
}

extern "C" {
  void framebuffer_reset_globals() {
    rgb_matrix::internal::Framebuffer::ResetGlobals();
    // Also reset global GPIO bookkeeping in led-matrix's static GPIO object
    extern void ledmatrix_reset_global_gpio();
    ledmatrix_reset_global_gpio();
  }
}
