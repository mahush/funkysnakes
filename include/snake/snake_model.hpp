#ifndef SNAKE_SNAKE_MODEL_HPP
#define SNAKE_SNAKE_MODEL_HPP

#include <optional>
#include <vector>

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Snake with guaranteed head
 *
 * Represents a snake that always has at least a head.
 * The tail can be empty (snake of length 1).
 * This makes illegal states (empty snake) unrepresentable.
 */
struct Snake {
  Point head;
  std::vector<Point> tail;  // Can be empty

  /**
   * @brief Create snake from head and optional tail
   */
  static Snake create(Point head, std::vector<Point> tail = {}) { return Snake{head, tail}; }

  /**
   * @brief Convert from body representation (vector of points)
   *
   * @param body Full snake body (head at index 0)
   * @return Snake if body is non-empty, nullopt otherwise
   */
  static std::optional<Snake> fromBody(const std::vector<Point>& body) {
    if (body.empty()) {
      return std::nullopt;
    }
    return Snake{body[0], std::vector<Point>(body.begin() + 1, body.end())};
  }

  /**
   * @brief Convert to body representation (vector of points)
   *
   * @return Full snake body with head at index 0
   */
  std::vector<Point> toBody() const {
    std::vector<Point> body;
    body.reserve(1 + tail.size());
    body.push_back(head);
    body.insert(body.end(), tail.begin(), tail.end());
    return body;
  }

  /**
   * @brief Get total length of snake
   */
  size_t length() const { return 1 + tail.size(); }
};

/**
 * @brief Pair of snakes for collision detection
 *
 * Used for functional collision handling between two snakes.
 */
struct SnakePair {
  Snake a;
  Snake b;
};

/**
 * @brief Get the head position of a snake
 *
 * Always returns the head position since Snake always has a head.
 *
 * @param snake Snake
 * @return Head position
 */
Point getHead(const Snake& snake);

/**
 * @brief Cut tail of snake starting at a specific point
 *
 * Returns a new snake that includes all points up to (but excluding)
 * the cut point. The tail from the cut point onwards is removed.
 * If the cut point is the head or not found, returns the original snake.
 *
 * @param snake Original snake
 * @param hitPoint Point where to start cutting tail (excluded from result)
 * @return New snake with tail cut off, or original if cut point is head/not found
 */
Snake cutTailAt(const Snake& snake, const Point& hitPoint);

/**
 * @brief Check if first snake's head bites second snake's body
 *
 * Returns true if the first snake's head is contained anywhere in the
 * second snake's body (head or tail).
 *
 * @param first Snake that might be biting
 * @param second Snake that might be bitten
 * @return true if first bites second
 */
bool firstBitesSecond(const Snake& first, const Snake& second);

/**
 * @brief Apply bite rule to two snakes
 *
 * Pure functional implementation of collision detection:
 * - If snake A's head hits snake B's body, cut B at the hit point
 * - If snake B's head hits snake A's body, cut A at the hit point
 * - Otherwise, return snakes unchanged
 *
 * @param s Pair of snakes
 * @return New pair of snakes with bite rule applied
 */
SnakePair applyBiteRule(SnakePair s);

}  // namespace snake

#endif  // SNAKE_SNAKE_MODEL_HPP
