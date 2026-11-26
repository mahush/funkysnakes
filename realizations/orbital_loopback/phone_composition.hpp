#pragma once

#include <chrono>
#include <cstddef>
#include <optional>

#include "domain/common/game_primitives.hpp"
#include "domain/common/players.hpp"
#include "domain/orbital/admission_policy.hpp"
#include "domain/orbital/game_authority.hpp"
#include "domain/orbital/game_boundary.hpp"
#include "domain/orbital/order_selection_authority.hpp"
#include "orbital_loopback/placement_link.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Phone composition - one player's device: the player's Order Selection Authority, whose facts it routes as the Domain
 * Application does, with a composition of its own
 *
 * Platform code without meaning, free of clocks and transport: whoever runs the device supplies the times, on the
 * host's game clock, and delivers the messages.
 * - A submission is admitted as the Admission Policy decides for the phase the host reported, gets the step its time
 *   falls in (Pacing Policy) and becomes the player's selection; its OrderSubmitted is forwarded.
 * - At each opportunity of the Transmission Schedule Policy the selection is transmitted; its OrderTransmitted is
 *   forwarded. An opportunity takes place when its time is reached, so before a submission at or after that time.
 * - At the conclusion the selection is cancelled, and no opportunity takes place any more.
 */
class PhoneComposition : public placement_link::HostToPhone {
 public:
  /**
   * @param player The player this device belongs to
   * @param host Where the device forwards its Order Selection Authority's facts; must outlive the composition
   */
  PhoneComposition(PlayerId player, placement_link::PhoneToHost& host);

  /**
   * @brief The host started the game; only the first start counts, a device plays one game
   */
  void gameStarted(const placement_link::GameStarted& started) override;

  /**
   * @brief The host concluded the game; from now on no opportunity takes place and no submission is admitted
   */
  void concluded(const game_authority::GameConcluded& concluded) override;

  /**
   * @brief Transmit at every opportunity whose time has come by host_time and that has not taken place yet
   *
   * Each opportunity transmits with its own step, in step order: normally one, several if the device was held up.
   * Calling it again for the same or an earlier time does nothing.
   *
   * @param host_time The device's reading of the host game clock
   */
  void transmitAtDueOpportunities(std::chrono::milliseconds host_time);

  /**
   * @brief The player submits a target
   *
   * Transmits at the opportunities due by host_time first, so the submission comes after them.
   *
   * @param target Board position
   * @param host_time The device's reading of the host game clock when the player submitted
   * @return The outcome, as in the Domain Application
   */
  game_boundary::SubmissionOutcome submit(Point target, std::chrono::milliseconds host_time);

 private:
  PlayerId player_;
  placement_link::PhoneToHost& host_;
  admission_policy::GamePhase phase_{admission_policy::GamePhase::NOT_STARTED};
  std::optional<placement_link::GameStarted> game_;
  std::optional<order_selection_authority::State> selections_;
  std::size_t next_opportunity_{0};  // Index in the game's opportunities of the next one to take place
};

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
