// Tests for the Late Transmission Policy: which step a transmission takes part in when it reaches the host late.

#include <gtest/gtest.h>

#include "orbital/late_transmission_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {

/// @section late_transmission_policy::placementOf

/// @brief A transmission arriving before its scheduled step runs takes part there and is not late
TEST(LateTransmissionPolicy, KeepsScheduledStepOfTimelyTransmission) {
  // Given: a transmission at step 5, arriving before step 6 has run
  // When: it is placed
  late_transmission_policy::Placement placement = late_transmission_policy::placementOf(StepId{5}, StepId{6});

  // Then: it takes part in step 6, the step after its transmission
  EXPECT_EQ(placement.scheduled_step, StepId{6});
  EXPECT_EQ(placement.first_step, StepId{6});
  EXPECT_FALSE(placement.isLate());
}

/// @brief A transmission arriving after its scheduled step ran takes part in the first step not yet run, and is late
TEST(LateTransmissionPolicy, MovesLateTransmissionToFirstUnprocessedStep) {
  // Given: a transmission at step 5, arriving when steps 6 and 7 have already run
  // When: it is placed
  late_transmission_policy::Placement placement = late_transmission_policy::placementOf(StepId{5}, StepId{8});

  // Then: it takes part in step 8 instead of step 6, as a deviation
  EXPECT_EQ(placement.scheduled_step, StepId{6});
  EXPECT_EQ(placement.first_step, StepId{8});
  EXPECT_TRUE(placement.isLate());
}

}  // namespace platform
}  // namespace orbital
}  // namespace snake
