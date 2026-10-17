// Tests for the Orbital Latency Policy: issue time -> communication delay and eligibility deadline.

#include <gtest/gtest.h>

#include <chrono>

#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

/// @section latency_policy::delayAt

namespace {

struct DelayCase {
  milliseconds issued_at;
  milliseconds expected_delay;
};

class LatencyPolicyDelay : public ::testing::TestWithParam<DelayCase> {};

}  // namespace

/// @brief The delay rises from 0.2 s at the start of the 60 s cycle to 2 s at its middle, falls back and repeats
TEST_P(LatencyPolicyDelay, FollowsTriangleWaveOverOrbitalCycle) {
  // When: the delay at an issue time is computed
  milliseconds delay = latency_policy::delayAt(GetParam().issued_at);

  // Then: it matches the triangle wave
  EXPECT_EQ(delay, GetParam().expected_delay);
}

INSTANTIATE_TEST_SUITE_P(OrbitalCycle,
                         LatencyPolicyDelay,
                         ::testing::Values(DelayCase{milliseconds{0}, milliseconds{200}},
                                           DelayCase{milliseconds{15000}, milliseconds{1100}},
                                           DelayCase{milliseconds{30000}, milliseconds{2000}},
                                           DelayCase{milliseconds{45000}, milliseconds{1100}},
                                           DelayCase{milliseconds{60000}, milliseconds{200}},
                                           DelayCase{milliseconds{90000}, milliseconds{2000}}));

/// @section latency_policy::deadlineFor

/// @brief A deadline is fixed by the orbital condition at issue time, not by the condition when it elapses
TEST(LatencyPolicy, AddsDelayAtIssueTimeToIssueTime) {
  // When: the deadline of an order issued at the peak of the cycle is computed
  milliseconds deadline = latency_policy::deadlineFor(milliseconds{30000});

  // Then: it lies the peak delay after the issue time
  EXPECT_EQ(deadline, milliseconds{32000});
}

}  // namespace orbital
}  // namespace snake
