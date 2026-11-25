#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "platform/orbital/transmission_reception_authority.hpp"

namespace snake {
namespace orbital {
namespace platform {

using transmission_reception_authority::DiscardedTransmission;
using transmission_reception_authority::LateTransmission;
using transmission_reception_authority::Receipt;
using transmission_reception_authority::State;
using transmission_reception_authority::SupersededTransmission;

namespace {

// A transmission, submitted one step before the opportunity that sent it
OrderTransmitted makeTransmission(const PlayerId& player, OrderNumber order, StepId transmitted_at) {
  return OrderTransmitted{
      RoundId{1}, order, player, Point{order.value, 7}, StepId{transmitted_at.value - 1}, transmitted_at, std::nullopt};
}

// The order a transmission carries
Order orderOf(const OrderTransmitted& transmitted) {
  return Order{transmitted.round, transmitted.order, transmitted.player, transmitted.target};
}

// Reception of the round after the given transmissions arrived
State makeReception(const std::vector<OrderTransmitted>& transmissions) {
  State state = transmission_reception_authority::startRound();
  for (const OrderTransmitted& transmitted : transmissions) {
    state = std::get<State>(transmission_reception_authority::receive(std::move(state), transmitted));
  }
  return state;
}

// Reception after a further transmission arrived
State receiveAnother(State state, const OrderTransmitted& transmitted) {
  return std::get<State>(transmission_reception_authority::receive(std::move(state), transmitted));
}

// Reception after the orders for a step were handed over
State releaseStep(State state, StepId step) {
  return std::get<State>(transmission_reception_authority::release(std::move(state), step));
}

}  // namespace

/// @section transmission_reception_authority::receive

TEST(TransmissionReceptionAuthority, KeepsReceivedTransmissionWaiting) {
  // When: a transmission of player A arrives
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  auto [state, receipt] =
      transmission_reception_authority::receive(transmission_reception_authority::startRound(), fourth);

  // Then: it is received and waits
  EXPECT_EQ(receipt, Receipt::RECEIVED);
  EXPECT_EQ(transmission_reception_authority::waiting(state), std::vector<OrderTransmitted>{fourth});
}

/// @brief An order is identified by its player and its number
TEST(TransmissionReceptionAuthority, ReceivesSameOrderNumberOfBothPlayers) {
  // Given: order 4 of player A waits
  const OrderTransmitted of_a = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  State state = makeReception({of_a});

  // When: order 4 of player B arrives
  const OrderTransmitted of_b = makeTransmission(PLAYER_B, OrderNumber{4}, StepId{21});
  auto [next_state, receipt] = transmission_reception_authority::receive(state, of_b);

  // Then: both wait
  EXPECT_EQ(receipt, Receipt::RECEIVED);
  EXPECT_EQ(transmission_reception_authority::waiting(next_state), (std::vector<OrderTransmitted>{of_a, of_b}));
}

TEST(TransmissionReceptionAuthority, RefusesTransmissionAfterConclusion) {
  // Given: the round concluded
  auto [state, discarded] = transmission_reception_authority::conclude(transmission_reception_authority::startRound());

  // When: a transmission arrives
  auto [next_state, receipt] =
      transmission_reception_authority::receive(state, makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21}));

  // Then: it is refused and does not wait
  EXPECT_EQ(receipt, Receipt::ROUND_CONCLUDED);
  EXPECT_TRUE(transmission_reception_authority::waiting(next_state).empty());
}

/// @section transmission_reception_authority::release

TEST(TransmissionReceptionAuthority, HandsOverOrderForItsScheduledStep) {
  // Given: a transmission of step 21 arrived
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  State state = makeReception({fourth});

  // When: step 22 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{22});

  // Then: the order is handed over without a deviation, and waits no longer
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(fourth)}));
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
  EXPECT_TRUE(transmission_reception_authority::waiting(next_state).empty());
}

/// @brief A device whose clock runs ahead sends before the step of the opportunity has run
TEST(TransmissionReceptionAuthority, KeepsEarlyOrderWaiting) {
  // Given: a transmission of step 21 arrived before step 21 ran
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  State state = makeReception({fourth});

  // When: step 21 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{21});

  // Then: the order keeps waiting
  EXPECT_EQ(orders, std::vector<Order>{});
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
  EXPECT_EQ(transmission_reception_authority::waiting(next_state), std::vector<OrderTransmitted>{fourth});
}

TEST(TransmissionReceptionAuthority, HandsOverLateOrderForNextStepAsDeviation) {
  // Given: a transmission of step 21 arrived after step 22 ran
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  State state = receiveAnother(releaseStep(makeReception({}), StepId{22}), fourth);

  // When: step 23 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{23});

  // Then: the order is handed over and reported as late
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(fourth)}));
  EXPECT_EQ(late, (std::vector<LateTransmission>{LateTransmission{fourth, StepId{23}}}));
  EXPECT_TRUE(superseded.empty());
}

/// @brief A late order must not delay a punctual one
TEST(TransmissionReceptionAuthority, SupersedesLateOrderByNewerPunctualOne) {
  // Given: a late transmission of step 21 and a punctual one of step 24 arrived
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  State state = receiveAnother(receiveAnother(releaseStep(makeReception({}), StepId{24}), fourth), fifth);

  // When: step 25 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{25});

  // Then: the newer order is handed over, and the late one is superseded and waits no longer
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(fifth)}));
  EXPECT_TRUE(late.empty());
  EXPECT_EQ(superseded,
            (std::vector<SupersededTransmission>{SupersededTransmission{fourth, OrderNumber{5}, StepId{25}}}));
  EXPECT_TRUE(transmission_reception_authority::waiting(next_state).empty());
}

