// Tests for the Orbital Scoring Policy: what arena events are worth and when points count.

#include <gtest/gtest.h>

#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {

namespace {

using scoring_policy::Scores;
using scoring_policy::Unbanked;

Scores startScores() { return Scores{{{PLAYER_A, 100}, {PLAYER_B, 100}}, {}}; }

}  // namespace

/// @section scoring_policy::applyScoring

/// @subsection food

TEST(OrbitalScoringPolicy, KeepsFoodCollectedDuringOrderUnbanked) {
  // When: food collected during order 1 is scored
  Scores scores = scoring_policy::applyScoring(startScores(), {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});

  // Then: the order holds ten unbanked points and the credited score is unchanged
  EXPECT_EQ(scores.unbanked.at(OrderId{1}), (Unbanked{PLAYER_A, 10}));
  EXPECT_EQ(scores.credited.at(PLAYER_A), 100);
}

TEST(OrbitalScoringPolicy, CreditsFoodCollectedWithoutOrderAtOnce) {
  // When: food collected without an executing order is scored
  Scores scores = scoring_policy::applyScoring(startScores(), {FoodCollected{PLAYER_A, Point{1, 1}, std::nullopt}});

  // Then: the eater is credited ten points
  EXPECT_EQ(scores.credited.at(PLAYER_A), 110);
  EXPECT_TRUE(scores.unbanked.empty());
}

/// @subsection order endings

/// @brief F's +20 count once F actually completes
TEST(OrbitalScoringPolicy, BanksUnbankedPointsOnCompletion) {
  // Given: order F holds 20 unbanked points
  Scores scores = scoring_policy::applyScoring(
      startScores(),
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}});

  // When: F's completion is scored
  Scores banked = scoring_policy::applyScoring(scores, {OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}});

  // Then: the 20 points are credited and nothing is unbanked any more
  EXPECT_EQ(banked.credited.at(PLAYER_A), 120);
  EXPECT_TRUE(banked.unbanked.empty());
}

namespace {

class OrbitalScoringPolicyLoss : public ::testing::TestWithParam<EndReason> {};

}  // namespace

/// @brief Unbanked points only count on completion; any other ending loses them
TEST_P(OrbitalScoringPolicyLoss, LosesUnbankedPointsWhenOrderEndsUncompleted) {
  // Given: order F holds 20 unbanked points
  Scores scores = scoring_policy::applyScoring(
      startScores(),
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}});

  // When: F ends without completing
  Scores lost = scoring_policy::applyScoring(scores, {OrderEnded{OrderId{1}, GetParam(), std::nullopt}});

  // Then: the points are lost and the credited score is unchanged
  EXPECT_EQ(lost.credited.at(PLAYER_A), 100);
  EXPECT_TRUE(lost.unbanked.empty());
}

INSTANTIATE_TEST_SUITE_P(EndReasons,
                         OrbitalScoringPolicyLoss,
                         ::testing::Values(EndReason::INTERRUPTED, EndReason::ELIMINATED, EndReason::ROUND_ENDED));

/// @brief Food collected during an order and its completion in the same step count in order
TEST(OrbitalScoringPolicy, BanksFoodCollectedOnArrivalStep) {
  // When: collection during order 1 followed by its completion is scored
  Scores scores = scoring_policy::applyScoring(
      startScores(),
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}});

  // Then: the points are credited
  EXPECT_EQ(scores.credited.at(PLAYER_A), 110);
}

TEST(OrbitalScoringPolicy, AwardsNoPointsForCompletionAlone) {
  // When: the completion of an order without unbanked points is scored
  Scores scores =
      scoring_policy::applyScoring(startScores(), {OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}});

  // Then: nothing changes
  EXPECT_EQ(scores.credited, startScores().credited);
}

/// @brief A replacement's own points start fresh; the interrupted order's points do not move over
TEST(OrbitalScoringPolicy, KeepsUnbankedPointsPerOrder) {
  // Given: order F holds 10 unbanked points
  Scores scores = scoring_policy::applyScoring(startScores(), {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});

  // When: G interrupts F and collects food
  Scores next = scoring_policy::applyScoring(
      scores,
      {OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{2}}});

  // Then: only G's 10 points are unbanked
  EXPECT_EQ(next.unbanked, (std::map<OrderId, Unbanked>{{OrderId{2}, Unbanked{PLAYER_A, 10}}}));
}

/// @subsection collisions

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsFromBittenVictimOnly) {
  // When: a bite is scored
  Scores scores = scoring_policy::applyScoring(startScores(), {Bitten{PLAYER_B, PLAYER_A}});

  // Then: only the victim loses ten credited points
  EXPECT_EQ(scores.credited, (PerPlayerScores{{PLAYER_A, 100}, {PLAYER_B, 90}}));
}

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsForSelfBite) {
  // When: a self-bite is scored
  Scores scores = scoring_policy::applyScoring(startScores(), {SelfBitten{PLAYER_A}});

  // Then: the snake loses ten credited points
  EXPECT_EQ(scores.credited, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
}

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsFromBothInMutualBite) {
  // When: a mutual bite is scored
  Scores scores = scoring_policy::applyScoring(startScores(), {MutualBite{PLAYER_A, PLAYER_B}});

  // Then: both snakes lose ten credited points
  EXPECT_EQ(scores.credited, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
}

/// @brief A penalty counts at once and does not touch an executing order's unbanked points
TEST(OrbitalScoringPolicy, KeepsUnbankedPointsOnCollisionPenalty) {
  // Given: order F of B holds 10 unbanked points
  Scores scores = scoring_policy::applyScoring(startScores(), {FoodCollected{PLAYER_B, Point{1, 1}, OrderId{1}}});

  // When: B is bitten
  Scores bitten = scoring_policy::applyScoring(scores, {Bitten{PLAYER_B, PLAYER_A}});

  // Then: B lost ten credited points and F still holds its 10 unbanked points
  EXPECT_EQ(bitten.credited.at(PLAYER_B), 90);
  EXPECT_EQ(bitten.unbanked.at(OrderId{1}), (Unbanked{PLAYER_B, 10}));
}

/// @section scoring_policy::loseUnbanked

TEST(OrbitalScoringPolicy, LosesAllUnbankedPointsAtConclusion) {
  // Given: an order holds unbanked points
  Scores scores = scoring_policy::applyScoring(startScores(), {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});

  // When: the unbanked points are lost
  Scores concluded = scoring_policy::loseUnbanked(scores);

  // Then: only credited points remain
  EXPECT_TRUE(concluded.unbanked.empty());
  EXPECT_EQ(concluded.credited, startScores().credited);
}

}  // namespace orbital
}  // namespace snake
