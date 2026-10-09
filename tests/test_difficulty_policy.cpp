// Tests for the Difficulty Policy: level -> step interval.

#include <gtest/gtest.h>

#include <string>

#include "classic/difficulty_policy.hpp"

namespace snake {

/// @section difficulty_policy::stepIntervalMs

namespace {

struct DifficultyCase {
  int level;
  int expected_interval_ms;
};

class DifficultyPolicy : public ::testing::TestWithParam<DifficultyCase> {};

}  // namespace

/// @brief Ensures that each level speeds the game up by 15 ms, down to a minimum of 50 ms
TEST_P(DifficultyPolicy, MapsLevelToStepInterval) {
  // When: the step interval of a level is computed
  int interval_ms = difficulty_policy::stepIntervalMs(GetParam().level);

  // Then: it matches the expected interval
  EXPECT_EQ(interval_ms, GetParam().expected_interval_ms);
}

INSTANTIATE_TEST_SUITE_P(
    Levels,
    DifficultyPolicy,
    ::testing::Values(DifficultyCase{1, 200}, DifficultyCase{2, 185}, DifficultyCase{11, 50}, DifficultyCase{20, 50}),
    [](const ::testing::TestParamInfo<DifficultyCase>& info) { return "Level" + std::to_string(info.param.level); });

}  // namespace snake
