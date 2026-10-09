#pragma once

#include <ostream>

#include "classic/game_primitives.hpp"

namespace snake {

// Readable gtest output for points
inline void PrintTo(const Point& p, std::ostream* os) { *os << "(" << p.x << "," << p.y << ")"; }

}  // namespace snake
