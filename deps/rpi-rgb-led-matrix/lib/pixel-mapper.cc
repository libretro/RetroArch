// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2018 Henner Zeller <h.zeller@acm.org>
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

#include "pixel-mapper.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

namespace rgb_matrix {
namespace {

class RotatePixelMapper : public PixelMapper {
public:
  RotatePixelMapper() : angle_(0) {}

  virtual const char *GetName() const { return "Rotate"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    if (param == NULL || strlen(param) == 0) {
      angle_ = 0;
      return true;
    }
    char *errpos;
    const int angle = strtol(param, &errpos, 10);
    if (*errpos != '\0') {
      fprintf(stderr, "Invalid rotate parameter '%s'\n", param);
      return false;
    }
    if (angle % 90 != 0) {
      fprintf(stderr, "Rotation needs to be multiple of 90 degrees\n");
      return false;
    }
    angle_ = (angle + 360) % 360;
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height)
    const {
    if (angle_ % 180 == 0) {
      *visible_width = matrix_width;
      *visible_height = matrix_height;
    } else {
      *visible_width = matrix_height;
      *visible_height = matrix_width;
    }
    return true;
  }

  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    switch (angle_) {
    case 0:
      *matrix_x = x;
      *matrix_y = y;
      break;
    case 90:
      *matrix_x = matrix_width - y - 1;
      *matrix_y = x;
      break;
    case 180:
      *matrix_x = matrix_width - x - 1;
      *matrix_y = matrix_height - y - 1;
      break;
    case 270:
      *matrix_x = y;
      *matrix_y = matrix_height - x - 1;
      break;
    }
  }

private:
  int angle_;
};

class MirrorPixelMapper : public PixelMapper {
public:
  MirrorPixelMapper() : horizontal_(true) {}

  virtual const char *GetName() const { return "Mirror"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    if (param == NULL || strlen(param) == 0) {
      horizontal_ = true;
      return true;
    }
    if (strlen(param) != 1) {
      fprintf(stderr, "Mirror parameter should be a single "
              "character:'V' or 'H'\n");
    }
    switch (*param) {
    case 'V':
    case 'v':
      horizontal_ = false;
      break;
    case 'H':
    case 'h':
      horizontal_ = true;
      break;
    default:
      fprintf(stderr, "Mirror parameter should be either 'V' or 'H'\n");
      return false;
    }
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height)
    const {
    *visible_height = matrix_height;
    *visible_width = matrix_width;
    return true;
  }

  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    if (horizontal_) {
      *matrix_x = matrix_width - 1 - x;
      *matrix_y = y;
    } else {
      *matrix_x = x;
      *matrix_y = matrix_height - 1 - y;
    }
  }

private:
  bool horizontal_;
};

// If we take a long chain of panels and arrange them in a U-shape, so
// that after half the panels we bend around and continue below. This way
// we have a panel that has double the height but only uses one chain.
// A single chain display with four 32x32 panels can then be arranged in this
// 64x64 display:
//    [<][<][<][<] }- Raspberry Pi connector
//
// can be arranged in this U-shape
//    [<][<] }----- Raspberry Pi connector
//    [>][>]
//
// This works for more than one chain as well. Here an arrangement with
// two chains with 8 panels each
//   [<][<][<][<]  }-- Pi connector #1
//   [>][>][>][>]
//   [<][<][<][<]  }--- Pi connector #2
//   [>][>][>][>]
class UArrangementMapper : public PixelMapper {
public:
  UArrangementMapper() : parallel_(1) {}

  virtual const char *GetName() const { return "U-mapper"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    if (chain < 2) {  // technically, a chain of 2 would work, but somewhat pointless
      fprintf(stderr, "U-mapper: need at least --led-chain=4 for useful folding\n");
      return false;
    }
    if (chain % 2 != 0) {
      fprintf(stderr, "U-mapper: Chain (--led-chain) needs to be divisible by two\n");
      return false;
    }
    parallel_ = parallel;
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height)
    const {
    *visible_width = (matrix_width / 64) * 32;   // Div at 32px boundary
    *visible_height = 2 * matrix_height;
    if (matrix_height % parallel_ != 0) {
      fprintf(stderr, "%s For parallel=%d we would expect the height=%d "
              "to be divisible by %d ??\n",
              GetName(), parallel_, matrix_height, parallel_);
      return false;
    }
    return true;
  }

  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    const int panel_height = matrix_height / parallel_;
    const int visible_width = (matrix_width / 64) * 32;
    const int slab_height = 2 * panel_height;   // one folded u-shape
    const int base_y = (y / slab_height) * panel_height;
    y %= slab_height;
    if (y < panel_height) {
      x += matrix_width / 2;
    } else {
      x = visible_width - x - 1;
      y = slab_height - y - 1;
    }
    *matrix_x = x;
    *matrix_y = base_y + y;
  }

