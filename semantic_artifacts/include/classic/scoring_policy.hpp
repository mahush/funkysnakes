#pragma once

#include "common/arena_events.hpp"
#include "common/game_types.hpp"

namespace snake {
namespace scoring_policy {

/**
 * Scoring Policy - decides what arena events are worth
 *
 * Stateless decision mapping the facts of an arena step to score changes:
 * - FoodEaten:  the eater gains 10
 * - Bitten:     the victim loses 10, the biter gets nothing
 * - SelfBitten: the snake loses 10
 * - MutualBite: both snakes lose 10
 */

constexpr int FOOD_POINTS = 10;
constexpr int COLLISION_PENALTY = 10;

/**
 * @brief Apply the score changes of a step's events
 *
 * @param scores Scores before the events (by value)
 * @param events Events of one arena step
 * @return Scores after the events
 */
PerPlayerScores applyScoring(PerPlayerScores scores, const ArenaEvents& events);

}  // namespace scoring_policy
}  // namespace snake
