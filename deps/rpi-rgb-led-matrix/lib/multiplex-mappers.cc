// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2017 Henner Zeller <h.zeller@acm.org>
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

#include "multiplex-mappers-internal.h"

namespace rgb_matrix {
namespace internal {
// A Pixel Mapper maps physical pixels locations to the internal logical
// mapping in a panel or panel-assembly, which depends on the wiring.
class MultiplexMapperBase : public MultiplexMapper {
public:
  MultiplexMapperBase(const char *name, int stretch_factor)
    : name_(name), panel_stretch_factor_(stretch_factor) {}

  // This method is const, but we sneakily remember the original size
  // of the panels so that we can more easily quantize things.
  // So technically, we're stateful, but let's pretend we're not changing
  // state. In the context this is used, it is never accessed in multiple
  // threads.
  virtual void EditColsRows(int *cols, int *rows) const {
    panel_rows_ = *rows;
    panel_cols_ = *cols;

    *rows /= panel_stretch_factor_;
    *cols *= panel_stretch_factor_;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height) const {
    // Matrix width has been altered. Alter it back.
    *visible_width = matrix_width / panel_stretch_factor_;
    *visible_height = matrix_height * panel_stretch_factor_;
    return true;
  }

  virtual const char *GetName() const { return name_; }

  // The MapVisibleToMatrix() as required by PanelMatrix here breaks it
  // down to the individual panel, so that derived classes only need to
  // implement MapSinglePanel().
  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int visible_x, int visible_y,
                                  int *matrix_x, int *matrix_y) const {
    const int chained_panel  = visible_x / panel_cols_;
    const int parallel_panel = visible_y / panel_rows_;

    const int within_panel_x = visible_x % panel_cols_;
    const int within_panel_y = visible_y % panel_rows_;

    int new_x, new_y;
    MapSinglePanel(within_panel_x, within_panel_y, &new_x, &new_y);
    *matrix_x = chained_panel  * panel_stretch_factor_*panel_cols_ + new_x;
    *matrix_y = parallel_panel * panel_rows_/panel_stretch_factor_ + new_y;
  }

  // Map the coordinates for a single panel. This is to be overridden in
  // derived classes.
  // Input parameter is the visible position on the matrix, and this method
  // should return the internal multiplexed position.
  virtual void MapSinglePanel(int visible_x, int visible_y,
                              int *matrix_x, int *matrix_y) const = 0;

protected:
  const char *const name_;
  const int panel_stretch_factor_;

  mutable int panel_cols_;
  mutable int panel_rows_;
};


/* ========================================================================
 * Multiplexer implementations.
 *
 * Extend MultiplexMapperBase and implement MapSinglePanel. You only have
 * to worry about the mapping within a single panel, the overall panel
 * construction with chains and parallel is already taken care of.
 *
 * Don't forget to register the new multiplexer sin CreateMultiplexMapperList()
 * below. After that, the new mapper is available in the --led-multiplexing
 * option.
 */
class StripeMultiplexMapper : public MultiplexMapperBase {
public:
  StripeMultiplexMapper() : MultiplexMapperBase("Stripe", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_top_stripe = (y % (panel_rows_/2)) < panel_rows_/4;
    *matrix_x = is_top_stripe ? x + panel_cols_ : x;
    *matrix_y = ((y / (panel_rows_/2)) * (panel_rows_/4)
                 + y % (panel_rows_/4));
  }
};

class FlippedStripeMultiplexMapper : public MultiplexMapperBase {
public:
  FlippedStripeMultiplexMapper() : MultiplexMapperBase("FlippedStripe", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_top_stripe = (y % (panel_rows_/2)) >= panel_rows_/4;
    *matrix_x = is_top_stripe ? x + panel_cols_ : x;
    *matrix_y = ((y / (panel_rows_/2)) * (panel_rows_/4)
                 + y % (panel_rows_/4));
  }
};

class CheckeredMultiplexMapper : public MultiplexMapperBase {
public:
  CheckeredMultiplexMapper() : MultiplexMapperBase("Checkered", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_top_check = (y % (panel_rows_/2)) < panel_rows_/4;
    const bool is_left_check = (x < panel_cols_/2);
    if (is_top_check) {
      *matrix_x = is_left_check ? x+panel_cols_/2 : x+panel_cols_;
    } else {
      *matrix_x = is_left_check ? x : x + panel_cols_/2;
    }
    *matrix_y = ((y / (panel_rows_/2)) * (panel_rows_/4)
                 + y % (panel_rows_/4));
  }
};

class SpiralMultiplexMapper : public MultiplexMapperBase {
public:
  SpiralMultiplexMapper() : MultiplexMapperBase("Spiral", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_top_stripe = (y % (panel_rows_/2)) < panel_rows_/4;
    const int panel_quarter = panel_cols_/4;
    const int quarter = x / panel_quarter;
    const int offset = x % panel_quarter;
    *matrix_x = ((2*quarter*panel_quarter)
                 + (is_top_stripe
                    ? panel_quarter - 1 - offset
                    : panel_quarter + offset));
    *matrix_y = ((y / (panel_rows_/2)) * (panel_rows_/4)
                 + y % (panel_rows_/4));
  }
};

class ZStripeMultiplexMapper : public MultiplexMapperBase {
public:
  ZStripeMultiplexMapper(const char *name, int even_vblock_offset, int odd_vblock_offset)
  : MultiplexMapperBase(name, 2),
    even_vblock_offset_(even_vblock_offset),
    odd_vblock_offset_(odd_vblock_offset) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    static const int tile_width = 8;
    static const int tile_height = 4;

