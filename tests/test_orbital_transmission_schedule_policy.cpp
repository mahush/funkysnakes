// Tests for the Transmission Schedule Policy: when the orbital link offers a transmission opportunity.

#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {

/// @section transmission_schedule_policy::intervalAfter

namespace {

struct IntervalCase {
  StepId opportunity;
  int expected_interval;
};

class TransmissionScheduleInterval : public ::testing::TestWithParam<IntervalCase> {};

}  // namespace

/// @brief The interval shrinks from 4 s (20 steps) at the start of the game to 0.5 s at its end, always a whole
/// number of 200 ms steps, so the end comes out as 3 steps, 0.6 s
TEST_P(TransmissionScheduleInterval, ShrinksLinearlyOverGameRoundedToSteps) {
  // When: the interval after an opportunity is computed
  int interval = transmission_schedule_policy::intervalAfter(GetParam().opportunity);

  // Then: it lies on the line from 4 s to 0.5 s, rounded to the nearest step
  EXPECT_EQ(interval, GetParam().expected_interval);
}

INSTANTIATE_TEST_SUITE_P(Game,
                         TransmissionScheduleInterval,
                         ::testing::Values(IntervalCase{StepId{0}, 20},
                                           IntervalCase{StepId{150}, 16},
                                           IntervalCase{StepId{300}, 11},
                                           IntervalCase{StepId{450}, 7},
                                           IntervalCase{StepId{463}, 6},
                                           IntervalCase{StepId{600}, 3},
                                           IntervalCase{StepId{900}, 3}));

/// @section transmission_schedule_policy::nextOpportunityAfter

TEST(TransmissionSchedulePolicy, OffersFirstOpportunityAtFirstStep) {
  // When: the first opportunity after round start is looked up
  StepId next = transmission_schedule_policy::nextOpportunityAfter(StepId{0});

  // Then: it falls on the first step
  EXPECT_EQ(next, StepId{1});
}

/// @brief An opportunity that has been reached already lies behind; the next one follows its interval
TEST(TransmissionSchedulePolicy, LooksPastOpportunityAtGivenStep) {
  // When: the next opportunity after the first one is looked up
  StepId next = transmission_schedule_policy::nextOpportunityAfter(StepId{1});

  // Then: it lies the longest interval, 20 steps, later
  EXPECT_EQ(next, StepId{21});
}

/// @section transmission_schedule_policy::isOpportunity

TEST(TransmissionSchedulePolicy, OffersNoOpportunityBetweenTwoOpportunities) {
  // When: the step after the first opportunity is checked
  bool opportunity = transmission_schedule_policy::isOpportunity(StepId{2});

  // Then: it is no opportunity, since the next one lies 20 steps later
  EXPECT_FALSE(opportunity);
}

TEST(TransmissionSchedulePolicy, OffersOpportunityAfterLongestInterval) {
  // When: the step 20 steps after the first opportunity is checked
  bool opportunity = transmission_schedule_policy::isOpportunity(StepId{21});

  // Then: it is an opportunity
  EXPECT_TRUE(opportunity);
}

/// @brief Near the end of the game the opportunities chain at the shortest interval
TEST(TransmissionSchedulePolicy, OffersOpportunityEveryThirdStepAtEndOfGame) {
  // When: the opportunity at step 596 and the step after it are checked
  bool at_opportunity = transmission_schedule_policy::isOpportunity(StepId{596});
  bool after_it = transmission_schedule_policy::isOpportunity(StepId{597});

  // Then: the first is an opportunity, the next step is not
  EXPECT_TRUE(at_opportunity);
  EXPECT_FALSE(after_it);
}

/// @section transmission_schedule_policy::firstStepAfterTransmission

/// @brief The opportunity comes after the step's movement, so what it sends cannot move in that step any more
TEST(TransmissionSchedulePolicy, LetsTransmissionTakePartFromNextStep) {
  // When: the first step of a transmission in step 21 is computed at compile time
  constexpr StepId first = transmission_schedule_policy::firstStepAfterTransmission(StepId{21});

  // Then: it is step 22
  EXPECT_EQ(first, StepId{22});
}