private:
  int parallel_;
};



class VerticalMapper : public PixelMapper {
public:
  VerticalMapper() {}

  virtual const char *GetName() const { return "V-mapper"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    chain_ = chain;
    parallel_ = parallel;
    // optional argument :Z allow for every other panel to be flipped
    // upside down so that cabling can be shorter:
    // [ O < I ]   without Z       [ O < I  ]
    //   ,---^      <----                ^
    // [ O < I ]                   [ I > O  ]
    //   ,---^            with Z     ^
    // [ O < I ]            --->   [ O < I  ]
    z_ = (param && strcasecmp(param, "Z") == 0);
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height)
    const {
    *visible_width = matrix_width * parallel_ / chain_;
    *visible_height = matrix_height * chain_ / parallel_;
#if 0
     fprintf(stderr, "%s: C:%d P:%d. Turning W:%d H:%d Physical "
	     "into W:%d H:%d Virtual\n",
             GetName(), chain_, parallel_,
	     *visible_width, *visible_height, matrix_width, matrix_height);
#endif
    return true;
  }

  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    const int panel_width  = matrix_width  / chain_;
    const int panel_height = matrix_height / parallel_;
    // because the panel you plug into ends up being the "bottom" panel and coordinates
    // start from the top panel, and you typically don't wire the bottom panel (first in
    // the chain) upside down, whether each panel gets swapped depends on this.
    // Without this, if you wire for 4 panels high and add a 5h panel, without this
    // code everything would get reversed and you'd have to re-layout all the panels
    bool is_height_even_panels = ( matrix_width / panel_width) % 2;
    const int x_panel_start = y / panel_height * panel_width;
    const int y_panel_start = x / panel_width * panel_height;
    const int x_within_panel = x % panel_width;
    const int y_within_panel = y % panel_height;
    const bool needs_flipping = z_ && (is_height_even_panels - ((y / panel_height) % 2)) == 0;
    *matrix_x = x_panel_start + (needs_flipping
                                 ? panel_width - 1 - x_within_panel
                                 : x_within_panel);
    *matrix_y = y_panel_start + (needs_flipping
                                 ? panel_height - 1 - y_within_panel
                                 : y_within_panel);
  }

private:
  bool z_;
  int chain_;
  int parallel_;
};

// Takes a vertically stacked parallel strands and display rearranges it into a
// horizontal row of the same bands, optionally flipping every other band for
// symmetry.
//
// Parameters (optional):
//   - "Z": flip every other band (serpentine)
//   - "F": flip the right half by 180° (for windmill symmetry)
class StackToRowMapper : public PixelMapper {
public:
  StackToRowMapper() : z_(false), flip_right_(false), bands_(2) {}

  virtual const char *GetName() const { return "StackToRow"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    bands_ = parallel;
    z_ = false;
    flip_right_ = false;
    if (param && *param) {
      for (const char *p = param; *p; ++p) {
        const char c = *p;
        if (c == ',' || c == ' ')
          continue;
        switch (c) {
        case 'Z': case 'z': z_ = true; break;
        case 'F': case 'f': flip_right_ = true; break;
        default:
          fprintf(stderr, "StackToRow: unknown parameter '%c' (use Z and/or F)\n", c);
          return false;
        }
      }
    }
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height) const {
    *visible_width = matrix_width * bands_;
    *visible_height = matrix_height / bands_;
    return true;
  }

  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height, int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    const int band_width = matrix_width;
    const int band_height = matrix_height / bands_;
    const int band = x / band_width;
    const int x_in_band = x % band_width;
    const int y_in_band = y;

    int src_band = band;
    int src_x = x_in_band;
    int src_y = y_in_band;

