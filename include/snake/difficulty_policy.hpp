#pragma once

#include <algorithm>

namespace snake {
namespace difficulty_policy {

/**
 * Difficulty Policy - maps a level to the arena's step interval
 *
 * Stateless decision: the game speeds up with each level, down to a minimum interval.
 */

constexpr int BASE_INTERVAL_MS = 200;
constexpr int REDUCTION_PER_LEVEL_MS = 15;
constexpr int MIN_INTERVAL_MS = 50;

/**
 * @brief Step interval for a level
 *
 * @param level Current level (1 or higher)
 * @return Step interval in milliseconds: max(50, 200 - 15 * (level - 1))
 */
constexpr int stepIntervalMs(int level) {
  return std::max(MIN_INTERVAL_MS, BASE_INTERVAL_MS - ((level - 1) * REDUCTION_PER_LEVEL_MS));
}

}  // namespace difficulty_policy
}  // namespace snake
