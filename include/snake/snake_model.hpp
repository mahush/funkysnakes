#ifndef SNAKE_SNAKE_MODEL_HPP
#define SNAKE_SNAKE_MODEL_HPP

#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "snake/game_messages.hpp"

namespace snake {

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
 * @brief Result of cutting a snake's tail
 *
 * Contains both the truncated snake and the segments that were cut off.
 */
struct CutResult {
  Snake snake;                      // Truncated snake
  std::vector<Point> cut_segments;  // Segments that were removed
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
 * Returns both the truncated snake and the segments that were cut off.
 * The cut point and everything after it are removed from the snake and
 * returned as cut_segments. If the cut point is the head or not found,
 * returns the original snake with empty cut_segments.
 *
 * @param snake Original snake
 * @param hitPoint Point where to start cutting tail (excluded from result)
 * @return CutResult containing truncated snake and cut segments
 */
CutResult cutTailAt(const Snake& snake, const Point& hitPoint);

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
 * @brief Check if both snakes bite each other simultaneously
 *
 * Returns true if both snakes' heads hit each other's bodies at the same time.
 *
 * @param a First snake
 * @param b Second snake
 * @return true if both bite each other
 */
bool bothBiteEachOther(const Snake& a, const Snake& b);

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
