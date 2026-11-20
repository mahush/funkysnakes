#pragma once

#include "common/start_layout.hpp"

namespace snake {
namespace board_setup_policy {

/**
 * Board Setup Policy - decides the board of the classic game and how the snakes start on it
 *
 * Stateless decisions on the board and the start layout, which must fit each other: every snake starts on the board,
 * apart from the others. It places nothing; the arena is constructed from the start layout.
 */

// Board of the classic game
constexpr Board BOARD{60, 20};

/**
 * @brief Start layout of the classic game
 *
 * Player A and Player B start side by side five rows apart, both heading right with seven segments, head included.
 *
 * @return The board and the start of Player A and Player B
 */
inline StartLayout startLayout() {
  constexpr Direction START_HEADING = Direction::RIGHT;
  constexpr int START_LENGTH = 7;
  return StartLayout{BOARD,
                     {{PLAYER_A, SnakeSetup{Point{6, 10}, START_HEADING, START_LENGTH}},
                      {PLAYER_B, SnakeSetup{Point{6, 15}, START_HEADING, START_LENGTH}}}};
}

}  // namespace board_setup_policy
}  // namespace snake
