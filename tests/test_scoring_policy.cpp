// Tests for the Scoring Policy: what arena events are worth.

#include <gtest/gtest.h>

#include "classic/scoring_policy.hpp"

namespace snake {

namespace {

PerPlayerScores startScores() { return {{PLAYER_A, 100}, {PLAYER_B, 100}}; }

}  // namespace

/// @section scoring_policy::applyScoring

/// @brief Ensures that eating food earns points
TEST(ScoringPolicy, AwardsTenPointsForEatenFood) {
  // When: eaten food is scored
  PerPlayerScores scores = scoring_policy::applyScoring(startScores(), {FoodEaten{PLAYER_A}});

  // Then: the eater gains ten points
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 110}, {PLAYER_B, 100}}));
}

/// @brief Ensures that only the victim of a bite pays, while the biter gets nothing
TEST(ScoringPolicy, DeductsTenPointsFromBittenVictimOnly) {
  // When: a bite is scored
  PerPlayerScores scores = scoring_policy::applyScoring(startScores(), {Bitten{PLAYER_B, PLAYER_A}});

  // Then: only the victim loses ten points
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 100}, {PLAYER_B, 90}}));
}

/// @brief Ensures that biting yourself costs points
TEST(ScoringPolicy, DeductsTenPointsForSelfBite) {
  // When: a self-bite is scored
  PerPlayerScores scores = scoring_policy::applyScoring(startScores(), {SelfBitten{PLAYER_A}});

  // Then: the snake loses ten points
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
}

/// @brief Ensures that a mutual bite costs both snakes points
TEST(ScoringPolicy, DeductsTenPointsFromBothInMutualBite) {
  // When: a mutual bite is scored
  PerPlayerScores scores = scoring_policy::applyScoring(startScores(), {MutualBite{PLAYER_A, PLAYER_B}});

  // Then: both snakes lose ten points
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
}

/// @brief Ensures that all events of a step add up
TEST(ScoringPolicy, AppliesAllEventsOfAStep) {
  // When: several events are scored together
  PerPlayerScores scores =
      scoring_policy::applyScoring(startScores(), {Bitten{PLAYER_B, PLAYER_A}, FoodEaten{PLAYER_A}});

  // Then: each of them changes the scores
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 110}, {PLAYER_B, 90}}));
}

/// @brief Ensures that a step without events leaves the scores unchanged
TEST(ScoringPolicy, KeepsScoresWithoutEvents) {
  // When: a step without events is scored
  PerPlayerScores scores = scoring_policy::applyScoring(startScores(), {});

  // Then: the scores are unchanged
  EXPECT_EQ(scores, startScores());
}

}  // namespace snake
