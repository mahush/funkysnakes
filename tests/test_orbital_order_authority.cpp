// Tests for the Order Authority: submission, replacement, transmission and order status.

#include <gtest/gtest.h>

#include <tuple>
#include <vector>

#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {

namespace {

using order_authority::Status;
using order_authority::Submission;

constexpr RoundId ROUND{1};

// Status of one order as projected by the Authority's view
Status statusOf(const order_authority::State& state, OrderId id) {
  for (const order_authority::OrderRecord& record : order_authority::view(state).orders) {
    if (record.order.id == id) {
      return record.status;
    }
  }
  ADD_FAILURE() << "order " << id.value << " is not in the view";
  return Status::SELECTED;
}

// Select a submission and keep only the new order's identity
std::tuple<order_authority::State, OrderId> submitOrder(order_authority::State state, const Submission& submission) {
  auto [next, submitted] = order_authority::submit(std::move(state), submission);
  return {std::move(next), submitted.order};
}

// Transmit the selected orders and keep only the state
order_authority::State transmitAt(order_authority::State state, StepId at) {
  return std::get<0>(order_authority::transmit(std::move(state), at));
}

// An order of player A that was submitted in step 0 and transmitted in step 1
std::tuple<order_authority::State, OrderId> transmittedOrder() {
  auto [state, id] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  return {transmitAt(std::move(state), StepId{1}), id};
}

// An order of player A that the Arena Authority activated after its transmission
std::tuple<order_authority::State, OrderId> executingOrder() {
  auto [state, id] = transmittedOrder();
  return {order_authority::observe(std::move(state), {OrderActivated{id, PLAYER_A, std::nullopt, {}}}, {}), id};
}

}  // namespace

/// @section order_authority::submit

TEST(OrderAuthority, AssignsNewIdentityToEachSubmission) {
  // Given: an order was submitted
  auto [state, first] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: the same player submits again
  auto [next_state, second] = submitOrder(state, Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // Then: the second order has its own identity
  EXPECT_NE(first, second);
}

TEST(OrderAuthority, SelectsSubmittedOrder) {
  // When: player A submits an order
  auto [state, id] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // Then: it is A's selected order
  EXPECT_EQ(statusOf(state, id), Status::SELECTED);
  EXPECT_EQ(order_authority::view(state).players.at(PLAYER_A).selected->order.id, id);
}

/// @brief A player can change their mind as often as they like before a transmission
TEST(OrderAuthority, ReplacesSelectedOrderOfSamePlayer) {
  // Given: player A selected G and then H
  auto [with_g, g] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [with_h, h] = submitOrder(with_g, Submission{PLAYER_A, Point{8, 1}, StepId{0}});

  // When: player A submits K
  auto [state, k] = submitOrder(with_h, Submission{PLAYER_A, Point{5, 5}, StepId{0}});

  // Then: G and H are replaced and K is the selected order
  EXPECT_EQ(statusOf(state, g), Status::REPLACED);
  EXPECT_EQ(statusOf(state, h), Status::REPLACED);
  EXPECT_EQ(order_authority::view(state).players.at(PLAYER_A).selected->order.id, k);
}

TEST(OrderAuthority, KeepsSelectedOrdersOfOtherPlayers) {
  // Given: player A has a selected order
  auto [state, a] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: player B submits an order
  auto [next_state, b] = submitOrder(state, Submission{PLAYER_B, Point{8, 1}, StepId{0}});

  // Then: both orders are selected
  EXPECT_EQ(statusOf(next_state, a), Status::SELECTED);
  EXPECT_EQ(statusOf(next_state, b), Status::SELECTED);
}

/// @brief The submission step is the domain fact the Game Authority relates to the Arena Authority's history
TEST(OrderAuthority, PublishesSubmissionWithItsStep) {
  // When: player A submits an order in step 6
  auto [state, submitted] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{6}});

  // Then: the record names round, order, player and step, and replaces nothing
  EXPECT_EQ(submitted,
            (OrderSubmitted{ROUND,
                            order_authority::view(state).players.at(PLAYER_A).selected->order.id,
                            PLAYER_A,
                            StepId{6},
                            std::nullopt}));
}