    const int vert_block_is_odd = ((y / tile_height) % 2);

    const int even_vblock_shift = (1 - vert_block_is_odd) * even_vblock_offset_;
    const int odd_vblock_shitf = vert_block_is_odd * odd_vblock_offset_;

    *matrix_x = x + ((x + even_vblock_shift) / tile_width) * tile_width + odd_vblock_shitf;
    *matrix_y = (y % tile_height) + tile_height * (y / (tile_height * 2));
  }

private:
  const int even_vblock_offset_;
  const int odd_vblock_offset_;
};

class CoremanMapper : public MultiplexMapperBase {
public:
  CoremanMapper() : MultiplexMapperBase("coreman", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_left_check = (x < panel_cols_/2);

    if ((y <= 7) || ((y >= 16) && (y <= 23))){
      *matrix_x = ((x / (panel_cols_/2)) * panel_cols_) + (x % (panel_cols_/2));
      if ((y & (panel_rows_/4)) == 0) {
        *matrix_y = (y / (panel_rows_/2)) * (panel_rows_/4) + (y % (panel_rows_/4));
      }
    } else {
      *matrix_x = is_left_check ? x + panel_cols_/2 : x + panel_cols_;
      *matrix_y = (y / (panel_rows_/2)) * (panel_rows_/4) + y % (panel_rows_/4);
    }
  }
};

class Kaler2ScanMapper : public MultiplexMapperBase {
public:
  Kaler2ScanMapper() : MultiplexMapperBase("Kaler2Scan", 4) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    // Now we have a 128x4 matrix
    int offset = ((y%4)/2) == 0 ? -1 : 1;// Add or substract
    int deltaOffset = offset < 0 ? 7:8;
    int deltaColumn = ((y%8)/4)== 0 ? 64 : 0;

    *matrix_y = (y%2+(y/8)*2);
    *matrix_x = deltaColumn + (16 * (x/8)) + deltaOffset + ((x%8) * offset);

  }
};

class P10MapperZ : public MultiplexMapperBase {
public:
  P10MapperZ() : MultiplexMapperBase("P10-128x4-Z", 4) {}
  // supports this panel: https://www.aliexpress.com/item/2017-Special-Offer-P10-Outdoor-Smd-Full-Color-Led-Display-Module-320x160mm-1-2-Scan-Outdoor/32809267439.html?spm=a2g0s.9042311.0.0.Ob0jEw
  // with --led-row-addr-type=2 flag
  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    int yComp = 0;
    if (y == 0 || y == 1 || y == 8 || y == 9) {
      yComp = 127;
    }
    else if (y == 2 || y == 3 || y == 10 || y == 11) {
      yComp = 112;
    }
    else if (y == 4 || y == 5 || y == 12 || y == 13) {
      yComp = 111;
    }
    else if (y == 6 || y == 7 || y == 14 || y == 15) {
      yComp = 96;
    }

    if (y == 0 || y == 1 || y == 4 || y == 5 ||
        y == 8 || y == 9 || y == 12 || y == 13) {
      *matrix_x = yComp - x;
      *matrix_x -= (24 * ((int)(x / 8)));
    }
    else {
      *matrix_x = yComp + x;
      *matrix_x -= (40 * ((int)(x / 8)));
    }

