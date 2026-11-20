#pragma once

#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace platform {

/**
 * Order facts a player's device forwards to the host
 *
 * Each player's Order Authority runs on the player's device. It publishes OrderSubmitted and OrderTransmitted;
 * the device forwards each unchanged, together with the Order it names, so the host learns the target as well.
 * The host receives these facts; it does not establish them.
 */

/**
 * @brief A player's Order Authority selected a submission
 */
struct ForwardedSubmission {
  Order order;
  OrderSubmitted submitted;

  bool operator==(const ForwardedSubmission& other) const noexcept {
    return order == other.order && submitted == other.submitted;
  }
};

/**
 * @brief A player's Order Authority transmitted its selected order
 */
struct ForwardedTransmission {
  Order order;
  OrderTransmitted transmitted;

  bool operator==(const ForwardedTransmission& other) const noexcept {
    return order == other.order && transmitted == other.transmitted;
  }
};

}  // namespace platform
}  // namespace orbital
}  // namespace snake
