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
 *
 * A board is always created with its size; there is no default board.
 */
struct Board {
  constexpr Board(int board_width, int board_height) : width{board_width}, height{board_height} {}

  int width;
  int height;
};

}  // namespace snake
