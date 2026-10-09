#include "snake/scoring_policy.hpp"

#include <type_traits>

namespace snake {
namespace scoring_policy {

PerPlayerScores applyScoring(PerPlayerScores scores, const ArenaEvents& events) {
  for (const ArenaEvent& event : events) {
    std::visit(
        [&scores](const auto& e) {
          using TEvent = std::decay_t<decltype(e)>;
          if constexpr (std::is_same_v<TEvent, FoodEaten>) {
            scores[e.player] += FOOD_POINTS;
          } else if constexpr (std::is_same_v<TEvent, Bitten>) {
            scores[e.victim] -= COLLISION_PENALTY;
          } else if constexpr (std::is_same_v<TEvent, SelfBitten>) {
            scores[e.player] -= COLLISION_PENALTY;
          } else if constexpr (std::is_same_v<TEvent, MutualBite>) {
            scores[e.first] -= COLLISION_PENALTY;
            scores[e.second] -= COLLISION_PENALTY;
          }
        },
        event);
  }
  return scores;
}

}  // namespace scoring_policy
}  // namespace snake
