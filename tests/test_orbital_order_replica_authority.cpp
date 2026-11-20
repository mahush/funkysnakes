// Tests for the Order Replica Authority: what the host knows of the players' orders from forwarded facts.

#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <vector>

#include "orbital/order_authority.hpp"
#include "orbital/order_replica_authority.hpp"

namespace snake {
namespace orbital {
namespace platform {

namespace {

using order_authority::OrderRecord;
using order_authority::Status;

constexpr RoundId ROUND{1};
const Point TARGET{3, 4};

Order orderOf(const PlayerId& player, int number, RoundId round = ROUND) {
  return Order{round, OrderNumber{number}, player, TARGET};
}

ForwardedSubmission submission(const PlayerId& player,
                               int number,
                               StepId submitted_at,
                               std::optional<int> replaced = std::nullopt,
                               RoundId round = ROUND) {
  std::optional<OrderNumber> replaced_number;
  if (replaced) {
    replaced_number = OrderNumber{*replaced};
  }
  return ForwardedSubmission{orderOf(player, number, round),
                             OrderSubmitted{round, OrderNumber{number}, player, submitted_at, replaced_number}};
}

ForwardedTransmission transmission(const PlayerId& player, int number, StepId submitted_at, StepId transmitted_at) {
  return ForwardedTransmission{
      orderOf(player, number),
      OrderTransmitted{ROUND, OrderNumber{number}, player, submitted_at, transmitted_at, std::nullopt}};
}

std::optional<OrderRecord> recordOf(const order_replica_authority::State& state, const PlayerId& player, int number) {
  for (const OrderRecord& record : order_replica_authority::view(state).orders) {
    if (record.order.player == player && record.order.id == OrderNumber{number}) {
      return record;
    }
  }
  return std::nullopt;
}

Status statusOf(const order_replica_authority::State& state, const PlayerId& player, int number) {
  std::optional<OrderRecord> record = recordOf(state, player, number);
  if (!record) {
    ADD_FAILURE() << "order " << player << " #" << number << " is not in the view";
    return Status::SELECTED;
  }
  return record->status;
}

order_replica_authority::State transmit(order_replica_authority::State state, const ForwardedTransmission& forwarded) {
  return std::get<0>(order_replica_authority::recordTransmission(std::move(state), forwarded));
}

// Player A's order 1, submitted in step 0 and transmitted at the opportunity in step 1, before step 2 ran
order_replica_authority::State transmittedOrder() {
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_A, 1, StepId{0}));
  state = order_replica_authority::observe(std::move(state), StepId{1}, {}, {});
  return transmit(std::move(state), transmission(PLAYER_A, 1, StepId{0}, StepId{1}));
}

void expectSameRecord(const std::optional<OrderRecord>& replica, const std::optional<OrderRecord>& order) {
  ASSERT_EQ(replica.has_value(), order.has_value());
  if (!replica) {
    return;
  }
  EXPECT_EQ(replica->order, order->order);
  EXPECT_EQ(replica->submitted_at, order->submitted_at);
  EXPECT_EQ(replica->transmitted_at, order->transmitted_at);
  EXPECT_EQ(replica->replaced, order->replaced);
  EXPECT_EQ(replica->status, order->status);
  EXPECT_EQ(replica->rejection, order->rejection);
}

}  // namespace

/// @section order_replica_authority::recordSubmission

TEST(OrderReplicaAuthority, SelectsForwardedSubmission) {
  // Given: a round on the host
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});

  // When: player A's submission arrives
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_A, 1, StepId{0}));

  // Then: it is player A's selected order
  order_authority::View view = order_replica_authority::view(state);
  ASSERT_TRUE(view.players.at(PLAYER_A).selected.has_value());
  EXPECT_EQ(view.players.at(PLAYER_A).selected->order, orderOf(PLAYER_A, 1));
  EXPECT_EQ(view.players.at(PLAYER_A).selected->submitted_at, StepId{0});
}

