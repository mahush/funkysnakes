#pragma once

#include <vector>

#include "common/game_primitives.hpp"

namespace snake {
namespace orbital {
namespace pursuit_policy {

/**
 * Pursuit Policy - decides the route a snake takes towards an order's target position
 *
 * Stateless decision taken once, when an order activates. A route is the sequence of moves, one
 * direction per tick, that leads the head from its position to the target. The Arena keeps the
 * route of the executing order and takes one move per tick; it does not interpret the route.
 *
 * Current route shape: follow the current heading until aligned with the target, wrapping at the
 * board edge if the target lies behind (this avoids a reversal), then turn once towards the target
 * as seen on screen and go straight to it without wrapping. A target on the line ahead or behind is
 * reached by going straight.
 *
 * A route never starts with or contains a reversal of the previous move: the snake model would
 * ignore such a move for snakes of length two or more, and the snake would leave its route.
 */

/**
 * @brief Plan the route from a head position to a target position
 *
 * @param head Current head position
 * @param heading Current heading of the snake
 * @param target Target position of the order (on the board)
 * @param board Board dimensions (for wrapping)
 * @return Moves from head to target, one per tick; empty if the head is already at the target
 */
std::vector<Direction> planRoute(Point head, Direction heading, Point target, const Board& board);

}  // namespace pursuit_policy
}  // namespace orbital
}  // namespace snake