TEST(TransmissionReceptionAuthority, HandsOverNewestOfSeveralLateOrders) {
  // Given: three transmissions arrived late, in the order they were sent
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  const OrderTransmitted sixth = makeTransmission(PLAYER_A, OrderNumber{6}, StepId{27});
  State state = releaseStep(makeReception({}), StepId{28});
  state = receiveAnother(std::move(state), fourth);
  state = receiveAnother(std::move(state), fifth);
  state = receiveAnother(std::move(state), sixth);

  // When: step 29 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{29});

  // Then: the newest is handed over as late, and the others are superseded by it
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(sixth)}));
  EXPECT_EQ(late, (std::vector<LateTransmission>{LateTransmission{sixth, StepId{29}}}));
  EXPECT_EQ(superseded,
            (std::vector<SupersededTransmission>{SupersededTransmission{fourth, OrderNumber{6}, StepId{29}},
                                                 SupersededTransmission{fifth, OrderNumber{6}, StepId{29}}}));
}

TEST(TransmissionReceptionAuthority, HandsOverNewerOrderAfterOlderOne) {
  // Given: order 4 was handed over, and order 5 arrived in time
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  State state = receiveAnother(releaseStep(makeReception({fourth}), StepId{22}), fifth);

  // When: step 25 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{25});

  // Then: order 5 is handed over without a deviation
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(fifth)}));
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
}

TEST(TransmissionReceptionAuthority, HandsOverLateOrderBeforeNewerOneIsDue) {
  // Given: a late order and an early newer one arrived
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted fifth = makeTransmission(PLAYER_A, OrderNumber{5}, StepId{24});
  State state = receiveAnother(receiveAnother(releaseStep(makeReception({}), StepId{22}), fourth), fifth);

  // When: step 23 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{23});

  // Then: the late order is handed over, and the newer one keeps waiting
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(fourth)}));
  EXPECT_EQ(late, (std::vector<LateTransmission>{LateTransmission{fourth, StepId{23}}}));
  EXPECT_TRUE(superseded.empty());
  EXPECT_EQ(transmission_reception_authority::waiting(next_state), std::vector<OrderTransmitted>{fifth});
}

/// @brief A player's orders never take the place of the other player's
TEST(TransmissionReceptionAuthority, HandsOverOrdersOfBothPlayers) {
  // Given: order 4 of player B and order 9 of player A arrived for the same step
  const OrderTransmitted of_a = makeTransmission(PLAYER_A, OrderNumber{9}, StepId{21});
  const OrderTransmitted of_b = makeTransmission(PLAYER_B, OrderNumber{4}, StepId{21});
  State state = makeReception({of_b, of_a});

  // When: their step is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{22});

  // Then: both are handed over, ordered by player
  EXPECT_EQ(orders, (std::vector<Order>{orderOf(of_a), orderOf(of_b)}));
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
}

TEST(TransmissionReceptionAuthority, HandsOverOrderOnce) {
  // Given: a transmission was handed over for step 22
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  State state = releaseStep(makeReception({fourth}), StepId{22});

  // When: step 23 is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{23});

  // Then: nothing is handed over
  EXPECT_EQ(orders, std::vector<Order>{});
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
}

/// @section transmission_reception_authority::conclude

/// @brief An order never disappears unreported
TEST(TransmissionReceptionAuthority, DiscardsWaitingOrdersAtConclusion) {
  // Given: an early transmission of each player waits
  const OrderTransmitted of_a = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  const OrderTransmitted of_b = makeTransmission(PLAYER_B, OrderNumber{2}, StepId{21});
  State state = makeReception({of_b, of_a});

  // When: the round concludes
  auto [next_state, discarded] = transmission_reception_authority::conclude(state);

  // Then: both are reported as discarded, in order of receipt, and wait no longer
  EXPECT_EQ(discarded, (std::vector<DiscardedTransmission>{DiscardedTransmission{of_b}, DiscardedTransmission{of_a}}));
  EXPECT_TRUE(transmission_reception_authority::waiting(next_state).empty());
}

TEST(TransmissionReceptionAuthority, HandsOverNothingAfterConclusion) {
  // Given: a transmission waited when the round concluded
  const OrderTransmitted fourth = makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21});
  auto [state, discarded] = transmission_reception_authority::conclude(makeReception({fourth}));

  // When: its step is released
  auto [next_state, orders, late, superseded] = transmission_reception_authority::release(state, StepId{22});

  // Then: nothing is handed over
  EXPECT_EQ(orders, std::vector<Order>{});
  EXPECT_TRUE(late.empty());
  EXPECT_TRUE(superseded.empty());
}

TEST(TransmissionReceptionAuthority, DiscardsOrdersOnce) {
  // Given: a round concluded with a waiting transmission
  auto [state, first_discarded] = transmission_reception_authority::conclude(
      makeReception({makeTransmission(PLAYER_A, OrderNumber{4}, StepId{21})}));

  // When: it concludes again
  auto [next_state, discarded] = transmission_reception_authority::conclude(state);

  // Then: nothing is discarded again
  EXPECT_TRUE(discarded.empty());
}

}  // namespace platform
}  // namespace orbital
}  // namespace snake
