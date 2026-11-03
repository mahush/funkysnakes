// Tests for the Order Release Policy: commitment time -> Order Delay, Order Release Time and release.

#include <gtest/gtest.h>

#include <chrono>

#include "orbital/order_release_policy.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

/// @section order_release_policy::delayAt

namespace {

struct DelayCase {
  milliseconds committed_at;
  milliseconds expected_delay;
};

class OrderReleasePolicyDelay : public ::testing::TestWithParam<DelayCase> {};

}  // namespace

/// @brief The delay rises from 0.2 s at the start of the 60 s cycle to 2 s at its middle, falls back and repeats
TEST_P(OrderReleasePolicyDelay, FollowsTriangleWaveOverOrbitalCycle) {
  // When: the delay at a commitment time is computed
  milliseconds delay = order_release_policy::delayAt(GetParam().committed_at);

  // Then: it matches the triangle wave
  EXPECT_EQ(delay, GetParam().expected_delay);
}

INSTANTIATE_TEST_SUITE_P(OrbitalCycle,
                         OrderReleasePolicyDelay,
                         ::testing::Values(DelayCase{milliseconds{0}, milliseconds{200}},
                                           DelayCase{milliseconds{15000}, milliseconds{1100}},
                                           DelayCase{milliseconds{30000}, milliseconds{2000}},
                                           DelayCase{milliseconds{45000}, milliseconds{1100}},
                                           DelayCase{milliseconds{60000}, milliseconds{200}},
                                           DelayCase{milliseconds{90000}, milliseconds{2000}}));

/// @section order_release_policy::releaseTime

/// @brief A release time is fixed by the orbital condition at commitment time, not by the condition when it is reached
TEST(OrderReleasePolicy, AddsDelayAtCommitmentTimeToCommitmentTime) {
  // When: the release time of an order committed at the peak of the cycle is computed
  milliseconds release_time = order_release_policy::releaseTime(milliseconds{30000});

  // Then: it lies the peak delay after the commitment time
  EXPECT_EQ(release_time, milliseconds{32000});
}

/// @section order_release_policy::isReleased

TEST(OrderReleasePolicy, HoldsBackOrderBeforeItsReleaseTime) {
  // When: an order committed at 0 ms is checked just before its release time of 200 ms
  bool released = order_release_policy::isReleased(milliseconds{0}, milliseconds{199});

  // Then: it is not released
  EXPECT_FALSE(released);
}

TEST(OrderReleasePolicy, ReleasesOrderAtItsReleaseTime) {
  // When: an order committed at 0 ms is checked at its release time of 200 ms
  bool released = order_release_policy::isReleased(milliseconds{0}, milliseconds{200});

  // Then: it is released
  EXPECT_TRUE(released);
}

}  // namespace orbital
}  // namespace snake
