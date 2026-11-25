#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "platform/orbital/transmission_participation_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {

using transmission_participation_policy::Participation;
using transmission_participation_policy::participationOf;
using transmission_participation_policy::Superseded;
using transmission_participation_policy::TakesPart;
using transmission_participation_policy::TakesPartLate;
using transmission_participation_policy::Waits;

namespace {

// A transmission, submitted one step before the opportunity that sent it
OrderTransmitted makeTransmission(const PlayerId& player, OrderNumber order, StepId transmitted_at) {
  return OrderTransmitted{
      RoundId{1}, order, player, Point{order.value, 7}, StepId{transmitted_at.value - 1}, transmitted_at, std::nullopt};
}

}  // namespace

/// @section transmission_participation_policy::participationOf

TEST(TransmissionParticipationPolicy, LetsTransmissionWaitBeforeItsScheduledStep) {
  // When: a transmission in step 21 is asked about step 21
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  Participation participation = participationOf(fourth, {fourth}, StepId{21});

  // Then: it waits
  EXPECT_EQ(participation, Participation{Waits{}});
}

TEST(TransmissionParticipationPolicy, LetsTransmissionTakePartInItsScheduledStep) {
  // When: a transmission in step 21 is asked about step 22
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  Participation participation = participationOf(fourth, {fourth}, StepId{22});

  // Then: it takes part
  EXPECT_EQ(participation, Participation{TakesPart{}});
}

namespace {

class TransmissionParticipationPolicyMissedStep : public ::testing::TestWithParam<StepId> {};

}  // namespace

/// @brief The player would otherwise lose an order through no fault of their own
TEST_P(TransmissionParticipationPolicyMissedStep, LetsTransmissionTakePartLateAfterItsScheduledStep) {
  // When: a transmission in step 21 is asked about a step after step 22
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  Participation participation = participationOf(fourth, {fourth}, GetParam());

  // Then: it takes part late
  EXPECT_EQ(participation, Participation{TakesPartLate{}});
}

INSTANTIATE_TEST_SUITE_P(LaterSteps,
                         TransmissionParticipationPolicyMissedStep,
                         ::testing::Values(StepId{23}, StepId{40}));

/// @brief A late order must not delay a punctual one
TEST(TransmissionParticipationPolicy, SupersedesLateOrderByNewerPunctualOne) {
  // When: a late order 4 is asked about the scheduled step of order 5
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  Participation participation = participationOf(fourth, {fourth, fifth}, StepId{25});

  // Then: order 5 takes its place
  EXPECT_EQ(participation, Participation{Superseded{OrderNumber{5}}});
}

TEST(TransmissionParticipationPolicy, LetsPunctualOrderTakePartDespiteOlderLateOne) {
  // When: order 5 is asked about its scheduled step while a late order 4 waits
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  Participation participation = participationOf(fifth, {fourth, fifth}, StepId{25});

  // Then: it takes part
  EXPECT_EQ(participation, Participation{TakesPart{}});
}

namespace {

class TransmissionParticipationPolicyOlderLateOrder : public ::testing::TestWithParam<std::size_t> {};

// Three orders of player A that all missed their steps by step 29, the newest in the middle
std::vector<OrderTransmitted> makeLateTransmissions() {
  return {makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24}),
          makeTransmission(PLAYER_A, OrderNumber{6}, StepId{27}),
          makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21})};
}

}  // namespace

TEST_P(TransmissionParticipationPolicyOlderLateOrder, IsSupersededByNewestLateOrder) {
  // When: an order other than the newest of three late ones is asked about
  const std::vector<OrderTransmitted> waiting = makeLateTransmissions();
  Participation participation = participationOf(waiting.at(GetParam()), waiting, StepId{29});

  // Then: the newest takes its place
  EXPECT_EQ(participation, Participation{Superseded{OrderNumber{6}}});
}

INSTANTIATE_TEST_SUITE_P(FirstAndLastWaiting,
                         TransmissionParticipationPolicyOlderLateOrder,
                         ::testing::Values(std::size_t{0}, std::size_t{2}));

TEST(TransmissionParticipationPolicy, LetsNewestOfSeveralLateOrdersTakePartLate) {
  // When: the newest of three late orders is asked about
  const std::vector<OrderTransmitted> waiting = makeLateTransmissions();
  Participation participation = participationOf(waiting.at(1), waiting, StepId{29});

  // Then: it takes part late
  EXPECT_EQ(participation, Participation{TakesPartLate{}});
}

/// @brief Only orders that compete for the same step take each other's place
TEST(TransmissionParticipationPolicy, LetsLateOrderTakePartBeforeNewerOneIsDue) {
  // When: a late order 4 is asked about a step before the scheduled step of order 5
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  Participation participation = participationOf(fourth, {fourth, fifth}, StepId{23});

  // Then: it takes part late
  EXPECT_EQ(participation, Participation{TakesPartLate{}});
}

TEST(TransmissionParticipationPolicy, KeepsPlayersOrdersIndependent) {
  // When: order 4 of player A is asked about while a higher number of player B is due
  const OrderTransmitted of_a = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted of_b = makeTransmission(PLAYER_B, OrderNumber{9}, StepId{21});
  Participation participation = participationOf(of_a, {of_a, of_b}, StepId{22});

  // Then: it takes part
  EXPECT_EQ(participation, Participation{TakesPart{}});
}

}  // namespace platform
}  // namespace orbital
}  // namespace snake
