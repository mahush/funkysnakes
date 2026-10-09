#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {
namespace scoring_policy {

namespace {

Scores applyEvent(Scores scores, const FoodCollected& collected) {
  if (collected.during) {
    Unbanked& unbanked = scores.unbanked.try_emplace(*collected.during, Unbanked{collected.player, 0}).first->second;
    unbanked.points += FOOD_POINTS;
  } else {
    scores.credited[collected.player] += FOOD_POINTS;
  }
  return scores;
}

Scores applyEvent(Scores scores, const OrderEnded& ended) {
  auto it = scores.unbanked.find(ended.order);
  if (it == scores.unbanked.end()) {
    return scores;
  }
  if (ended.reason == EndReason::COMPLETED) {
    scores.credited[it->second.player] += it->second.points;
  }
  scores.unbanked.erase(it);
  return scores;
}

Scores applyEvent(Scores scores, const Bitten& bitten) {
  scores.credited[bitten.victim] -= COLLISION_PENALTY;
  return scores;
}

Scores applyEvent(Scores scores, const SelfBitten& self_bitten) {
  scores.credited[self_bitten.player] -= COLLISION_PENALTY;
  return scores;
}

Scores applyEvent(Scores scores, const MutualBite& mutual) {
  scores.credited[mutual.first] -= COLLISION_PENALTY;
  scores.credited[mutual.second] -= COLLISION_PENALTY;
  return scores;
}

}  // namespace

Scores applyScoring(Scores scores, const ArenaEvents& events) {
  for (const ArenaEvent& event : events) {
    scores = std::visit([&scores](const auto& item) { return applyEvent(std::move(scores), item); }, event);
  }
  return scores;
}

Scores loseUnbanked(Scores scores) {
  scores.unbanked.clear();
  return scores;
}

}  // namespace scoring_policy
}  // namespace orbital
}  // namespace snake
