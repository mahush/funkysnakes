#pragma once

#include <tuple>
#include <vector>

#include "snake/game_primitives.hpp"

namespace snake {
namespace snake_model {

/**
 * @brief Snake with guaranteed head and protected body evolution
 *
 * Represents a snake that always has at least a head.
 * The tail can be empty (snake of length 1).
 * This makes illegal states (empty snake) unrepresentable.
 *
 * All mutations go through friend functions to protect invariants:
 * - Head moves exactly one cell per tick
 * - Body remains a connected chain
 * - Dead snakes cannot move or grow
 *
 * Note: player_id is NOT stored here - it's the key in GameState.snakes map
 */
class Snake {
 public:
  const Point& head() const { return head_; }
  const std::vector<Point>& tail() const { return tail_; }
  Direction currentDirection() const { return current_direction_; }
  bool alive() const { return alive_; }
  std::size_t length() const { return 1 + tail_.size(); }

  std::vector<Point> toBody() const {
    std::vector<Point> body;
    body.reserve(1 + tail_.size());
    body.push_back(head_);
    body.insert(body.end(), tail_.begin(), tail_.end());
    return body;
  }

 private:
  Point head_;
  std::vector<Point> tail_;
  Direction current_direction_;
  bool alive_{true};

  friend Snake initial(Point head, Direction dir, int length);
  friend Snake move(Snake s, const Board& board);
  friend Snake grow(Snake s, const Board& board);
  friend std::tuple<Snake, std::vector<Point>> cutAt(Snake s, Point cut_point);
  friend Snake kill(Snake s);
  friend Snake setDirection(Snake s, Direction dir);
};

/**
 * @brief Create a snake with initial body extending backwards from head
 *
 * Builds a connected chain of the specified length, extending in the
 * opposite direction of the initial movement direction.
 *
 * @param head Starting head position
 * @param dir Initial movement direction
 * @param length Total snake length (head + tail)
 * @return Newly constructed Snake
 */
Snake initial(Point head, Direction dir, int length);

/**
 * @brief Move snake one step in current direction (constant length)
 *
 * Head advances one cell (wrapped at board boundaries), tail tip removed.
 * No-op if dead.
 *
 * @param s Snake to move
 * @param board Board dimensions for wrapping
 * @return Snake after moving
 */
Snake move(Snake s, const Board& board);

/**
 * @brief Grow snake one step in current direction (length increases by one)
 *
 * Head advances one cell (wrapped at board boundaries), tail tip kept.
 * No-op if dead.
 *
 * @param s Snake to grow
 * @param board Board dimensions for wrapping
 * @return Snake after growing
 */
Snake grow(Snake s, const Board& board);

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
 * @brief Set movement direction
 *
 * @param s Snake to update
 * @param dir New direction
 * @return Snake with updated direction
 */
Snake setDirection(Snake s, Direction dir);

}  // namespace snake_model
}  // namespace snake
