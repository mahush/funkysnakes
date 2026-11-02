#pragma once

#include "common/game_primitives.hpp"

namespace snake {

/**
 * @brief A snake at the start
 */
struct SnakeSetup {
  Point head;
  Direction heading;
  int length;  // Segments, head included
};

}  // namespace snake
