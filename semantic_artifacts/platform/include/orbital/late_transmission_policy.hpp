#pragma once

#include "orbital/identities.hpp"
#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace late_transmission_policy {

/**
 * Late Transmission Policy - decides which step a transmission takes part in when it reaches the host late
 *
 * Platform Policy of the distributed realization, in which each player's Order Authority transmits on the
 * player's device and the host runs the Arena. A transmission takes part in the step the Transmission Schedule
 * Policy names for it. If that step already ran on the host when the transmission arrives, it takes part in the
 * first step not yet run instead; the transmission keeps its own step, and the realization logs the difference
 * as a platform deviation, which the canonical game never shows.
 *
 * The alternative, rejecting a late order, would need a new rejection reason in the Order vocabulary, and the
 * player would lose an order through no fault of their own.
 */

/**
 * @brief Where a transmission takes part
 */
struct Placement {
  StepId scheduled_step;  // First step after the transmission (Transmission Schedule Policy)
  StepId first_step;      // First step it actually takes part in

  constexpr bool isLate() const noexcept { return first_step != scheduled_step; }
};

/**
 * @brief Place a transmission that arrives before a given step runs
 *
 * @param transmitted_in Step whose opportunity sent the order
 * @param first_unprocessed_step First step that has not run yet when the transmission arrives
 * @return The scheduled step if it has not run yet; otherwise first_unprocessed_step, which makes it late
 */
constexpr Placement placementOf(StepId transmitted_in, StepId first_unprocessed_step) {
  const StepId scheduled = transmission_schedule_policy::firstStepAfterTransmission(transmitted_in);
  return Placement{scheduled, scheduled < first_unprocessed_step ? first_unprocessed_step : scheduled};
}

}  // namespace late_transmission_policy
}  // namespace platform
}  // namespace orbital
}  // namespace snake
