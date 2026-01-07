#ifndef SNAKE_GAME_TYPES_HPP
#define SNAKE_GAME_TYPES_HPP

#include <deque>
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
 * @brief Snake with guaranteed head
 *
 * Represents a snake that always has at least a head.
 * The tail can be empty (snake of length 1).
 * This makes illegal states (empty snake) unrepresentable.
 *
 * Also includes snake state: direction and alive status.
 * Note: player_id is NOT stored here - it's the key in GameState.snakes map
 */
struct Snake {
  Point head;
  std::vector<Point> tail;  // Can be empty
  Direction current_direction;
  bool alive{true};

  /**
   * @brief Create snake from head and optional tail
   */
  static Snake create(Point head, std::vector<Point> tail = {}, Direction dir = Direction::RIGHT,
                      bool is_alive = true) {
    return Snake{head, tail, dir, is_alive};
  }

  /**
   * @brief Get total length of snake
   */
  size_t length() const { return 1 + tail.size(); }

  /**
   * @brief Convert to body representation (vector of points with head at index 0)
   *
   * This is mainly for compatibility/serialization purposes.
   */
  std::vector<Point> toBody() const {
    std::vector<Point> body;
    body.reserve(1 + tail.size());
    body.push_back(head);
    body.insert(body.end(), tail.begin(), tail.end());
    return body;
  }
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
using PerPlayerSnakes = std::map<PlayerId, Snake>;
using PerPlayerScores = std::map<PlayerId, int>;
using PerPlayerDirectionQueue = std::map<PlayerId, std::deque<Direction>>;
using PerPlayerDirection = std::map<PlayerId, Direction>;

// Type alias for food items
using FoodItems = std::vector<Point>;

}  // namespace snake

#endif  // SNAKE_GAME_TYPES_HPP
