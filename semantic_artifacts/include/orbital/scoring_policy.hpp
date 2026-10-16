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
 * Stateless decision. Food and collision values follow classic snake, except that food without
 * an order is worth less: eating during an order risks its points, so it has to be worth more than
 * eating while driving straight. The timing of when points count is Orbital's own rule:
 * - FoodCollected during an order: +10 unbanked for that order
 * - FoodCollected without an order: +5 credited at once
 * - Bitten: the victim loses 10 credited at once, the biter gets nothing
 * - SelfBitten: the snake loses 10 credited at once
 * - MutualBite: both snakes lose 10 credited at once
 * - OrderEnded COMPLETED: the order's unbanked points are banked (credited)
 * - OrderEnded INTERRUPTED, ELIMINATED or ROUND_ENDED: the order's unbanked points are lost
 *
 * Submitting or superseding a pending order is no arena event and changes no points. A
 * nonlethal bite leaves an order's unbanked points intact.
 *
 * A completion that hands over to an already committed, still pending successor is worth a
 * handover bonus in addition; whether a completion qualifies is the Game Authority's chain. The
 * first handover of a chain is worth the base bonus, each consecutive one doubles it, up to a cap.
 * Base and cap are provisional tuning values.
 */

constexpr int FOOD_POINTS = 10;          // During an order, unbanked
constexpr int FOOD_POINTS_NO_ORDER = 5;  // Without an order, credited at once
constexpr int COLLISION_PENALTY = 10;
constexpr int HANDOVER_BONUS_BASE = 10;  // First handover of a chain
constexpr int HANDOVER_BONUS_CAP = 80;   // Doubling stops here

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
 * @brief Bonus for the given handover of a chain
 *
 * @param handover Position of the handover in its chain, starting at 1
 * @return Base bonus doubled for each earlier handover of the chain, at most the cap
 */
int handoverBonus(int handover);

/**
 * @brief Lose all unbanked points
 *
 * @param scores Scores (by value)
 * @return Scores with only credited points
 */
Scores loseUnbanked(Scores scores);

}  // namespace scoring_policy
}  // namespace orbital
}  // namespace snake
