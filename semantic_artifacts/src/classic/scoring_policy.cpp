#include "classic/scoring_policy.hpp"

#include <variant>

namespace snake {
namespace scoring_policy {

namespace {

// One overload per arena event, so that a new event type needs a deliberate value here

std::vector<ScoreChange> effectsOfEvent(const FoodEaten& eaten) { return {ScoreChange{eaten.player, FOOD_POINTS}}; }

std::vector<ScoreChange> effectsOfEvent(const Bitten& bitten) {
  return {ScoreChange{bitten.victim, -COLLISION_PENALTY}};
}

std::vector<ScoreChange> effectsOfEvent(const SelfBitten& self_bitten) {
  return {ScoreChange{self_bitten.player, -COLLISION_PENALTY}};
}

std::vector<ScoreChange> effectsOfEvent(const MutualBite& mutual) {
  return {ScoreChange{mutual.first, -COLLISION_PENALTY}, ScoreChange{mutual.second, -COLLISION_PENALTY}};
}

}  // namespace

std::vector<ScoreChange> effectsOf(const classic_arena_authority::ArenaEvent& event) {
  return std::visit([](const auto& item) { return effectsOfEvent(item); }, event);
}

}  // namespace scoring_policy
}  // namespace snake