TEST(OrderAuthority, PublishesReplacedOrderWithSubmission) {
  // Given: player A has a selected order G
  auto [state, g] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: player A submits H
  auto [next_state, h] = order_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, StepId{0}});

  // Then: H's record names G as replaced
  EXPECT_EQ(h.replaced, g);
}

/// @brief Only a selected order can be replaced; a transmitted one has left and cannot be recalled
TEST(OrderAuthority, KeepsTransmittedOrderOnNewSubmission) {
  // Given: G was transmitted
  auto [state, g] = transmittedOrder();

  // When: player A submits H
  auto [next_state, h] = order_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, StepId{1}});

  // Then: G stays transmitted and H replaces nothing
  EXPECT_EQ(statusOf(next_state, g), Status::TRANSMITTED);
  EXPECT_FALSE(h.replaced.has_value());
}

/// @section order_authority::transmit

TEST(OrderAuthority, TransmitsSelectedOrdersOfAllPlayers) {
  // Given: A selected G, B selected K
  auto [with_g, g] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [state, k] = submitOrder(with_g, Submission{PLAYER_B, Point{5, 5}, StepId{15}});

  // When: the selected orders are transmitted in step 21
  auto [transmitted, transmissions] = order_authority::transmit(state, StepId{21});

  // Then: both are transmitted, each keeping its own submission step, and nothing is selected any more
  EXPECT_EQ(
      transmissions,
      (std::vector<OrderTransmitted>{OrderTransmitted{ROUND, g, PLAYER_A, StepId{0}, StepId{21}, std::nullopt},
                                     OrderTransmitted{ROUND, k, PLAYER_B, StepId{15}, StepId{21}, std::nullopt}}));
  EXPECT_EQ(statusOf(transmitted, g), Status::TRANSMITTED);
  EXPECT_FALSE(order_authority::view(transmitted).players.at(PLAYER_A).selected.has_value());
}

/// @brief The latest replacement is what is sent, with its own submission step instead of the first selection's, and
/// it names the selection it replaced
TEST(OrderAuthority, TransmitsLatestReplacementWithItsOwnSubmissionStep) {
  // Given: player A selected G in step 0 and replaced it by H in step 15
  auto [with_g, g] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [state, h] = submitOrder(with_g, Submission{PLAYER_A, Point{8, 1}, StepId{15}});

  // When: the selected orders are transmitted in step 21
  auto [transmitted, transmissions] = order_authority::transmit(state, StepId{21});

  // Then: only H is transmitted, with its submission step 15 and naming G; G stays replaced
  EXPECT_EQ(transmissions,
            (std::vector<OrderTransmitted>{OrderTransmitted{ROUND, h, PLAYER_A, StepId{15}, StepId{21}, g}}));
  EXPECT_EQ(statusOf(transmitted, g), Status::REPLACED);
}

/// @brief An unchanged selection is sent once; a later opportunity sends nothing for it
TEST(OrderAuthority, TransmitsNothingWithoutSelectedOrder) {
  // Given: player A's order was transmitted in step 1
  auto [state, g] = transmittedOrder();

  // When: the next opportunity comes in step 21
  auto [next_state, transmissions] = order_authority::transmit(state, StepId{21});

  // Then: nothing is transmitted
  EXPECT_TRUE(transmissions.empty());
}

/// @brief A step's submissions come after the step's opportunity, so what is chosen there waits for the next one
TEST(OrderAuthority, KeepsOrderSubmittedInTransmissionStepSelected) {
  // Given: player A selected G in step 21
  auto [state, g] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{21}});

  // When: the selected orders are transmitted in step 21
  auto [next_state, transmissions] = order_authority::transmit(state, StepId{21});

  // Then: nothing is transmitted and G stays selected
  EXPECT_TRUE(transmissions.empty());
  EXPECT_EQ(statusOf(next_state, g), Status::SELECTED);
}

