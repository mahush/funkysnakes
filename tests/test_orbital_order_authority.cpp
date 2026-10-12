// Tests for the Order Authority: submission, supersession, eligibility and order status.

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

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
  return Status::PENDING;
}

}  // namespace

/// @section order_authority::submit

TEST(OrderAuthority, AssignsNewIdentityToEachSubmission) {
  // Given: an order was submitted
  auto [state, first] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: the same player submits again
  auto [next_state, second] = order_authority::submit(state, Submission{PLAYER_A, Point{3, 4}, milliseconds{100}});

  // Then: the second order has its own identity
  EXPECT_NE(first, second);
}

/// @brief The deadline is fixed when the order is issued and taken from the Orbital Latency Policy
TEST(OrderAuthority, StoresDeadlineFromIssueTime) {
  // When: an order is submitted at the peak of the orbital cycle
  auto [state, id] = order_authority::submit(order_authority::startRound(ROUND),
                                             Submission{PLAYER_A, Point{3, 4}, milliseconds{15000}});

  // Then: its deadline lies the peak delay after its issue time
  EXPECT_EQ(order_authority::view(state).players.at(PLAYER_A).pending->deadline, milliseconds{18000});
}

/// @brief A player can change their mind; the superseded order never activates
TEST(OrderAuthority, SupersedesPendingOrderOfSamePlayer) {
  // Given: player A has a pending order G
  auto [state, g] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: player A submits H
  auto [next_state, h] = order_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, milliseconds{100}});

  // Then: G is superseded and H is the pending order
  EXPECT_EQ(statusOf(next_state, g), Status::SUPERSEDED);
  EXPECT_EQ(order_authority::view(next_state).players.at(PLAYER_A).pending->order.id, h);
}

TEST(OrderAuthority, KeepsPendingOrdersOfOtherPlayers) {
  // Given: player A has a pending order
  auto [state, a] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: player B submits an order
  auto [next_state, b] = order_authority::submit(state, Submission{PLAYER_B, Point{8, 1}, milliseconds{100}});

  // Then: both orders are pending
  EXPECT_EQ(statusOf(next_state, a), Status::PENDING);
  EXPECT_EQ(statusOf(next_state, b), Status::PENDING);
}

/// @section order_authority::eligible

TEST(OrderAuthority, HoldsBackOrderBeforeItsDeadline) {
  // Given: an order issued at 0 ms, eligible after 400 ms
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: eligibility is checked just before the deadline
  std::vector<Order> orders = order_authority::eligible(state, milliseconds{399});

  // Then: nothing is eligible
  EXPECT_TRUE(orders.empty());
}

TEST(OrderAuthority, ReleasesOrderOnceItsDeadlineHasElapsed) {
  // Given: an order issued at 0 ms, eligible after 400 ms
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: eligibility is checked at the deadline
  std::vector<Order> orders = order_authority::eligible(state, milliseconds{400});

  // Then: the order is eligible with its round, identity, player and target
  ASSERT_EQ(orders.size(), 1U);
  EXPECT_EQ(orders[0].round, ROUND);
  EXPECT_EQ(orders[0].id, id);
  EXPECT_EQ(orders[0].player, PLAYER_A);
  EXPECT_EQ(orders[0].target, (Point{3, 4}));
}

/// @brief Revising postpones activation: the new order waits for its own deadline, not the old one
TEST(OrderAuthority, DoesNotInheritDeadlineOfSupersededOrder) {
  // Given: G issued at 0 ms (eligible at 400 ms), superseded by H issued at 300 ms
  auto [state, g] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  auto [revised_state, h] = order_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, milliseconds{300}});

  // When: eligibility is checked after G's but before H's deadline
  std::vector<Order> orders = order_authority::eligible(revised_state, milliseconds{500});

  // Then: nothing is eligible
  EXPECT_TRUE(orders.empty());
}

