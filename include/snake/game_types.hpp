#ifndef SNAKE_GAME_TYPES_HPP
#define SNAKE_GAME_TYPES_HPP

#include <map>
#include <string>
#include <vector>

namespace snake {

// Forward declare PlayerId from control_messages
using PlayerId = std::string;

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

/**
 * @brief Collision handling mode
 *
 * Determines what happens when snakes collide.
 */
enum class CollisionMode {
  BITE_REMOVE_TAIL,  // Cut tail is simply removed
  BITE_DROP_FOOD     // Cut tail segments become food items
};

// Type aliases for per-player game data
using PerPlayerScores = std::map<PlayerId, int>;
using PerPlayerDirection = std::map<PlayerId, Direction>;
using PerPlayerAliveStates = std::map<PlayerId, bool>;

// Type alias for food items
using FoodItems = std::vector<Point>;

}  // namespace snake

#endif  // SNAKE_GAME_TYPES_HPP