    if (y == 0 || y == 2 || y == 4 || y == 6) {
      *matrix_y = 3;
    }
    else if (y == 1 || y == 3 || y == 5 || y == 7) {
      *matrix_y = 2;
    }
    else if (y == 8 || y == 10 || y == 12 || y == 14) {
      *matrix_y = 1;
    }
    else if (y == 9 || y == 11 || y == 13 || y == 15) {
      *matrix_y = 0;
    }
  }
};

class QiangLiQ8 : public MultiplexMapperBase {
public:
  QiangLiQ8() : MultiplexMapperBase("QiangLiQ8", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const int column = x + (4+ 4*(x/4));
    *matrix_x = column;
    if ((y >= 15 && y <=19) || (y >= 5 && y <= 9)) {
      const int reverseColumn = x + (4*(x/4));
      *matrix_x = reverseColumn;
    }
    *matrix_y = y % 5 + (y/10) *5;
  }
};

class InversedZStripe : public MultiplexMapperBase {
public:
  InversedZStripe() : MultiplexMapperBase("InversedZStripe", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    static const int tile_width = 8;
    static const int tile_height = 4;

    const int vert_block_is_odd = ((y / tile_height) % 2);
    const int evenOffset[8] = {7, 5, 3, 1, -1, -3, -5, -7};

    if (vert_block_is_odd) {
      *matrix_x = x + (x / tile_width) * tile_width;
    } else {
      *matrix_x = x + (x / tile_width) * tile_width + 8 + evenOffset[x % 8];
    }
    *matrix_y = (y % tile_height) + tile_height * (y / (tile_height * 2));
  }
};


/*
 * Various P10 1R1G1B Outdoor implementations for 16x16 modules with separate
 * RGB LEDs, e.g.:
 * https://www.ledcontrollercard.com/english/p10-outdoor-rgb-led-module-160x160mm-dip.html
 *
 */
class P10Outdoor1R1G1BMultiplexBase : public MultiplexMapperBase {
public:
  P10Outdoor1R1G1BMultiplexBase(const char *name)
    : MultiplexMapperBase(name, 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const int vblock_is_odd = (y / tile_height_) % 2;
    const int vblock_is_even = 1 - vblock_is_odd;
    const int even_vblock_shift = vblock_is_even * even_vblock_offset_;
    const int odd_vblock_shift = vblock_is_odd * odd_vblock_offset_;

    MapPanel(x, y, matrix_x, matrix_y,
             vblock_is_even, vblock_is_odd,
             even_vblock_shift, odd_vblock_shift);
  }

protected:
  virtual void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                        int vblock_is_even, int vblock_is_odd,
                        int even_vblock_shift, int odd_vblock_shift) const = 0;

  static const int tile_width_ = 8;
  static const int tile_height_ = 4;
  static const int even_vblock_offset_ = 0;
  static const int odd_vblock_offset_ = 8;
};

class P10Outdoor1R1G1BMultiplexMapper1 : public P10Outdoor1R1G1BMultiplexBase {
public:
  P10Outdoor1R1G1BMultiplexMapper1()
    : P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-1") {}

protected:
  void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                const int vblock_is_even, const int vblock_is_odd,
                const int even_vblock_shift, const int odd_vblock_shift) const {
    *matrix_x = tile_width_ * (1 + vblock_is_even + 2 * (x / tile_width_))
      - (x % tile_width_) - 1;
    *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
  }
};

class P10Outdoor1R1G1BMultiplexMapper2 : public P10Outdoor1R1G1BMultiplexBase {
public:
  P10Outdoor1R1G1BMultiplexMapper2()
    : P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-2") {}

protected:
  void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                const int vblock_is_even, const int vblock_is_odd,
                const int even_vblock_shift, const int odd_vblock_shift) const {
    *matrix_x = vblock_is_even
      ? tile_width_ * (1 + 2 * (x / tile_width_)) - (x % tile_width_) - 1
      : x + ((x + even_vblock_shift) / tile_width_) * tile_width_ + odd_vblock_shift;
    *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
  }
};

class P10Outdoor1R1G1BMultiplexMapper3 : public P10Outdoor1R1G1BMultiplexBase {
public:
  P10Outdoor1R1G1BMultiplexMapper3()
    : P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-3") {}

protected:
  void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                const int vblock_is_even, const int vblock_is_odd,
                const int even_vblock_shift, const int odd_vblock_shift) const {
    *matrix_x = vblock_is_odd
      ? tile_width_ * (2 + 2 * (x / tile_width_)) - (x % tile_width_) - 1
      : x + ((x + even_vblock_shift) / tile_width_) * tile_width_ + odd_vblock_shift;
    *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
  }
};