TEST(OrderReplicaAuthority, ReplacesTheOrderTheSubmissionNames) {
  // Given: player A's order 1 is selected
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_A, 1, StepId{0}));

  // When: order 2 arrives, naming order 1 as replaced
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_A, 2, StepId{0}, 1));

  // Then: order 1 is replaced and order 2 is selected
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::REPLACED);
  EXPECT_EQ(order_replica_authority::view(state).players.at(PLAYER_A).selected->order.id, OrderNumber{2});
}

TEST(OrderReplicaAuthority, IgnoresSubmissionOfAnotherRound) {
  // Given: a round on the host
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});

  // When: a submission of another round arrives
  state = order_replica_authority::recordSubmission(std::move(state),
                                                    submission(PLAYER_A, 1, StepId{0}, std::nullopt, RoundId{2}));

  // Then: nothing is recorded
  EXPECT_TRUE(order_replica_authority::view(state).orders.empty());
}

/// @section order_replica_authority::recordTransmission

TEST(OrderReplicaAuthority, TransmitsForwardedOrderWithItsOwnSteps) {
  // When: player A's selected order is transmitted at step 1
  order_replica_authority::State state = transmittedOrder();

  // Then: it is transmitted with its own steps and no longer selected
  order_authority::View view = order_replica_authority::view(state);
  EXPECT_FALSE(view.players.at(PLAYER_A).selected.has_value());
  ASSERT_TRUE(view.players.at(PLAYER_A).transmitted.has_value());
  EXPECT_EQ(view.players.at(PLAYER_A).transmitted->submitted_at, StepId{0});
  EXPECT_EQ(view.players.at(PLAYER_A).transmitted->transmitted_at, StepId{1});
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::TRANSMITTED);
}

TEST(OrderReplicaAuthority, HandsTransmittedOrderToStepAfterItsTransmission) {
  // Given: an order transmitted at step 1, arriving before step 2 ran
  order_replica_authority::State state = transmittedOrder();

  // When: the orders for steps 1 and 2 are queried
  // Then: it takes part from step 2 on
  EXPECT_TRUE(order_replica_authority::transmittedOrders(state, StepId{1}).empty());
  EXPECT_EQ(order_replica_authority::transmittedOrders(state, StepId{2}), std::vector<Order>{orderOf(PLAYER_A, 1)});
}

TEST(OrderReplicaAuthority, ReportsTimelyTransmissionAsNoDeviation) {
  // Given: step 1 ran
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});
  state = order_replica_authority::observe(std::move(state), StepId{1}, {}, {});

  // When: a transmission at step 1 arrives before step 2 ran
  auto [next, late] =
      order_replica_authority::recordTransmission(std::move(state), transmission(PLAYER_A, 1, StepId{0}, StepId{1}));

  // Then: no deviation is reported
  EXPECT_FALSE(late.has_value());
}

TEST(OrderReplicaAuthority, MovesLateTransmissionToFirstUnprocessedStepAndReportsIt) {
  // Given: steps 1 to 3 ran
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});
  for (int step = 1; step <= 3; ++step) {
    state = order_replica_authority::observe(std::move(state), StepId{step}, {}, {});
  }

  // When: a transmission at step 1, scheduled for step 2, arrives only now
  auto [next, late] =
      order_replica_authority::recordTransmission(std::move(state), transmission(PLAYER_A, 1, StepId{0}, StepId{1}));

  // Then: it takes part in step 4, keeps its own transmission step and is reported as a deviation
  EXPECT_TRUE(order_replica_authority::transmittedOrders(next, StepId{3}).empty());
  EXPECT_EQ(order_replica_authority::transmittedOrders(next, StepId{4}), std::vector<Order>{orderOf(PLAYER_A, 1)});
  EXPECT_EQ(recordOf(next, PLAYER_A, 1)->transmitted_at, StepId{1});
  EXPECT_EQ(late, (order_replica_authority::LateTransmission{PLAYER_A, OrderNumber{1}, StepId{1}, StepId{2}, StepId{4}}));
}

