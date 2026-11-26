#include <gtest/gtest.h>

#include <chrono>

#include "domain/orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

/// @section pacing_policy::stepAt

TEST(PacingPolicy, PlacesRoundStartInStepZero) {
  // When: the step of round start is asked
  constexpr StepId step = pacing_policy::stepAt(milliseconds{0});

  // Then: it is step 0
  EXPECT_EQ(step, StepId{0});
}

/// @brief A step's movement comes first, so what happens at its time already belongs to it
TEST(PacingPolicy, PlacesTimeOfStepInThatStep) {
  // When: the step of the time at which step 21 falls due is asked
  constexpr StepId step = pacing_policy::stepAt(pacing_policy::timeOfStep(StepId{21}));

  // Then: it is step 21
  EXPECT_EQ(step, StepId{21});
}

TEST(PacingPolicy, PlacesTimeJustBeforeStepInPreviousStep) {
  // When: the step of one millisecond before step 21 falls due is asked
  constexpr StepId step = pacing_policy::stepAt(pacing_policy::timeOfStep(StepId{21}) - milliseconds{1});

  // Then: it is step 20
  EXPECT_EQ(step, StepId{20});
}

TEST(PacingPolicy, PlacesTimeWithinStepInThatStep) {
  // When: the step of a time halfway through step 21 is asked
  constexpr StepId step =
      pacing_policy::stepAt(pacing_policy::timeOfStep(StepId{21}) + pacing_policy::STEP_INTERVAL / 2);

  // Then: it is step 21
  EXPECT_EQ(step, StepId{21});
}

/// @brief Only a clock that runs behind the round's timeline produces such a time
TEST(PacingPolicy, PlacesTimeBeforeRoundStartInStepZero) {
  // When: the step of a time before round start is asked
  constexpr StepId step = pacing_policy::stepAt(milliseconds{-50});

  // Then: it is step 0
  EXPECT_EQ(step, StepId{0});
}

TEST(PacingPolicy, PlacesTimeLimitInLastStep) {
  // When: the step of the game's time limit is asked
  constexpr StepId step = pacing_policy::stepAt(pacing_policy::GAME_DURATION);

  // Then: it is the last step
  EXPECT_EQ(step, pacing_policy::LAST_STEP);
}

}  // namespace orbital
}  // namespace snake
