#pragma once

#include <variant>
#include <vector>

#include "domain/orbital/identities.hpp"
#include "domain/orbital/order.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace transmission_participation_policy {

/**
 * Transmission Participation Policy - decides whether a waiting transmission takes part in a step, also a late one
 *
 * Platform Policy: a transmission takes part in the step it is scheduled for (Transmission Schedule Policy) and waits
 * until then. Where transmissions travel over a link, one can arrive after that step has run. It then still takes
 * part, in the first step that has not run yet, unless a newer order of its player is due for that step as well. Of
 * several due orders of a player only the newest takes part, so a late order never delays a newer one.
 */

/**
 * @brief The transmission is not due yet
 */
struct Waits {
  bool operator==(const Waits& /*other*/) const noexcept { return true; }
};

/**
 * @brief The transmission takes part in the step, which is its scheduled one
 */
struct TakesPart {
  bool operator==(const TakesPart& /*other*/) const noexcept { return true; }
};

/**
 * @brief The transmission takes part in the step, after its scheduled one
 */
struct TakesPartLate {
  bool operator==(const TakesPartLate& /*other*/) const noexcept { return true; }
};

/**
 * @brief The transmission never takes part, because a newer order of its player is due as well
 */
struct Superseded {
  OrderNumber by;  // The player's newest due order

  bool operator==(const Superseded& other) const noexcept { return by == other.by; }
};

using Participation = std::variant<Waits, TakesPart, TakesPartLate, Superseded>;

/**
 * @brief What becomes of a waiting transmission in a step that is about to run
 *
 * It is due from its scheduled step on. Of a player's orders the one with the higher number is the newer.
 *
 * @param candidate The transmission in question, one of waiting
 * @param waiting Every waiting transmission of the round, of all players, in any order
 * @param step Step about to run
 * @return Waits before the scheduled step; Superseded if a newer order of the same player among waiting is due as
 *         well; otherwise TakesPart in the scheduled step and TakesPartLate after it
 */
Participation participationOf(const OrderTransmitted& candidate,
                              const std::vector<OrderTransmitted>& waiting,
                              StepId step);

}  // namespace transmission_participation_policy
}  // namespace platform
}  // namespace orbital
}  // namespace snake