/// @brief Asking is not consuming: only the Arena's outcome resolves a pending order
TEST(OrderAuthority, KeepsOrderPendingWhenEligibilityIsQueried) {
  // Given: an eligible order whose eligibility was queried
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::eligible(state, milliseconds{400});

  // When: eligibility is queried again
  std::vector<Order> orders = order_authority::eligible(state, milliseconds{400});

  // Then: the order is still eligible and pending
  EXPECT_EQ(orders.size(), 1U);
  EXPECT_EQ(statusOf(state, id), Status::PENDING);
}

/// @section order_authority::observe

/// @subsection activation outcomes

TEST(OrderAuthority, MarksActivatedOrderAsExecuting) {
  // Given: a pending order
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: the Arena reports its activation
  order_authority::State observed = order_authority::observe(state, {OrderActivated{id, std::nullopt, {}}}, {});

  // Then: it executes and is no longer pending
  EXPECT_EQ(statusOf(observed, id), Status::EXECUTING);
  EXPECT_EQ(order_authority::view(observed).players.at(PLAYER_A).executing->order.id, id);
  EXPECT_FALSE(order_authority::view(observed).players.at(PLAYER_A).pending.has_value());
}

TEST(OrderAuthority, RecordsReasonOfRejectedOrder) {
  // Given: a pending order
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: the Arena reports its rejection
  order_authority::State observed =
      order_authority::observe(state, {OrderRejected{id, RejectionReason::SNAKE_DEAD}}, {});

  // Then: it is rejected with the reported reason and no longer pending
  EXPECT_EQ(statusOf(observed, id), Status::REJECTED);
  EXPECT_EQ(order_authority::view(observed).orders.at(0).rejection, RejectionReason::SNAKE_DEAD);
  EXPECT_FALSE(order_authority::view(observed).players.at(PLAYER_A).pending.has_value());
}

/// @brief A refused replacement leaves the executing order in place, as in the world
TEST(OrderAuthority, KeepsExecutingOrderWhenReplacementIsRejected) {
  // Given: F executes and G is pending
  auto [state, f] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State executing = order_authority::observe(state, {OrderActivated{f, std::nullopt, {}}}, {});
  auto [pending_state, g] = order_authority::submit(executing, Submission{PLAYER_A, Point{8, 1}, milliseconds{500}});

  // When: the Arena rejects G
  order_authority::State observed =
      order_authority::observe(pending_state, {OrderRejected{g, RejectionReason::TARGET_OFF_BOARD}}, {});

  // Then: F still executes
  EXPECT_EQ(statusOf(observed, f), Status::EXECUTING);
}

/// @brief In a distributed realization H may still be in transit when G activates; the world decides
TEST(OrderAuthority, FollowsArenaWhenSupersededOrderWasActivated) {
  // Given: G was superseded by pending H
  auto [state, g] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  auto [revised_state, h] = order_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, milliseconds{300}});

  // When: the Arena reports that it activated G
  order_authority::State observed = order_authority::observe(revised_state, {OrderActivated{g, std::nullopt, {}}}, {});

  // Then: G executes and H stays pending
  EXPECT_EQ(statusOf(observed, g), Status::EXECUTING);
  EXPECT_EQ(order_authority::view(observed).players.at(PLAYER_A).pending->order.id, h);
}

