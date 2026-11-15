// Tests for the Transmission Schedule Policy: when the orbital link offers a transmission opportunity.

#include <gtest/gtest.h>

#include <chrono>

#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

/// @section transmission_schedule_policy::intervalAfter

namespace {

struct IntervalCase {
  milliseconds opportunity;
  milliseconds expected_interval;
};

class TransmissionScheduleInterval : public ::testing::TestWithParam<IntervalCase> {};

}  // namespace

/// @brief The interval shrinks from 4 s at the start of the game to 0.5 s at its end, always a whole number of 200 ms
/// steps, so the end comes out as 0.6 s
TEST_P(TransmissionScheduleInterval, ShrinksLinearlyOverGameRoundedToSteps) {
  // When: the interval after an opportunity is computed
  milliseconds interval = transmission_schedule_policy::intervalAfter(GetParam().opportunity);

  // Then: it lies on the line from 4 s to 0.5 s, rounded to the nearest step
  EXPECT_EQ(interval, GetParam().expected_interval);
}

INSTANTIATE_TEST_SUITE_P(Game,
                         TransmissionScheduleInterval,
                         ::testing::Values(IntervalCase{milliseconds{0}, milliseconds{4000}},
                                           IntervalCase{milliseconds{30000}, milliseconds{3200}},
                                           IntervalCase{milliseconds{60000}, milliseconds{2200}},
                                           IntervalCase{milliseconds{90000}, milliseconds{1400}},
                                           IntervalCase{milliseconds{92600}, milliseconds{1200}},
                                           IntervalCase{milliseconds{120000}, milliseconds{600}},
                                           IntervalCase{milliseconds{180000}, milliseconds{600}}));

/// @section transmission_schedule_policy::nextOpportunityAfter

TEST(TransmissionSchedulePolicy, OffersFirstOpportunityAtFirstStep) {
  // When: the first opportunity after round start is looked up
  milliseconds next = transmission_schedule_policy::nextOpportunityAfter(milliseconds{0});

  // Then: it falls on the first step
  EXPECT_EQ(next, milliseconds{200});
}

/// @brief An opportunity that has been reached already lies behind; the next one follows its interval
TEST(TransmissionSchedulePolicy, LooksPastOpportunityAtGivenTime) {
  // When: the next opportunity after the first one is looked up
  milliseconds next = transmission_schedule_policy::nextOpportunityAfter(milliseconds{200});

  // Then: it lies the longest interval later
  EXPECT_EQ(next, milliseconds{4200});
}

/// @section transmission_schedule_policy::isOpportunity

TEST(TransmissionSchedulePolicy, OffersNoOpportunityBetweenTwoOpportunities) {
  // When: the step after the first opportunity is checked
  bool opportunity = transmission_schedule_policy::isOpportunity(milliseconds{400});

  // Then: it is no opportunity, since the next one lies 4 s later
  EXPECT_FALSE(opportunity);
}

TEST(TransmissionSchedulePolicy, OffersOpportunityAfterLongestInterval) {
  // When: the time 4 s after the first opportunity is checked
  bool opportunity = transmission_schedule_policy::isOpportunity(milliseconds{4200});

  // Then: it is an opportunity
  EXPECT_TRUE(opportunity);
}

/// @brief Near the end of the game the opportunities chain at the shortest interval
TEST(TransmissionSchedulePolicy, OffersOpportunityEveryThirdStepAtEndOfGame) {
  // When: the opportunity at 119.2 s and the step after it are checked
  bool at_opportunity = transmission_schedule_policy::isOpportunity(milliseconds{119200});
  bool after_it = transmission_schedule_policy::isOpportunity(milliseconds{119400});

  // Then: the first is an opportunity, the next step is not
  EXPECT_TRUE(at_opportunity);
  EXPECT_FALSE(after_it);
}

}  // namespace orbital
}  // namespace snake