/// @section order_authority::transmittedOrders

TEST(OrderAuthority, ListsTransmittedOrdersButNotSelectedOnes) {
  // Given: A's G was transmitted, then A selected H
  auto [state, g] = transmittedOrder();
  auto [with_h, h] = submitOrder(state, Submission{PLAYER_A, Point{8, 1}, StepId{1}});

  // When: the transmitted orders are queried
  std::vector<Order> transmitted = order_authority::transmittedOrders(with_h, StepId{2});

  // Then: only G is listed
  ASSERT_EQ(transmitted.size(), 1U);
  EXPECT_EQ(transmitted.front().id, g);
}

/// @brief An order sent at the opportunity in a step activates in a later step, after that step's completions
TEST(OrderAuthority, WithholdsOrderFromItsTransmissionStep) {
  // Given: A's G was transmitted in step 1
  auto [state, g] = transmittedOrder();

  // When: the transmitted orders are queried for step 1
  std::vector<Order> transmitted = order_authority::transmittedOrders(state, StepId{1});

  // Then: none is listed
  EXPECT_TRUE(transmitted.empty());
}

TEST(OrderAuthority, DropsActivatedOrderFromTransmittedOrders) {
  // Given: a transmitted order the Arena Authority activated
  auto [observed, id] = executingOrder();

  // When: the transmitted orders are queried
  std::vector<Order> transmitted = order_authority::transmittedOrders(observed, StepId{3});

  // Then: none is listed
  EXPECT_TRUE(transmitted.empty());
}

/// @section order_authority::observe

/// @subsection activation outcomes

TEST(OrderAuthority, MarksActivatedOrderAsExecuting) {
  // Given: a transmitted order
  auto [state, id] = transmittedOrder();

  // When: the Arena Authority reports its activation
  order_authority::State observed =
      order_authority::observe(state, {OrderActivated{id, PLAYER_A, std::nullopt, {}}}, {});

  // Then: it executes and is no longer transmitted
  EXPECT_EQ(statusOf(observed, id), Status::EXECUTING);
  EXPECT_EQ(order_authority::view(observed).players.at(PLAYER_A).executing->order.id, id);
  EXPECT_FALSE(order_authority::view(observed).players.at(PLAYER_A).transmitted.has_value());
}

TEST(OrderAuthority, RecordsReasonOfRejectedOrder) {
  // Given: a transmitted order
  auto [state, id] = transmittedOrder();

  // When: the Arena Authority reports its rejection
  order_authority::State observed =
      order_authority::observe(state, {OrderRejected{id, PLAYER_A, RejectionReason::SNAKE_DEAD}}, {});

  // Then: it is rejected with the reported reason and no longer transmitted
  EXPECT_EQ(statusOf(observed, id), Status::REJECTED);
  EXPECT_EQ(order_authority::view(observed).orders.at(0).rejection, RejectionReason::SNAKE_DEAD);
  EXPECT_FALSE(order_authority::view(observed).players.at(PLAYER_A).transmitted.has_value());
}

/// @brief A refused replacement leaves the executing order in place, as in the world
TEST(OrderAuthority, KeepsExecutingOrderWhenReplacementIsRejected) {
  // Given: F executes and G was transmitted
  auto [executing, f] = executingOrder();
  auto [selected, g] = submitOrder(executing, Submission{PLAYER_A, Point{8, 1}, StepId{2}});
  order_authority::State state = transmitAt(selected, StepId{21});

  // When: the Arena Authority rejects G
  order_authority::State observed =
      order_authority::observe(state, {OrderRejected{g, PLAYER_A, RejectionReason::TARGET_OFF_BOARD}}, {});

  // Then: F still executes
  EXPECT_EQ(statusOf(observed, f), Status::EXECUTING);
}