    // Optionally flip every other band (serpentine)
    if (z_ && (band % 2 == 1)) {
      src_x = band_width - 1 - src_x;
      src_y = band_height - 1 - src_y;
    }

    // Optionally flip the right half by 180deg
    if (flip_right_ && band >= bands_ / 2) {
      src_x = band_width - 1 - src_x;
      src_y = band_height - 1 - src_y;
    }

    *matrix_x = src_x;
    *matrix_y = src_band * band_height + src_y;
  }

private:
  bool z_;
  bool flip_right_;
  int bands_;
};

class RemapMapper : public PixelMapper {
public:
  RemapMapper() {}

  virtual const char *GetName() const { return "Remap"; }

  virtual bool SetParameters(int chain, int parallel, const char *param) {
    chain_ = chain;
    parallel_ = parallel;
    const char* pos = param;
    if (!pos || !*pos) {
      fprintf(stderr, "Remap mapper can't be used without parameters\n");
      return false;
    }
    width_ = strtol(pos, (char**)&pos, 10);
    if (*pos++ != ',') {
      fprintf(stderr, "expected ',' after width parameter ('%s')\n", param);
      return false;
    }
    height_ = strtol(pos, (char**)&pos, 10);
    if (*pos++ != '|') {
      fprintf(stderr, "expected '|' after height parameter ('%s')\n", param);
      return false;
    }
    while(*pos) {
      MapTile tile;
      if ((pos = tile.ParseParam(pos)) == NULL)
        return false;
      map_.push_back(tile);
      if (*pos == '|') {
        ++pos;
        continue;
      }
      if (*pos) {
        fprintf(stderr, "Expected '|' tile separator after panel %d '%s'\n", (int)map_.size(), param);
        return false;
      }
    }
    if ((int)map_.size() != chain * parallel) {
      fprintf(stderr, "Remap list must have 1 entry for each panel (got %d, expected %d)\n", (int)map_.size(), chain * parallel);
      return false;
    }
    return true;
  }

  virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                              int *visible_width, int *visible_height)
    const {
    // need to go through mapping here to handle rectangular rotated panels
    const int panel_width  = matrix_width  / chain_;
    const int panel_height = matrix_height / parallel_;
    // check if all panels are at least partially inside visible area
    for (size_t i = 0; i < map_.size(); i++) {
      int x0 = -1, y0 = -1, x1 = -1, y1 = -1;
      // map opposite corners to get panel placement
      if (!map_[i].MapToVisible(panel_width, panel_height, 0, 0, &x0, &y0)) continue;
      if (!map_[i].MapToVisible(panel_width, panel_height, panel_width - 1, panel_height - 1, &x1, &y1)) continue;
      if (x1 < 0 || x0 >= width_ || y1 < 0 || y0 >= height_) {
        fprintf(stderr, "Panel %d is fully outside of visible area [%d,%d-%d,%d]", (int)i, x0, y0, x1, y1);
        return false;
      }
    }
    *visible_width = width_;
    *visible_height = height_;
#if 0
     fprintf(stderr, "%s: C:%d P:%d. Turning W:%d H:%d Physical "
	     "into W:%d H:%d Virtual\n",
             GetName(), chain_, parallel_,
	     *visible_width, *visible_height, matrix_width, matrix_height);
#endif
    return true;
  }

  virtual MappingType GetMappingType() const { return MatrixToVisible; }
  virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *matrix_x, int *matrix_y) const {
    return;
  }
  virtual bool MapMatrixToVisible(int matrix_width, int matrix_height,
                                  int x, int y,
                                  int *visible_x, int *visible_y) const {
    const int panel_width  = matrix_width  / chain_;
    const int panel_height = matrix_height / parallel_;

    const int panel_col = chain_ - x / panel_width - 1;   // count panels from Raspberry (first panel is top-right corner)
    const int panel_row = y / panel_height;

    const int x_within_panel = x % panel_width;
    const int y_within_panel = y % panel_height;

    const MapTile& tile = map_[panel_row * chain_ + panel_col];

    int new_x = -1, new_y = -1;
    if (tile.MapToVisible(panel_width, panel_height, x_within_panel, y_within_panel, &new_x, &new_y)) {
      if (new_x >= 0 && new_x  < width_
          && new_y >=0 && new_y < height_) {
        *visible_x = new_x;
        *visible_y = new_y;
        return true;
      }
    }
    return false;
  }