class P10CoremanMapper : public MultiplexMapperBase {
public:
  P10CoremanMapper() : MultiplexMapperBase("P10CoremanMapper", 4) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    //Row offset 8,8,8,8,0,0,0,0,8,8,8,8,0,0,0,0
    int mulY = (y & 4) > 0 ? 0 : 8;

    //Row offset 9,9,8,8,1,1,0,0,9,9,8,8,1,1,0,0
    mulY += (y & 2) > 0 ? 0 : 1;
    mulY += (x >> 2) & ~1; //Drop lsb

    *matrix_x = (mulY << 3) + x % 8;
    *matrix_y = (y & 1) + ((y >> 2) & ~1);
  }
};

/*
 * P8 1R1G1B Outdoor P8-5S-V3.2-HX 20x40
 */
class P8Outdoor1R1G1BMultiplexBase : public MultiplexMapperBase {
public:
  P8Outdoor1R1G1BMultiplexBase(const char *name)
    : MultiplexMapperBase(name, 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const int vblock_is_odd = (y / tile_height_) % 2;
    const int vblock_is_even = 1 - vblock_is_odd;
    const int even_vblock_shift = vblock_is_even * even_vblock_offset_;
    const int odd_vblock_shift = vblock_is_odd * odd_vblock_offset_;

    MapPanel(x, y, matrix_x, matrix_y,
             vblock_is_even, vblock_is_odd,
             even_vblock_shift, odd_vblock_shift);
  }

protected:
  virtual void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                        int vblock_is_even, int vblock_is_odd,
                        int even_vblock_shift, int odd_vblock_shift) const = 0;

  static const int tile_width_ = 8;
  static const int tile_height_ = 5;
  static const int even_vblock_offset_ = 0;
  static const int odd_vblock_offset_ = 8;
};

class P8Outdoor1R1G1BMultiplexMapper : public P8Outdoor1R1G1BMultiplexBase {
public:
  P8Outdoor1R1G1BMultiplexMapper()
    : P8Outdoor1R1G1BMultiplexBase("P8Outdoor1R1G1") {}

protected:
  void MapPanel(int x, int y, int *matrix_x, int *matrix_y,
                const int vblock_is_even, const int vblock_is_odd,
                const int even_vblock_shift, const int odd_vblock_shift) const {


    *matrix_x = vblock_is_even
      ? tile_width_ * (1 + tile_width_ - 2 * (x / tile_width_)) + tile_width_ - (x % tile_width_) - 1
      : tile_width_ * (1 + tile_width_ - 2 * (x / tile_width_)) - tile_width_ + (x % tile_width_);

    *matrix_y = (tile_height_ - y % tile_height_) + tile_height_ * (1 - y / (tile_height_ * 2)) -1;

  }
};

class P10Outdoor32x16HalfScanMapper : public MultiplexMapperBase {
public:
  P10Outdoor32x16HalfScanMapper() : MultiplexMapperBase("P10Outdoor32x16HalfScan", 4) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    int base = (x/8)*32;
    bool reverse = (y%4)/2 == 0;
    int offset = (3-((y%8)/2))*8;
    int dx = x%8;

    *matrix_y = (y/8 == 0) ? ((y%2 == 0) ? 0:1) : ((y%2 == 0) ? 2:3);
    *matrix_x = base + (reverse ? offset + (7-dx) : offset + dx);
  }
};

class P10Outdoor32x16QuarterScanMapper : public MultiplexMapperBase {
public:
  P10Outdoor32x16QuarterScanMapper() : MultiplexMapperBase("P10Outdoor32x16QuarterScanMapper", 4) {}
  // P10 quarter scan panel, e.g. https://www.ebay.com.au/itm/175517677191

  void EditColsRows(int *cols, int *rows) const {
    panel_rows_ = *rows;
    panel_cols_ = *cols;
  
    *rows /= panel_stretch_factor_/2;  // has half stretch factor in y compared to x
    *cols *= panel_stretch_factor_;
  }

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    int cell_starting_point = (x/8)*32;
    int delta_x = x%8;
    int offset = (3 - (y/4))*8;
    *matrix_x = cell_starting_point + delta_x + offset;
    *matrix_y = y%4;
  }
};


class P3Outdoor64x64MultiplexMapper : public MultiplexMapperBase {
public:
  P3Outdoor64x64MultiplexMapper() : MultiplexMapperBase("P3Outdoor64x64MultiplexMapper", 2) {}
  // P3 RGB panel 64x64
  // with pattern   [1] [3]
  //                 | \ |
  //                [0] [2]

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
    const bool is_top_stripe = (y % (panel_rows_/2)) < panel_rows_/4;
    *matrix_x = ((x*2) + (is_top_stripe ? 1 : 0));
    *matrix_y = ((y / (panel_rows_/2)) * (panel_rows_/4)
                 + y % (panel_rows_/4));
  }
};

