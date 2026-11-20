#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "common/players.hpp"
#include "orbital/admission_policy.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/order_authority.hpp"
#include "orbital_loopback/link.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Phone composition - one player's device: the player's Order Authority, composed as in the Domain Application
 *
 * Platform code without meaning. The device runs the player's Order Authority with its rules unchanged:
 * - A submission is admitted as the Admission Policy decides for the game phase the host reported, classified
 *   into the step of the device's aligned host clock, and selected; its OrderSubmitted is forwarded.
 * - At each opportunity of the schedule the host sent, fired by the device's own timer, the selection is
 *   transmitted; its OrderTransmitted is forwarded.
 * - The host's step reports are observed once and in order; at the conclusion the remaining orders are cancelled.
 * The only time→step cut is the submission's: a submission between step k and step k+1 belongs to step k.
 */
class PhoneComposition {
 public:
  /**
   * @param player The player this device belongs to
   * @param uplink Where the device forwards its Order Authority's facts; must outlive the composition
   */
  PhoneComposition(PlayerId player, PhoneUplink& uplink);

  // Downlink from the host
  void roundStarted(const RoundStarted& started);
  void stepReported(const game_boundary::StepReport& step);
  void ordersEndedWithRound(const ArenaEvents& events);
  void concluded(const game_authority::GameConcluded& concluded);

  /**
   * @brief The player submits a target
   *
   * @param target Board position
   * @param host_time The device's estimate of the host game clock when the player submitted
   * @return The outcome, as in the Domain Application
   */
  game_boundary::SubmissionOutcome submit(Point target, std::chrono::milliseconds host_time);

  /**
   * @brief The device's timer fired the opportunity of a step of the schedule
   */
  void fireOpportunity(StepId at);

  const PlayerId& player() const { return player_; }
  const std::optional<RoundStarted>& round() const { return round_; }
  admission_policy::GamePhase phase() const { return phase_; }
  std::optional<order_authority::View> orders() const;

 private:
  PlayerId player_;
  PhoneUplink& uplink_;
  admission_policy::GamePhase phase_{admission_policy::GamePhase::NOT_STARTED};
  std::optional<RoundStarted> round_;
  std::optional<order_authority::State> orders_;
};

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