/// @section transmission_schedule_policy::firstOpportunityAfterCompletion

/// @brief A completion happens in the step's movement, before the step's opportunity
TEST(TransmissionSchedulePolicy, CountsOpportunityOfCompletionStepAsAfterCompletion) {
  // When: the first opportunity after a completion in step 21 is computed at compile time
  constexpr StepId first = transmission_schedule_policy::firstOpportunityAfterCompletion(StepId{21});

  // Then: it is the opportunity of step 21 itself
  EXPECT_EQ(first, StepId{21});
}

TEST(TransmissionSchedulePolicy, FindsFirstOpportunityAfterCompletionBetweenOpportunities) {
  // When: the first opportunity after a completion in step 15 is computed at compile time
  constexpr StepId first = transmission_schedule_policy::firstOpportunityAfterCompletion(StepId{15});

  // Then: it is step 21
  EXPECT_EQ(first, StepId{21});
}

/// @section transmission_schedule_policy::opportunityOfSubmission

/// @brief A step's submissions come after the step's opportunity, so they wait for the next one
TEST(TransmissionSchedulePolicy, SendsSubmissionInOpportunityStepAtNextOpportunity) {
  // When: the opportunity of a submission in step 21 is computed at compile time
  constexpr StepId opportunity = transmission_schedule_policy::opportunityOfSubmission(StepId{21});

  // Then: it is the next opportunity, step 40
  EXPECT_EQ(opportunity, StepId{40});
}

TEST(TransmissionSchedulePolicy, SendsSubmissionBetweenOpportunitiesAtFollowingOpportunity) {
  // When: the opportunity of a submission in step 15 is computed at compile time
  constexpr StepId opportunity = transmission_schedule_policy::opportunityOfSubmission(StepId{15});

  // Then: it is step 21
  EXPECT_EQ(opportunity, StepId{21});
}

/// @section transmission_schedule_policy::firstOpportunityFrom

/// @brief An opportunity in the given step itself counts
TEST(TransmissionSchedulePolicy, OffersOpportunityAtTheStepItself) {
  // When: the first opportunity from step 21 is computed at compile time
  constexpr StepId first = transmission_schedule_policy::firstOpportunityFrom(StepId{21});

  // Then: it is step 21
  EXPECT_EQ(first, StepId{21});
}

TEST(TransmissionSchedulePolicy, OffersFirstOpportunityAfterTimeBetweenOpportunities) {
  // When: the first opportunity from step 15 is computed at compile time
  constexpr StepId first = transmission_schedule_policy::firstOpportunityFrom(StepId{15});

  // Then: it is step 21
  EXPECT_EQ(first, StepId{21});
}

/// @section transmission_schedule_policy::lastOpportunityBefore

TEST(TransmissionSchedulePolicy, FindsLastOpportunityStrictlyBefore) {
  // When: the last opportunity before step 21 is computed at compile time
  constexpr std::optional<StepId> last = transmission_schedule_policy::lastOpportunityBefore(StepId{21});

  // Then: it is the first opportunity at step 1
  EXPECT_EQ(last, StepId{1});
}

TEST(TransmissionSchedulePolicy, FindsNoOpportunityBeforeFirst) {
  // When: the last opportunity before step 1 is computed at compile time
  constexpr std::optional<StepId> last = transmission_schedule_policy::lastOpportunityBefore(StepId{1});

  // Then: there is none
  EXPECT_FALSE(last.has_value());
}

/// @section transmission_schedule_policy::opportunitiesOfGame

TEST(TransmissionSchedulePolicy, SchedulesSeventyOneOpportunitiesPerGame) {
  // When: the opportunities of a game are listed
  const std::vector<StepId>& opportunities = transmission_schedule_policy::opportunitiesOfGame();

  // Then: there are 71, from step 1 to step 599
  ASSERT_EQ(opportunities.size(), 71U);
  EXPECT_EQ(opportunities.front(), StepId{1});
  EXPECT_EQ(opportunities.back(), StepId{599});
}

}  // namespace orbital
}  // namespace snake