TEST(OrderReplicaAuthority, RecordsTransmissionWhoseSubmissionDidNotArrive) {
  // Given: a round on the host
  order_replica_authority::State state = order_replica_authority::startRound(ROUND, StepId{1});

  // When: a transmission arrives without its submission
  state = transmit(std::move(state), transmission(PLAYER_A, 1, StepId{0}, StepId{1}));

  // Then: the order is transmitted with the steps the transmission carries
  std::optional<OrderRecord> record = recordOf(state, PLAYER_A, 1);
  ASSERT_TRUE(record.has_value());
  EXPECT_EQ(record->status, Status::TRANSMITTED);
  EXPECT_EQ(record->submitted_at, StepId{0});
}

TEST(OrderReplicaAuthority, IgnoresRepeatedTransmission) {
  // Given: an order was transmitted and the Arena Authority activated it
  order_replica_authority::State state = transmittedOrder();
  state = order_replica_authority::observe(
      std::move(state), StepId{2}, {OrderActivated{OrderNumber{1}, PLAYER_A, std::nullopt, {}}}, {});

  // When: its transmission arrives again
  state = transmit(std::move(state), transmission(PLAYER_A, 1, StepId{0}, StepId{1}));

  // Then: it keeps executing and is not handed out again
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::EXECUTING);
  EXPECT_TRUE(order_replica_authority::transmittedOrders(state, StepId{3}).empty());
}

/// @section order_replica_authority::observe

/// @brief Known Issues #2, #3 and #6 need a resolved order handed to the Arena again; the replica never does that
TEST(OrderReplicaAuthority, NeverHandsOutResolvedOrderAgain) {
  // Given: a transmitted order
  order_replica_authority::State state = transmittedOrder();

  // When: the Arena Authority's activation of it is observed in step 2
  state = order_replica_authority::observe(
      std::move(state), StepId{2}, {OrderActivated{OrderNumber{1}, PLAYER_A, std::nullopt, {}}}, {});

  // Then: it executes and is not handed to any later step
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::EXECUTING);
  EXPECT_TRUE(order_replica_authority::transmittedOrders(state, StepId{3}).empty());
  EXPECT_TRUE(order_replica_authority::transmittedOrders(state, StepId{100}).empty());
}

TEST(OrderReplicaAuthority, ResolvesOrdersAsOrderResolutionPolicyDecides) {
  // Given: an executing order
  order_replica_authority::State state = transmittedOrder();
  state = order_replica_authority::observe(
      std::move(state), StepId{2}, {OrderActivated{OrderNumber{1}, PLAYER_A, std::nullopt, {}}}, {});

  // When: a refusal and then its completion are reported
  state = order_replica_authority::observe(
      std::move(state), StepId{3}, {OrderRejected{OrderNumber{1}, PLAYER_A, RejectionReason::SNAKE_DEAD}}, {});
  Status after_refusal = statusOf(state, PLAYER_A, 1);
  state = order_replica_authority::observe(
      std::move(state), StepId{4}, {}, {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::COMPLETED, std::nullopt}});

  // Then: the refusal changed nothing and the completion completed it
  EXPECT_EQ(after_refusal, Status::EXECUTING);
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::COMPLETED);
}

/// @section order_replica_authority::conclude

TEST(OrderReplicaAuthority, CancelsSelectedAndTransmittedOrdersAtConclusion) {
  // Given: player A has a transmitted order, player B a selected one
  order_replica_authority::State state = transmittedOrder();
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_B, 1, StepId{1}));

  // When: the game concludes
  state = order_replica_authority::conclude(std::move(state), {});

  // Then: both are cancelled
  EXPECT_EQ(statusOf(state, PLAYER_A, 1), Status::CANCELLED);
  EXPECT_EQ(statusOf(state, PLAYER_B, 1), Status::CANCELLED);
  EXPECT_TRUE(order_replica_authority::transmittedOrders(state, StepId{100}).empty());
}

