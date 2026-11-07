#pragma once

#include <optional>

#include "orbital/game_boundary.hpp"

namespace snake {
namespace orbital {
namespace admission_policy {

using game_boundary::StartRejectionReason;
using game_boundary::SubmissionRejectionReason;

/**
 * Admission Policy - decides which players' interactions a game admits in each of its phases
 *
 * Stateless decision: one Domain Application plays one game, from its start to its conclusion.
 * - Start: admitted only before a game has started; a new game needs a new Domain Application
 * - Submit: admitted only while the game is running; before the start and after the conclusion it
 *   is rejected, each with its own reason
 *
 * What an admitted interaction does is decided by the Authorities it reaches. Whether the game is
 * running or concluded is the Game Authority's.
 */

/**
 * @brief Phase of the game a Domain Application plays
 */
enum class GamePhase {
  NOT_STARTED,  // No game has started yet
  RUNNING,      // The game has started and not concluded (Game Authority)
  CONCLUDED     // The game has concluded (Game Authority)
};

/**
 * @brief Why a start is refused in the given phase
 *
 * @param phase Phase of the game
 * @return Empty before a game has started; ALREADY_STARTED once one has
 */
constexpr std::optional<StartRejectionReason> startRejectionIn(GamePhase phase) {
  if (phase == GamePhase::NOT_STARTED) {
    return std::nullopt;
  }
  return StartRejectionReason::ALREADY_STARTED;
}

/**
 * @brief Why a submission is refused in the given phase
 *
 * @param phase Phase of the game
 * @return Empty while the game is running; NOT_STARTED before the start; ROUND_OVER after the conclusion
 */
constexpr std::optional<SubmissionRejectionReason> submissionRejectionIn(GamePhase phase) {
  switch (phase) {
    case GamePhase::NOT_STARTED:
      return SubmissionRejectionReason::NOT_STARTED;
    case GamePhase::RUNNING:
      return std::nullopt;
    case GamePhase::CONCLUDED:
      return SubmissionRejectionReason::ROUND_OVER;
  }
  return SubmissionRejectionReason::ROUND_OVER;
}

}  // namespace admission_policy
}  // namespace orbital
}  // namespace snake
