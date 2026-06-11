#pragma once

#include <tuple>
#include <vector>

#include "snake/snake_model.hpp"

namespace snake {
namespace snake_model {

/**
 * @brief Create a snake with initial body extending backwards from head
 *
 * @param head Starting head position
 * @param dir Initial movement direction
 * @param length Total snake length (head + tail)
 * @return Newly constructed Snake
 */
Snake initial(Point head, Direction dir, int length);

/**
 * @brief Move snake one step in given direction (constant length)
 *
 * Head advances one cell (wrapped at board boundaries), tail tip removed.
 * No-op if dead.
 * If length >= 2 and dir is opposite to body-implied direction, dir is ignored
 * and the snake continues in its current direction.
 *
 * @param s Snake to move
 * @param dir Requested movement direction
 * @param board Board dimensions for wrapping
 * @return Snake after moving
 */
Snake move(Snake s, Direction dir, const Board& board);

/**
 * @brief Grow snake one step in given direction (length increases by one)
 *
 * Head advances one cell (wrapped at board boundaries), tail tip kept.
 * No-op if dead.
 * If length >= 2 and dir is opposite to body-implied direction, dir is ignored
 * and the snake continues in its current direction.
 *
 * @param s Snake to grow
 * @param dir Requested movement direction
 * @param board Board dimensions for wrapping
 * @return Snake after growing
 */
Snake grow(Snake s, Direction dir, const Board& board);

/**
 * @brief Cut snake tail at specified point
 *
 * Removes all tail segments from the cut point onwards and returns them.
 * Body stays connected after truncation.
 * If cut point is the head or not in tail, snake is unchanged and cut is empty.
 *
 * @param s Snake to cut
 * @param cut_point Point where to cut the tail
 * @return Tuple of (snake with tail cut, cut tail segments)
 */
std::tuple<Snake, std::vector<Point>> cutAt(Snake s, Point cut_point);

/**
 * @brief Kill a snake (alive -> dead, one-way transition)
 *
 * @param s Snake to kill
 * @return Dead snake
 */
Snake kill(Snake s);

/**
 * @brief Query where the head would be after one step in given direction
 *
 * Applies the same 180 degree rejection as move/grow.
 *
 * @param s Snake to query
 * @param dir Requested movement direction
 * @param board Board dimensions for wrapping
 * @return Next head position
 */
Point nextHead(const Snake& s, Direction dir, const Board& board);

}  // namespace snake_model
}  // namespace snake
