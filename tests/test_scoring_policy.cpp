// Tests for the Scoring Policy: what arena events are worth.

#include <gtest/gtest.h>

#include <vector>

#include "classic/scoring_policy.hpp"

namespace snake {

namespace {

using scoring_policy::ScoreChange;

}  // namespace

/// @section scoring_policy::effectsOf

TEST(ScoringPolicy, AwardsTenPointsForEatenFood) {
  // When: the score changes of eaten food are decided
  std::vector<ScoreChange> changes = scoring_policy::effectsOf(FoodEaten{PLAYER_A});

  // Then: the eater gains ten points
  EXPECT_EQ(changes, (std::vector<ScoreChange>{{PLAYER_A, 10}}));
}

/// @brief Only the victim of a bite pays; the biter gets nothing from the bite itself
TEST(ScoringPolicy, DeductsTenPointsFromBittenVictimOnly) {
  // When: the score changes of a bite are decided
  std::vector<ScoreChange> changes = scoring_policy::effectsOf(Bitten{PLAYER_B, PLAYER_A});

  // Then: only the victim loses ten points
  EXPECT_EQ(changes, (std::vector<ScoreChange>{{PLAYER_B, -10}}));
}

TEST(ScoringPolicy, DeductsTenPointsForSelfBite) {
  // When: the score changes of a self-bite are decided
  std::vector<ScoreChange> changes = scoring_policy::effectsOf(SelfBitten{PLAYER_A});

  // Then: the snake loses ten points
  EXPECT_EQ(changes, (std::vector<ScoreChange>{{PLAYER_A, -10}}));
}

TEST(ScoringPolicy, DeductsTenPointsFromBothInMutualBite) {
  // When: the score changes of a mutual bite are decided
  std::vector<ScoreChange> changes = scoring_policy::effectsOf(MutualBite{PLAYER_A, PLAYER_B});

  // Then: both snakes lose ten points
  EXPECT_EQ(changes, (std::vector<ScoreChange>{{PLAYER_A, -10}, {PLAYER_B, -10}}));
}

}  // namespace snake