private:
  struct MapTile {
    const char* ParseParam(const char* param) {
      const char *pos = param;
      const char *end = strchrnul(param, '|');
      x_ = strtol(pos, (char**)&pos, 10);
      if (*pos++ != ',') {
        fprintf(stderr, "expected ',' after x parameter '%.*s'\n", (int)(end-param), param);
        return NULL;
      }
      y_ = strtol(pos, (char**)&pos, 10);
      switch (tolower(*pos++)) {
        case 'n': type_ = rot0; break;
        case 's': type_ = rot180; break;
        case 'e': type_ = rot270; break;
        case 'w': type_ = rot90; break;
        case 'x': type_ = discard; break;
        default:
          fprintf(stderr, "Expected orientation (one of [neswx]) after y parameter '%.*s'\n", (int)(end-param), param);
          return NULL;
      }
      return pos;
      return NULL;
    }
    bool MapToVisible(int tile_width, int tile_height, int x, int y, int* new_x, int* new_y) const {
      switch(type_) {
        case discard:
          return false;
        case rot0:
          *new_x = x_ + x;
          *new_y = y_ + y;
          return true;
        case rot90:             // mapping direction is opposite to Rotate pixel mapper
          *new_x = x_ + y;
          *new_y = y_ + tile_width - x - 1;
          return true;
        case rot180:
          *new_x = x_ + tile_width - x - 1;
          *new_y = y_ + tile_height - y - 1;
          return true;
        case rot270:
          *new_x = x_ + tile_height - y - 1;
          *new_y = y_ + x;
          return true;
      }
      return false;
    }
    enum { discard, rot0, rot90, rot180, rot270 } type_;
    int x_, y_;
  };
  int width_;
  int height_;
  std::vector<MapTile> map_;
  int chain_;
  int parallel_;
};


typedef std::map<std::string, PixelMapper*> MapperByName;
static void RegisterPixelMapperInternal(MapperByName *registry,
                                        PixelMapper *mapper) {
  assert(mapper != NULL);
  std::string lower_name;
  for (const char *n = mapper->GetName(); *n; n++)
    lower_name.append(1, tolower(*n));
  (*registry)[lower_name] = mapper;
}

static MapperByName *CreateMapperMap() {
  MapperByName *result = new MapperByName();

  // Register all the default PixelMappers here.
  RegisterPixelMapperInternal(result, new RotatePixelMapper());
  RegisterPixelMapperInternal(result, new UArrangementMapper());
  RegisterPixelMapperInternal(result, new VerticalMapper());
  RegisterPixelMapperInternal(result, new StackToRowMapper());
  RegisterPixelMapperInternal(result, new MirrorPixelMapper());
  RegisterPixelMapperInternal(result, new RemapMapper());
  return result;
}

static MapperByName *GetMapperMap() {
  static MapperByName *singleton_instance = CreateMapperMap();
  return singleton_instance;
}
}  // anonymous namespace

// Public API.
void RegisterPixelMapper(PixelMapper *mapper) {
  RegisterPixelMapperInternal(GetMapperMap(), mapper);
}

std::vector<std::string> GetAvailablePixelMappers() {
  std::vector<std::string> result;
  MapperByName *m = GetMapperMap();
  for (MapperByName::const_iterator it = m->begin(); it != m->end(); ++it) {
    result.push_back(it->second->GetName());
  }
  return result;
}

const PixelMapper *FindPixelMapper(const char *name,
                                   int chain, int parallel,
                                   const char *parameter) {
  std::string lower_name;
  for (const char *n = name; *n; n++) lower_name.append(1, tolower(*n));
  MapperByName::const_iterator found = GetMapperMap()->find(lower_name);
  if (found == GetMapperMap()->end()) {
    fprintf(stderr, "%s: no such mapper\n", name);
    return NULL;
  }
  PixelMapper *mapper = found->second;
  if (mapper == NULL) return NULL;  // should not happen.
  if (!mapper->SetParameters(chain, parallel, parameter))
    return NULL;   // Got parameter, but couldn't deal with it.
  return mapper;
}
}  // namespace rgb_matrix