/// @brief Only a transmitted order can be refused; a selected one never reached the Arena Authority
TEST(OrderAuthority, KeepsSelectedOrderWhenRejectionIsReported) {
  // Given: a selected order
  auto [state, id] = submitOrder(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: the Arena Authority reports a rejection for it
  order_authority::State observed =
      order_authority::observe(state, {OrderRejected{id, PLAYER_A, RejectionReason::TARGET_OFF_BOARD}}, {});

  // Then: it stays selected
  EXPECT_EQ(statusOf(observed, id), Status::SELECTED);
}

TEST(OrderAuthority, IgnoresOutcomesOfUnknownOrders) {
  // Given: a transmitted order
  auto [state, id] = transmittedOrder();

  // When: the Arena Authority reports outcomes for an order this round never issued
  order_authority::State observed =
      order_authority::observe(state,
                               {OrderActivated{OrderId{42}, PLAYER_A, std::nullopt, {}}},
                               {OrderEnded{OrderId{42}, PLAYER_A, EndReason::COMPLETED, std::nullopt}});

  // Then: nothing changes
  EXPECT_EQ(order_authority::view(observed).orders.size(), 1U);
  EXPECT_EQ(statusOf(observed, id), Status::TRANSMITTED);
}

/// @subsection order endings

namespace {

struct EndingCase {
  EndReason reason;
  Status expected_status;
};

class OrderAuthorityEnding : public ::testing::TestWithParam<EndingCase> {};

}  // namespace

TEST_P(OrderAuthorityEnding, RecordsHowExecutingOrderEnded) {
  // Given: an executing order
  auto [executing, id] = executingOrder();

  // When: the Arena Authority reports how it ended
  order_authority::State observed =
      order_authority::observe(executing, {}, {OrderEnded{id, PLAYER_A, GetParam().reason, std::nullopt}});

  // Then: its status reflects the ending, and the player has no executing order
  EXPECT_EQ(statusOf(observed, id), GetParam().expected_status);
  EXPECT_FALSE(order_authority::view(observed).players.at(PLAYER_A).executing.has_value());
}

INSTANTIATE_TEST_SUITE_P(EndReasons,
                         OrderAuthorityEnding,
                         ::testing::Values(EndingCase{EndReason::COMPLETED, Status::COMPLETED},
                                           EndingCase{EndReason::INTERRUPTED, Status::INTERRUPTED},
                                           EndingCase{EndReason::ELIMINATED, Status::FAILED},
                                           EndingCase{EndReason::ROUND_ENDED, Status::ENDED_BY_ROUND}));

/// @brief Within one step, the old order is interrupted at activation of its replacement
TEST(OrderAuthority, InterpretsReplacementAndInterruptionOfOneStep) {
  // Given: F executes and G was transmitted
  auto [executing, f] = executingOrder();
  auto [selected, g] = submitOrder(executing, Submission{PLAYER_A, Point{8, 1}, StepId{2}});
  order_authority::State state = transmitAt(selected, StepId{21});

  // When: the Arena Authority activates G, interrupting F, in one step
  order_authority::State observed = order_authority::observe(
      state, {OrderActivated{g, PLAYER_A, f, {}}}, {OrderEnded{f, PLAYER_A, EndReason::INTERRUPTED, g}});

  // Then: F is interrupted and G executes
  EXPECT_EQ(statusOf(observed, f), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(observed, g), Status::EXECUTING);
  EXPECT_EQ(order_authority::view(observed).players.at(PLAYER_A).executing->order.id, g);
}

/// @brief F's completion stays F's, even while H is the player's selected intention
TEST(OrderAuthority, KeepsSelectedOrderWhenExecutingOrderCompletes) {
  // Given: F executes and H is selected
  auto [executing, f] = executingOrder();
  auto [state, h] = submitOrder(executing, Submission{PLAYER_A, Point{8, 1}, StepId{2}});

  // When: the Arena Authority reports that F completed
  order_authority::State observed =
      order_authority::observe(state, {}, {OrderEnded{f, PLAYER_A, EndReason::COMPLETED, std::nullopt}});

  // Then: F is completed and H is still selected
  EXPECT_EQ(statusOf(observed, f), Status::COMPLETED);
  EXPECT_EQ(statusOf(observed, h), Status::SELECTED);
}

/// @subsection world events

namespace {

class OrderAuthorityWorldEvent : public ::testing::TestWithParam<ArenaEvent> {};

}  // namespace

/// @brief Collection and collisions carry no order status; an elimination arrives as its own OrderEnded
TEST_P(OrderAuthorityWorldEvent, KeepsOrderStatusOnWorldEvent) {
  // Given: an executing order
  auto [executing, id] = executingOrder();

  // When: the Arena Authority reports a world event of that player
  order_authority::State observed = order_authority::observe(executing, {}, {GetParam()});

  // Then: the order still executes
  EXPECT_EQ(statusOf(observed, id), Status::EXECUTING);
}

INSTANTIATE_TEST_SUITE_P(WorldEvents,
                         OrderAuthorityWorldEvent,
                         ::testing::Values(ArenaEvent{FoodCollected{PLAYER_A, Point{3, 4}, OrderId{1}}},
                                           ArenaEvent{Bitten{PLAYER_A, PLAYER_B}},
                                           ArenaEvent{SelfBitten{PLAYER_A}},
                                           ArenaEvent{MutualBite{PLAYER_A, PLAYER_B}}));

/// @section order_authority::cancel

TEST(OrderAuthority, CancelsSelectedAndTransmittedOrders) {
  // Given: A's order was transmitted, B's order is selected
  auto [state, a] = transmittedOrder();
  auto [both_state, b] = submitOrder(state, Submission{PLAYER_B, Point{8, 1}, StepId{1}});

  // When: the orders are cancelled
  order_authority::State cancelled = order_authority::cancel(both_state);

  // Then: both are cancelled and none is transmitted or selected any more
  EXPECT_EQ(statusOf(cancelled, a), Status::CANCELLED);
  EXPECT_EQ(statusOf(cancelled, b), Status::CANCELLED);
  EXPECT_TRUE(order_authority::transmittedOrders(cancelled, StepId{3}).empty());
  EXPECT_FALSE(order_authority::view(cancelled).players.at(PLAYER_B).selected.has_value());
}

/// @brief In a distributed realization the Arena Authority's report of the last step may arrive after the cancel
TEST(OrderAuthority, AppliesLateEndingOfExecutingOrderAfterCancel) {
  // Given: an executing order, and the remaining orders were cancelled
  auto [executing, f] = executingOrder();
  order_authority::State cancelled = order_authority::cancel(executing);

  // When: the Arena Authority reports that the order completed
  order_authority::State observed =
      order_authority::observe(cancelled, {}, {OrderEnded{f, PLAYER_A, EndReason::COMPLETED, std::nullopt}});

  // Then: the order is completed
  EXPECT_EQ(statusOf(observed, f), Status::COMPLETED);
}

/// @brief A cancelled order did not take part in the world; a late refusal does not rewrite that
TEST(OrderAuthority, KeepsCancelledOrderCancelledWhenRejectionArrivesLate) {
  // Given: a transmitted order was cancelled
  auto [state, id] = transmittedOrder();
  order_authority::State cancelled = order_authority::cancel(state);

  // When: the Arena Authority reports that it refused the order because the round ended
  order_authority::State observed =
      order_authority::observe(cancelled, {OrderRejected{id, PLAYER_A, RejectionReason::ROUND_ENDED}}, {});

  // Then: the order stays cancelled
  EXPECT_EQ(statusOf(observed, id), Status::CANCELLED);
}

}  // namespace orbital
}  // namespace snake
