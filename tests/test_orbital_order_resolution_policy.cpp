// Tests for the Order Resolution Policy: what the Arena Authority's report about an order means for its status.

#include <gtest/gtest.h>

#include "orbital/order_resolution_policy.hpp"

namespace snake {
namespace orbital {

using order_authority::Status;

/// @section order_resolution_policy::statusAfterActivation

/// @brief What the Arena Authority actually did wins: an activated order executes
TEST(OrderResolutionPolicy, ExecutesActivatedOrder) {
  // When: an activation is interpreted
  Status status = order_resolution_policy::statusAfterActivation();

  // Then: the order executes
  EXPECT_EQ(status, Status::EXECUTING);
}

/// @section order_resolution_policy::statusAfterRejection

/// @brief A refusal resolves a transmitted order as rejected
TEST(OrderResolutionPolicy, RejectsTransmittedOrder) {
  // When: a refusal of a transmitted order is interpreted
  std::optional<Status> status = order_resolution_policy::statusAfterRejection(Status::TRANSMITTED);

  // Then: the order is rejected
  EXPECT_EQ(status, Status::REJECTED);
}

/// @brief A refusal of an order that is not transmitted changes nothing, since nothing happened to it in the world
TEST(OrderResolutionPolicy, KeepsStatusOfRefusedOrderNotTransmitted) {
  // Given: orders that are not transmitted
  for (Status current : {Status::SELECTED, Status::EXECUTING, Status::COMPLETED, Status::CANCELLED}) {
    // When: a refusal of one is interpreted
    std::optional<Status> status = order_resolution_policy::statusAfterRejection(current);

    // Then: it keeps its status
    EXPECT_FALSE(status.has_value());
  }
}

/// @section order_resolution_policy::statusAfterEnding

/// @brief Each reason an executing order ended gives its own final status
TEST(OrderResolutionPolicy, GivesEndedOrderFinalStatusByReason) {
  // When: endings are interpreted
  // Then: each reason maps to its final status
  EXPECT_EQ(order_resolution_policy::statusAfterEnding(EndReason::COMPLETED), Status::COMPLETED);
  EXPECT_EQ(order_resolution_policy::statusAfterEnding(EndReason::INTERRUPTED), Status::INTERRUPTED);
  EXPECT_EQ(order_resolution_policy::statusAfterEnding(EndReason::ELIMINATED), Status::FAILED);
  EXPECT_EQ(order_resolution_policy::statusAfterEnding(EndReason::ROUND_ENDED), Status::ENDED_BY_ROUND);
}

}  // namespace orbital
}  // namespace snake
