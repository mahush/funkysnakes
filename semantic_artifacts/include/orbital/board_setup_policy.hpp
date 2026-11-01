#pragma once

#include "common/game_primitives.hpp"

namespace snake {
namespace orbital {
namespace board_setup_policy {

/**
 * Orbital Board Setup Policy - decides the board an Orbital Orders round is played on and how the snakes start on it
 *
 * Stateless decisions on the board and the start layout, which must fit each other: every snake starts on the board,
 * apart from the others. It places nothing; the Arena constructs the round from these values.
 */

// Square board; it looks square in the terminal, where each cell is drawn two columns wide
constexpr Board BOARD{28, 28};

// Start heads of Player A and Player B, side by side five rows apart
constexpr Point PLAYER_A_HEAD{5, 11};
constexpr Point PLAYER_B_HEAD{5, 16};

// Every snake starts heading right with this many segments, head included
constexpr Direction START_HEADING = Direction::RIGHT;
constexpr int START_LENGTH = 7;

}  // namespace board_setup_policy
}  // namespace orbital
}  // namespace snake
