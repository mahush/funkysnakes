#pragma once

#include <map>

#include "common/game_primitives.hpp"
#include "common/players.hpp"

namespace snake {

/**
 * @brief A snake at the start
 */
struct SnakeSetup {
  Point head;
  Direction heading;
  int length;  // Segments, head included
};

/**
 * @brief The board and how every snake starts on it
 */
struct StartLayout {
  Board board;
  std::map<PlayerId, SnakeSetup> snakes;
};

}  // namespace snake
