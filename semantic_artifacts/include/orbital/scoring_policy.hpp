#pragma once

#include <map>

#include "common/game_types.hpp"
#include "orbital/arena_events.hpp"

namespace snake {
namespace orbital {
namespace scoring_policy {

/**
 * Orbital Scoring Policy - decides what arena events are worth and when points count
 *
 * Stateless decision. Point values follow classic snake; the timing of when they count is
 * Orbital's own rule:
 * - FoodCollected during an order: +10 unbanked for that order
 * - FoodCollected without an order: +10 credited at once
 * - Bitten: the victim loses 10 credited at once, the biter gets nothing
 * - SelfBitten: the snake loses 10 credited at once
 * - MutualBite: both snakes lose 10 credited at once
 * - OrderEnded COMPLETED: the order's unbanked points are banked (credited)
 * - OrderEnded INTERRUPTED, ELIMINATED or ROUND_ENDED: the order's unbanked points are lost
 *
 * Submitting or superseding a pending order is no arena event and changes no points. A
 * nonlethal bite leaves an order's unbanked points intact.
 */

constexpr int FOOD_POINTS = 10;
constexpr int COLLISION_PENALTY = 10;

/**
 * @brief Unbanked points of one executing order
 */
struct Unbanked {
  PlayerId player;
  int points;

  bool operator==(const Unbanked& other) const noexcept { return player == other.player && points == other.points; }
};

/**
 * @brief Credited points per player and unbanked points per executing order
 */
struct Scores {
  PerPlayerScores credited;
  std::map<OrderId, Unbanked> unbanked;
};

/**
 * @brief Apply the point changes of one step's events, in order
 *
 * Applying in order matters: food collected during an order is unbanked first and then banked
 * or lost when that order ends later in the same step.
 *
 * @param scores Scores before the events (by value)
 * @param events Arena events of one step
 * @return Scores after the events
 */
Scores applyScoring(Scores scores, const ArenaEvents& events);

/**
 * @brief Lose all unbanked points, because the round concluded
 *
 * @param scores Scores at conclusion (by value)
 * @return Scores with only credited points
 */
Scores loseUnbanked(Scores scores);

}  // namespace scoring_policy
}  // namespace orbital
}  // namespace snake
