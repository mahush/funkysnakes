#pragma once

#include "common/game_primitives.hpp"

namespace snake {
namespace board_setup_policy {

/**
 * Board Setup Policy - decides the board of the classic game and how the snakes start on it
 *
 * Stateless decisions on the board and the start layout, which must fit each other: every snake starts on the board,
 * apart from the others. It places nothing; the arena is constructed from these values.
 */

// Board of the classic game
constexpr Board BOARD{60, 20};

// Start heads of Player A and Player B, side by side five rows apart
constexpr Point PLAYER_A_HEAD{6, 10};
constexpr Point PLAYER_B_HEAD{6, 15};

// Every snake starts heading right with this many segments, head included
constexpr Direction START_HEADING = Direction::RIGHT;
constexpr int START_LENGTH = 7;

}  // namespace board_setup_policy
}  // namespace snake