TEST(OrderReplicaAuthority, IgnoresFactsAfterConclusion) {
  // Given: the game concluded
  order_replica_authority::State state =
      order_replica_authority::conclude(order_replica_authority::startRound(ROUND, StepId{1}), {});

  // When: a submission and a transmission arrive
  state = order_replica_authority::recordSubmission(std::move(state), submission(PLAYER_A, 1, StepId{5}));
  state = transmit(std::move(state), transmission(PLAYER_B, 1, StepId{4}, StepId{5}));

  // Then: nothing is recorded
  EXPECT_TRUE(order_replica_authority::view(state).orders.empty());
}

/// @section order_replica_authority::view

/// @brief For the facts an Order Authority published, the replica's view is the Order Authority's own
TEST(OrderReplicaAuthority, ViewsForwardedFactsAsTheOrderAuthorityDoes) {
  // Given: an Order Authority that selects, replaces, transmits and executes orders, with every fact forwarded
  order_authority::State orders = order_authority::startRound(ROUND);
  order_replica_authority::State replica = order_replica_authority::startRound(ROUND, StepId{1});
  auto forwardSubmission = [&](const PlayerId& player, Point target, StepId at) {
    auto [next, submitted] = order_authority::submit(std::move(orders), {player, target, at});
    orders = std::move(next);
    replica = order_replica_authority::recordSubmission(
        std::move(replica),
        ForwardedSubmission{Order{ROUND, submitted.order, player, target}, submitted});
  };
  forwardSubmission(PLAYER_A, Point{1, 1}, StepId{0});
  forwardSubmission(PLAYER_A, Point{2, 2}, StepId{0});
  forwardSubmission(PLAYER_B, Point{3, 3}, StepId{0});
  replica = order_replica_authority::observe(std::move(replica), StepId{1}, {}, {});
  auto [transmitted_orders, transmissions] = order_authority::transmit(std::move(orders), StepId{1});
  orders = std::move(transmitted_orders);
  for (const OrderTransmitted& transmitted : transmissions) {
    const Point target = transmitted.player == PLAYER_A ? Point{2, 2} : Point{3, 3};
    replica = transmit(std::move(replica),
                       ForwardedTransmission{Order{ROUND, transmitted.order, transmitted.player, target}, transmitted});
  }
  forwardSubmission(PLAYER_A, Point{4, 4}, StepId{1});
  std::vector<ActivationOutcome> outcomes{OrderActivated{OrderNumber{2}, PLAYER_A, std::nullopt, {}},
                                          OrderRejected{OrderNumber{1}, PLAYER_B, RejectionReason::TARGET_OFF_BOARD}};

  // When: both observe the Arena Authority's outcomes of step 2
  orders = order_authority::observe(std::move(orders), outcomes, {});
  replica = order_replica_authority::observe(std::move(replica), StepId{2}, outcomes, {});

  // Then: both views agree, order by order and per player
  order_authority::View expected = order_authority::view(orders);
  order_authority::View actual = order_replica_authority::view(replica);
  ASSERT_EQ(actual.orders.size(), expected.orders.size());
  for (std::size_t i = 0; i < expected.orders.size(); ++i) {
    expectSameRecord(actual.orders[i], expected.orders[i]);
  }
  ASSERT_EQ(actual.players.size(), expected.players.size());
  for (const auto& [player, situation] : expected.players) {
    expectSameRecord(actual.players.at(player).selected, situation.selected);
    expectSameRecord(actual.players.at(player).transmitted, situation.transmitted);
    expectSameRecord(actual.players.at(player).executing, situation.executing);
  }
}

}  // namespace platform
}  // namespace orbital
}  // namespace snake
