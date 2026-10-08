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
 * All access goes through free functions:
 * - Queries (declared here) provide free read access
 * - Mutations (declared in snake_model_transitions.hpp) protect invariants
 *
 * Note: player_id is NOT stored here - it's the key in GameState.snakes map
 */
class Snake {
 private:
  Point head_;
  std::vector<Point> tail_;
  Direction current_direction_;
  bool alive_{true};

  // Queries
  friend const Point& head(const Snake& s);
  friend const std::vector<Point>& tail(const Snake& s);
  friend Direction currentDirection(const Snake& s);
  friend bool alive(const Snake& s);
  friend std::size_t length(const Snake& s);

  // Mutations (declared in snake_model_transitions.hpp)
  friend Snake initial(Point head, Direction dir, int length);
  friend Snake move(Snake s, Direction dir, const Board& board);
  friend Snake grow(Snake s, Direction dir, const Board& board);
  friend std::tuple<Snake, std::vector<Point>> cutAt(Snake s, Point cut_point);
  friend Snake kill(Snake s);
};

// ============================================================================
// Queries
// ============================================================================

inline const Point& head(const Snake& s) { return s.head_; }
inline const std::vector<Point>& tail(const Snake& s) { return s.tail_; }
inline Direction currentDirection(const Snake& s) { return s.current_direction_; }
inline bool alive(const Snake& s) { return s.alive_; }
inline std::size_t length(const Snake& s) { return 1 + s.tail_.size(); }

}  // namespace snake_model
}  // namespace snake
