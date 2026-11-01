#include "orbital/scoring_policy.hpp"

#include <algorithm>

namespace snake {
namespace orbital {
namespace scoring_policy {

namespace {

std::vector<PointEffect> effectsOfEvent(const FoodCollected& collected) {
  if (collected.during) {
    return {Hold{*collected.during, collected.player, FOOD_POINTS}};
  }
  return {Bank{collected.player, FOOD_POINTS_NO_ORDER}};
}

std::vector<PointEffect> effectsOfEvent(const OrderEnded& /*ended*/) { return {}; }

std::vector<PointEffect> effectsOfEvent(const Bitten& bitten) { return {Bank{bitten.victim, -COLLISION_PENALTY}}; }

std::vector<PointEffect> effectsOfEvent(const SelfBitten& self_bitten) {
  return {Bank{self_bitten.player, -COLLISION_PENALTY}};
}

std::vector<PointEffect> effectsOfEvent(const MutualBite& mutual) {
  return {Bank{mutual.first, -COLLISION_PENALTY}, Bank{mutual.second, -COLLISION_PENALTY}};
}

}  // namespace

std::vector<PointEffect> effectsOf(const ArenaEvent& event) {
  return std::visit([](const auto& item) { return effectsOfEvent(item); }, event);
}

Settlement settlementOf(EndReason reason) {
  switch (reason) {
    case EndReason::COMPLETED:
      return Settlement::BANK;
    case EndReason::INTERRUPTED:
    case EndReason::ELIMINATED:
    case EndReason::ROUND_ENDED:
      return Settlement::DROP;
  }
  return Settlement::DROP;
}

int handoverBonus(int handover) {
  int bonus = HANDOVER_BONUS_BASE;
  for (int i = 1; i < handover && bonus < HANDOVER_BONUS_CAP; ++i) {
    bonus *= 2;
  }
  return std::min(bonus, HANDOVER_BONUS_CAP);
}

}  // namespace scoring_policy
}  // namespace orbital
}  // namespace snake
