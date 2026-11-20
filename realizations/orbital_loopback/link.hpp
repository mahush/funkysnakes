#pragma once

#include <chrono>
#include <vector>

#include "orbital/arena_events.hpp"
#include "orbital/forwarded_order_facts.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/identities.hpp"
#include "orbital/order_replica_authority.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Messages between the host and the players' devices of the distributed Orbital realization
 *
 * Platform code without meaning: every message carries an owner's fact or view unchanged. The uplink forwards
 * what a player's Order Authority published; the downlink carries the round's start data, the host's step
 * reports and conclusion, and the views of the boundary.
 */

/**
 * @brief The host started a round
 */
struct RoundStarted {
  RoundId round;
  std::chrono::milliseconds t0;             // Host game clock at the start of the round
  std::chrono::milliseconds step_interval;  // Interval between arena steps (Pacing Policy)
  std::vector<StepId> schedule;             // Every transmission opportunity of the game, as data
};

/**
 * @brief What a player's device sends to the host
 */
class PhoneUplink {
 public:
  virtual ~PhoneUplink() = default;
  virtual void forward(const platform::ForwardedSubmission& submission) = 0;
  virtual void forward(const platform::ForwardedTransmission& transmission) = 0;
};

/**
 * @brief What the host reports, to the devices, the shared screen and the record
 */
class HostOutbound {
 public:
  virtual ~HostOutbound() = default;
  virtual void roundStarted(const RoundStarted& started) = 0;
  virtual void stepReported(const game_boundary::StepReport& step) = 0;
  virtual void ordersEndedWithRound(const ArenaEvents& events) = 0;
  virtual void concluded(const game_authority::GameConcluded& concluded) = 0;
  // A forwarded submission or transmission the host took in while the game was running
  virtual void submissionReceived(const platform::ForwardedSubmission& submission) = 0;
  virtual void transmissionReceived(const platform::ForwardedTransmission& transmission,
                                    const std::vector<game_authority::HandoverGranted>& handovers) = 0;
  // A platform deviation: the transmission took part later than the Transmission Schedule Policy says
  virtual void lateTransmission(const platform::order_replica_authority::LateTransmission& late) = 0;
  virtual void viewsUpdated(const game_boundary::Views& views) = 0;
};

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
