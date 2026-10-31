#pragma once

#include <vector>

#include "classic/classic_arena_events.hpp"
#include "common/players.hpp"

namespace snake {
namespace scoring_policy {

/**
 * Scoring Policy - decides what arena events are worth
 *
 * Stateless decision mapping one arena event to score changes; it owns no score state. Applying the
 * changes to a score is up to its user:
 * - FoodEaten:  the eater gains 10
 * - Bitten:     the victim loses 10, the biter gets nothing
 * - SelfBitten: the snake loses 10
 * - MutualBite: both snakes lose 10
 */

constexpr int FOOD_POINTS = 10;
constexpr int COLLISION_PENALTY = 10;

/**
 * @brief A change of one player's score; negative for a penalty
 */
struct ScoreChange {
  PlayerId player;
  int points;

  bool operator==(const ScoreChange& other) const noexcept { return player == other.player && points == other.points; }
};

/**
 * @brief Score changes of one arena event
 *
 * @param event Arena event
 * @return Score changes of the event, in order
 */
std::vector<ScoreChange> effectsOf(const classic_arena_authority::ArenaEvent& event);

}  // namespace scoring_policy
}  // namespace snake
