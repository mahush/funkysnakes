#pragma once

// Mechanics of the Orbital Game Authority: plumbing that makes no gameplay decision.
// Private to the Authority's implementation; scoring triggers, settlement and conclusion stay in game_authority.cpp.

#include <utility>
#include <variant>

#include "orbital/game_authority.hpp"
#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {
namespace game_authority {
namespace mechanics {

// ============================================================================
// Point accumulation - adds what a point effect names
// ============================================================================

inline Scores applyEffect(Scores scores, const scoring_policy::Bank& bank) {
  scores.credited[bank.player] += bank.points;
  return scores;
}

inline Scores applyEffect(Scores scores, const scoring_policy::Hold& hold) {
  scores.unbanked.try_emplace(hold.order, Unbanked{hold.player, 0}).first->second.points += hold.points;
  return scores;
}

inline Scores applyEffect(Scores scores, const scoring_policy::PointEffect& effect) {
  return std::visit([&scores](const auto& item) { return applyEffect(std::move(scores), item); }, effect);
}

}  // namespace mechanics
}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
