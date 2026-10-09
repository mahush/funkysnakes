#pragma once

namespace snake {

/**
 * @brief Direction enumeration
 */
enum class Direction { UP, DOWN, LEFT, RIGHT };

/**
 * @brief Point in 2D space
 *
 * Represents a position on the game board.
 */
struct Point {
  int x;
  int y;

  bool operator==(const Point& other) const noexcept { return x == other.x && y == other.y; }
};

/**
 * @brief Game board dimensions
 */
struct Board {
  int width{60};
  int height{20};
};

}  // namespace snake
