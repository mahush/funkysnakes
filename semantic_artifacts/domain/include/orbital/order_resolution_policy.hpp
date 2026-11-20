#pragma once

#include <optional>

#include "orbital/order.hpp"
#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {
namespace order_resolution_policy {

/**
 * Order Resolution Policy - decides what the Arena Authority's report about an order means for the order's status
 *
 * Stateless decision. Whoever keeps a record of the players' orders asks it when the Arena Authority reports an
 * activation outcome or an ending: the Order Authority, and a realization's record of orders it receives from
 * elsewhere, so the interpretation is stated once.
 *
 * - What the Arena Authority actually did wins over what was expected: an activated order executes, whatever its
 *   status was.
 * - A refusal resolves only a transmitted order; to any other order nothing happened in the world.
 * - An ending gives the executing order its final status by the reason it ended.
 */

/**
 * @brief Status of an order the Arena Authority activated
 *
 * @return EXECUTING, whatever the order's status was
 */
constexpr order_authority::Status statusAfterActivation() { return order_authority::Status::EXECUTING; }

/**
 * @brief Status of an order the Arena Authority refused to activate
 *
 * @param current The order's status when the refusal is observed
 * @return REJECTED for a transmitted order; nothing for any other, which keeps its status
 */
constexpr std::optional<order_authority::Status> statusAfterRejection(order_authority::Status current) {
  if (current != order_authority::Status::TRANSMITTED) {
    return std::nullopt;
  }
  return order_authority::Status::REJECTED;
}

/**
 * @brief Status of an executing order that stopped executing
 *
 * @param reason Why the Arena Authority ended it
 * @return COMPLETED, INTERRUPTED, FAILED (eliminated) or ENDED_BY_ROUND
 */
constexpr order_authority::Status statusAfterEnding(EndReason reason) {
  switch (reason) {
    case EndReason::COMPLETED:
      return order_authority::Status::COMPLETED;
    case EndReason::INTERRUPTED:
      return order_authority::Status::INTERRUPTED;
    case EndReason::ELIMINATED:
      return order_authority::Status::FAILED;
    case EndReason::ROUND_ENDED:
      return order_authority::Status::ENDED_BY_ROUND;
  }
  return order_authority::Status::FAILED;
}

}  // namespace order_resolution_policy
}  // namespace orbital
}  // namespace snake