class DoubleZMultiplexMapper : public MultiplexMapperBase {
  public:
    DoubleZMultiplexMapper() : MultiplexMapperBase("DoubleZ", 2) {}
       
    void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {
      const int quarter_rows  = panel_rows_ / 4;
      const int quarter_cols  = panel_cols_ / 4;
  
      const int y_quarter = y / quarter_rows;   // 0..3
      const int x_quarter = x / quarter_cols;   // 0..3
  
      const int offset_y  = y % quarter_rows;
      int offset_x        = x % quarter_cols;
  
      const bool flip_quarter = (y_quarter == 1 || y_quarter == 3);
      if (flip_quarter)
          offset_x = quarter_cols - 1 - offset_x;
  
      const bool is_top_stripe = !((y % (panel_rows_ / 2)) < quarter_rows);
  
      // Compute matrix_x: mirrors within half panels depending on stripe position
      const int base_x = 2 * x_quarter * quarter_cols;
      const int top_offset = is_top_stripe
          ? (quarter_cols - 1 - offset_x)
          : (quarter_cols + offset_x);
  
      *matrix_x = base_x + top_offset;
      *matrix_y = (y_quarter / 2) * quarter_rows + offset_y;
    }    
  };

/*
 * P4 Outdoor panel 80x40 pixels
 * https://nl.aliexpress.com/item/1005003999341251.html?spm=a2g0o.order_list.order_list_main.11.408679d2q5LwTb&gatewayAdapt=glo2nld
 */
class P4Outdoor80x40Mapper : public MultiplexMapperBase {
public:
  P4Outdoor80x40Mapper() : MultiplexMapperBase("P4Outdoor80x40Mapper", 2) {}

  void MapSinglePanel(int x, int y, int *matrix_x, int *matrix_y) const {

    const int tile_width_ = 8;
    const int tile_height_ = 10;
    const int vblock_is_odd = (y / tile_height_) % 2;
    const int hblock = x / tile_width_;

    if (vblock_is_odd) {
      *matrix_x = (x % tile_width_) + (2 * tile_width_ * hblock) + tile_width_;
    } else {
      // even tiles have reverse x-order
      *matrix_x = -((x % tile_width_) - tile_width_ + 1) + (2 * tile_width_ * hblock);
    }
    *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
  }
};

/*
 * Here is where the registration happens.
 * If you add an instance of the mapper here, it will automatically be
 * made available in the --led-multiplexing commandline option.
 */
static MuxMapperList *CreateMultiplexMapperList() {
  MuxMapperList *result = new MuxMapperList();

  // Here, register all multiplex mappers from above.
  result->push_back(new StripeMultiplexMapper());
  result->push_back(new CheckeredMultiplexMapper());
  result->push_back(new SpiralMultiplexMapper());
  result->push_back(new ZStripeMultiplexMapper("ZStripe", 0, 8));
  result->push_back(new ZStripeMultiplexMapper("ZnMirrorZStripe", 4, 4));
  result->push_back(new CoremanMapper());
  result->push_back(new Kaler2ScanMapper());
  result->push_back(new ZStripeMultiplexMapper("ZStripeUneven", 8, 0));
  result->push_back(new P10MapperZ());
  result->push_back(new QiangLiQ8());
  result->push_back(new InversedZStripe());
  result->push_back(new P10Outdoor1R1G1BMultiplexMapper1());
  result->push_back(new P10Outdoor1R1G1BMultiplexMapper2());
  result->push_back(new P10Outdoor1R1G1BMultiplexMapper3());
  result->push_back(new P10CoremanMapper());
  result->push_back(new P8Outdoor1R1G1BMultiplexMapper());
  result->push_back(new FlippedStripeMultiplexMapper());
  result->push_back(new P10Outdoor32x16HalfScanMapper());
  result->push_back(new P10Outdoor32x16QuarterScanMapper());
  result->push_back(new P3Outdoor64x64MultiplexMapper());
  result->push_back(new DoubleZMultiplexMapper());
  result->push_back(new P4Outdoor80x40Mapper());
  return result;
}

const MuxMapperList &GetRegisteredMultiplexMappers() {
  static const MuxMapperList *all_mappers = CreateMultiplexMapperList();
  return *all_mappers;
}
}  // namespace internal
}  // namespace rgb_matrix