TEST(OrderAuthority, IgnoresOutcomesOfUnknownOrders) {
  // Given: a pending order
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});

  // When: the Arena reports outcomes for an order this round never issued
  order_authority::State observed =
      order_authority::observe(state,
                               {OrderActivated{OrderId{42}, std::nullopt, {}}},
                               {OrderEnded{OrderId{42}, EndReason::COMPLETED, std::nullopt}});

  // Then: nothing changes
  EXPECT_EQ(order_authority::view(observed).orders.size(), 1U);
  EXPECT_EQ(statusOf(observed, id), Status::PENDING);
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
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State executing = order_authority::observe(state, {OrderActivated{id, std::nullopt, {}}}, {});

  // When: the Arena reports how it ended
  order_authority::State observed =
      order_authority::observe(executing, {}, {OrderEnded{id, GetParam().reason, std::nullopt}});

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
  // Given: F executes and G is pending
  auto [state, f] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State executing = order_authority::observe(state, {OrderActivated{f, std::nullopt, {}}}, {});
  auto [pending_state, g] = order_authority::submit(executing, Submission{PLAYER_A, Point{8, 1}, milliseconds{500}});

  // When: the Arena activates G, interrupting F, in one step
  order_authority::State observed =
      order_authority::observe(pending_state, {OrderActivated{g, f, {}}}, {OrderEnded{f, EndReason::INTERRUPTED, g}});

  // Then: F is interrupted and G executes
  EXPECT_EQ(statusOf(observed, f), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(observed, g), Status::EXECUTING);
  EXPECT_EQ(order_authority::view(observed).players.at(PLAYER_A).executing->order.id, g);
}

/// @brief F's completion stays F's, even while H is the latest intention
TEST(OrderAuthority, KeepsPendingOrderWhenExecutingOrderCompletes) {
  // Given: F executes and H is pending
  auto [state, f] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State executing = order_authority::observe(state, {OrderActivated{f, std::nullopt, {}}}, {});
  auto [pending_state, h] = order_authority::submit(executing, Submission{PLAYER_A, Point{8, 1}, milliseconds{500}});

  // When: the Arena reports that F completed
  order_authority::State observed =
      order_authority::observe(pending_state, {}, {OrderEnded{f, EndReason::COMPLETED, std::nullopt}});

  // Then: F is completed and H is still pending
  EXPECT_EQ(statusOf(observed, f), Status::COMPLETED);
  EXPECT_EQ(statusOf(observed, h), Status::PENDING);
}

/// @section order_authority::cancel

TEST(OrderAuthority, CancelsPendingOrders) {
  // Given: pending orders of both players
  auto [state, a] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  auto [both_state, b] = order_authority::submit(state, Submission{PLAYER_B, Point{8, 1}, milliseconds{0}});

  // When: the orders are cancelled
  order_authority::State cancelled = order_authority::cancel(both_state);

  // Then: both are cancelled and nothing is eligible any more
  EXPECT_EQ(statusOf(cancelled, a), Status::CANCELLED);
  EXPECT_EQ(statusOf(cancelled, b), Status::CANCELLED);
  EXPECT_TRUE(order_authority::eligible(cancelled, milliseconds{10000}).empty());
}

/// @brief In a distributed realization the Arena's report of the last step may arrive after the cancel
TEST(OrderAuthority, AppliesLateEndingOfExecutingOrderAfterCancel) {
  // Given: an executing order, and the pending orders were cancelled
  auto [state, f] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State cancelled =
      order_authority::cancel(order_authority::observe(state, {OrderActivated{f, std::nullopt, {}}}, {}));

  // When: the Arena reports that the order completed
  order_authority::State observed =
      order_authority::observe(cancelled, {}, {OrderEnded{f, EndReason::COMPLETED, std::nullopt}});

  // Then: the order is completed
  EXPECT_EQ(statusOf(observed, f), Status::COMPLETED);
}

/// @brief A cancelled order did not take part in the world; a late refusal does not rewrite that
TEST(OrderAuthority, KeepsCancelledOrderCancelledWhenRejectionArrivesLate) {
  // Given: a pending order was cancelled
  auto [state, id] =
      order_authority::submit(order_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, milliseconds{0}});
  order_authority::State cancelled = order_authority::cancel(state);

  // When: the Arena reports that it refused the order because the round ended
  order_authority::State observed =
      order_authority::observe(cancelled, {OrderRejected{id, RejectionReason::ROUND_ENDED}}, {});

  // Then: the order stays cancelled
  EXPECT_EQ(statusOf(observed, id), Status::CANCELLED);
}

}  // namespace orbital
}  // namespace snake
