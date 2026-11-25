#pragma once

// Mechanics of the Orbital Game Authority: plumbing that makes no gameplay decision.
// Private to the Authority's implementation; scoring triggers, settlement and conclusion stay in game_authority.cpp.

#include <utility>
#include <variant>

#include "domain/orbital/game_authority.hpp"
#include "domain/orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {
namespace game_authority {
namespace mechanics {

// ============================================================================
// Point accumulation - adds what a point effect names
// ============================================================================

inline Scores applyEffect(Scores scores, const scoring_policy::Bank& bank) {
  scores.banked[bank.player] += bank.points;
  return scores;
}

inline Scores applyEffect(Scores scores, const scoring_policy::Hold& hold) {
  scores.held[hold.player][hold.order] += hold.points;
  return scores;
}

inline Scores applyEffect(Scores scores, const scoring_policy::PointEffect& effect) {
  return std::visit([&scores](const auto& item) { return applyEffect(std::move(scores), item); }, effect);
}

}  // namespace mechanics
}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
