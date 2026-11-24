#pragma once

#include "common/start_layout.hpp"

namespace snake {
namespace orbital {
namespace board_setup_policy {

/**
 * Orbital Board Setup Policy - decides the board an Orbital Orders round is played on and how the snakes start on it
 *
 * Stateless decisions on the board and the start layout, which must fit each other: every snake starts on the board,
 * apart from the others. It places nothing; the Arena Authority constructs the round from the start layout.
 */

// Square board; it looks square in the terminal, where each cell is drawn two columns wide
constexpr Board BOARD{28, 28};

/**
 * @brief Start layout of an Orbital Orders round
 *
 * Player A and Player B start side by side five rows apart, both heading right with seven segments, head included.
 *
 * @return The board and the start of Player A and Player B
 */
inline StartLayout startLayout() {
  constexpr Direction START_HEADING = Direction::RIGHT;
  constexpr int START_LENGTH = 7;
  return StartLayout{BOARD,
                     {{PLAYER_A, SnakeSetup{Point{6, 11}, START_HEADING, START_LENGTH}},
                      {PLAYER_B, SnakeSetup{Point{6, 16}, START_HEADING, START_LENGTH}}}};
}

}  // namespace board_setup_policy
}  // namespace orbital
}  // namespace snake
