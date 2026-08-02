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

#include <vector>

#include "pixel-mapper.h"

namespace rgb_matrix {
namespace internal {

class MultiplexMapper : public PixelMapper {
public:
  // Function that edits the original rows and columns of the panels
  // provided by the user to the actual underlying mapping. This is called
  // before we do the actual set-up of the GPIO mapping as this influences
  // the hardware interface.
  // This is so that the user can provide the rows/columns they see.
  virtual void EditColsRows(int *cols, int *rows) const = 0;
};

// Returns a vector of the registered Multiplex mappers.
typedef std::vector<const MultiplexMapper*> MuxMapperList;
const MuxMapperList &GetRegisteredMultiplexMappers();

}  // namespace internal
}  // namespace rgb_matrix
