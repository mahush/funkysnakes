// Tests for the Orbital Scoring Policy: what arena events are worth and when points count.

#include <gtest/gtest.h>

#include <vector>

#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {

namespace {

using scoring_policy::Credit;
using scoring_policy::Hold;
using scoring_policy::PointEffect;
using scoring_policy::Settlement;

}  // namespace

/// @section scoring_policy::effectsOf

/// @subsection food

TEST(OrbitalScoringPolicy, HoldsFoodCollectedDuringOrder) {
  // When: the effects of food collected during order 1 are decided
  std::vector<PointEffect> effects = scoring_policy::effectsOf(FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}});

  // Then: ten points are held for the order
  EXPECT_EQ(effects, (std::vector<PointEffect>{Hold{OrderId{1}, PLAYER_A, 10}}));
}

TEST(OrbitalScoringPolicy, CreditsFoodCollectedWithoutOrderAtOnce) {
  // When: the effects of food collected without an executing order are decided
  std::vector<PointEffect> effects = scoring_policy::effectsOf(FoodCollected{PLAYER_A, Point{1, 1}, std::nullopt});

  // Then: the eater is credited five points
  EXPECT_EQ(effects, (std::vector<PointEffect>{Credit{PLAYER_A, 5}}));
}

/// @subsection collisions

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsFromBittenVictimOnly) {
  // When: the effects of a bite are decided
  std::vector<PointEffect> effects = scoring_policy::effectsOf(Bitten{PLAYER_B, PLAYER_A});

  // Then: only the victim loses ten credited points
  EXPECT_EQ(effects, (std::vector<PointEffect>{Credit{PLAYER_B, -10}}));
}

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsForSelfBite) {
  // When: the effects of a self-bite are decided
  std::vector<PointEffect> effects = scoring_policy::effectsOf(SelfBitten{PLAYER_A});

  // Then: the snake loses ten credited points
  EXPECT_EQ(effects, (std::vector<PointEffect>{Credit{PLAYER_A, -10}}));
}

TEST(OrbitalScoringPolicy, DeductsTenCreditedPointsFromBothInMutualBite) {
  // When: the effects of a mutual bite are decided
  std::vector<PointEffect> effects = scoring_policy::effectsOf(MutualBite{PLAYER_A, PLAYER_B});

  // Then: both snakes lose ten credited points
  EXPECT_EQ(effects, (std::vector<PointEffect>{Credit{PLAYER_A, -10}, Credit{PLAYER_B, -10}}));
}

/// @subsection order endings

/// @brief What an ending means for the order's points is the settlement's decision alone
TEST(OrbitalScoringPolicy, LeavesOrderEndingWithoutPointEffect) {
  // When: the effects of an order ending are decided
  std::vector<PointEffect> effects =
      scoring_policy::effectsOf(OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt});

  // Then: there are none
  EXPECT_TRUE(effects.empty());
}

/// @section scoring_policy::settlementOf

/// @brief Unbanked points count once the order actually completes
TEST(OrbitalScoringPolicy, BanksCompletedOrder) {
  // When: the settlement of a completed order is decided
  Settlement settlement = scoring_policy::settlementOf(EndReason::COMPLETED);

  // Then: its points are banked
  EXPECT_EQ(settlement, Settlement::BANK);
}

namespace {

class OrbitalScoringPolicyDrop : public ::testing::TestWithParam<EndReason> {};

}  // namespace

/// @brief Unbanked points only count on completion; any other ending, the end of the round included, drops them
TEST_P(OrbitalScoringPolicyDrop, DropsOrderEndedUncompleted) {
  // When: the settlement of an order that ended without completing is decided
  Settlement settlement = scoring_policy::settlementOf(GetParam());

  // Then: its points are dropped
  EXPECT_EQ(settlement, Settlement::DROP);
}

INSTANTIATE_TEST_SUITE_P(EndReasons,
                         OrbitalScoringPolicyDrop,
                         ::testing::Values(EndReason::INTERRUPTED, EndReason::ELIMINATED, EndReason::ROUND_ENDED));

/// @section scoring_policy::handoverBonus

namespace {

struct HandoverBonusCase {
  int handover;
  int bonus;
};

class OrbitalScoringPolicyHandoverBonus : public ::testing::TestWithParam<HandoverBonusCase> {};

}  // namespace

/// @brief Consecutive handovers double the bonus; the cap keeps a long chain from deciding the game alone
TEST_P(OrbitalScoringPolicyHandoverBonus, DoublesBonusForEachConsecutiveHandoverUpToCap) {
  // When: the bonus of a handover is computed
  int bonus = scoring_policy::handoverBonus(GetParam().handover);

  // Then: it is the base doubled per earlier handover, capped
  EXPECT_EQ(bonus, GetParam().bonus);
}

INSTANTIATE_TEST_SUITE_P(Handovers,
                         OrbitalScoringPolicyHandoverBonus,
                         ::testing::Values(HandoverBonusCase{1, 10},
                                           HandoverBonusCase{2, 20},
                                           HandoverBonusCase{3, 40},
                                           HandoverBonusCase{4, 80},
                                           HandoverBonusCase{5, 80},
                                           HandoverBonusCase{12, 80}));

}  // namespace orbital
}  // namespace snake
