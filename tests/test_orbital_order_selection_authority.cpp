// Tests for the Order Selection Authority: submission, replacement, transmission and cancellation.

#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <vector>

#include "orbital/order_selection_authority.hpp"

namespace snake {
namespace orbital {

namespace {

using order_selection_authority::Selection;
using order_selection_authority::Submission;

constexpr RoundId ROUND{1};

// Select a submission and keep only the new order's number
std::tuple<order_selection_authority::State, OrderNumber> submitOrder(order_selection_authority::State state,
                                                                      const Submission& submission) {
  auto [next, submitted] = order_selection_authority::submit(std::move(state), submission);
  return {std::move(next), submitted.order};
}

// Transmit the selections and keep only the state
order_selection_authority::State transmitAt(order_selection_authority::State state, StepId at) {
  return std::get<0>(order_selection_authority::transmit(std::move(state), at));
}

// A player's selection as projected by the Authority's view
std::optional<Selection> selectionOf(const order_selection_authority::State& state, const PlayerId& player) {
  const order_selection_authority::View view = order_selection_authority::view(state);
  if (auto selection = view.selections.find(player); selection != view.selections.end()) {
    return selection->second;
  }
  return std::nullopt;
}

}  // namespace

/// @section order_selection_authority::submit

TEST(OrderSelectionAuthority, AssignsNewNumberToEachSubmission) {
  // Given: an order was submitted
  auto [state, first] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: the same player submits again
  auto [next_state, second] = submitOrder(state, Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // Then: the second order has its own number
  EXPECT_NE(first, second);
}

/// @brief An order is identified by its player and its number, so each player numbers their own orders
TEST(OrderSelectionAuthority, NumbersOrdersPerPlayer) {
  // Given: player A submitted two orders
  auto [with_a1, a1] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [with_a2, a2] = submitOrder(with_a1, Submission{PLAYER_A, Point{5, 5}, StepId{0}});

  // When: player B submits their first order
  auto [state, b1] = submitOrder(with_a2, Submission{PLAYER_B, Point{8, 1}, StepId{0}});

  // Then: each player's orders are numbered from 1
  EXPECT_EQ(a1, OrderNumber{1});
  EXPECT_EQ(a2, OrderNumber{2});
  EXPECT_EQ(b1, OrderNumber{1});
}

TEST(OrderSelectionAuthority, SelectsSubmittedOrder) {
  // When: player A submits an order in step 6
  auto [state, id] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{6}});

  // Then: it is A's selection, with its submission step
  EXPECT_EQ(selectionOf(state, PLAYER_A),
            (Selection{Order{ROUND, id, PLAYER_A, Point{3, 4}}, StepId{6}, std::nullopt}));
}

/// @brief A player can change their mind as often as they like before a transmission
TEST(OrderSelectionAuthority, ReplacesSelectionOfSamePlayer) {
  // Given: player A selected G and then H
  auto [with_g, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [with_h, h] = submitOrder(with_g, Submission{PLAYER_A, Point{8, 1}, StepId{0}});

  // When: player A submits K
  auto [state, k] = submitOrder(with_h, Submission{PLAYER_A, Point{5, 5}, StepId{0}});

  // Then: K is A's selection and names H as the selection it replaced
  EXPECT_EQ(selectionOf(state, PLAYER_A), (Selection{Order{ROUND, k, PLAYER_A, Point{5, 5}}, StepId{0}, h}));
}

TEST(OrderSelectionAuthority, KeepsSelectionsOfOtherPlayers) {
  // Given: player A has a selection
  auto [state, a] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: player B submits an order
  auto [next_state, b] = submitOrder(state, Submission{PLAYER_B, Point{8, 1}, StepId{0}});

  // Then: both players have their selection
  EXPECT_EQ(selectionOf(next_state, PLAYER_A)->order.id, a);
  EXPECT_EQ(selectionOf(next_state, PLAYER_B)->order.id, b);
}

/// @brief The submission step is the domain fact the Game Authority relates to the Arena Authority's history
TEST(OrderSelectionAuthority, PublishesSubmissionWithItsStep) {
  // When: player A submits an order in step 6
  auto [state, submitted] = order_selection_authority::submit(order_selection_authority::startRound(ROUND),
                                                              Submission{PLAYER_A, Point{3, 4}, StepId{6}});

  // Then: the record names round, order, player and step, and replaces nothing
  EXPECT_EQ(submitted, (OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, StepId{6}, std::nullopt}));
}

TEST(OrderSelectionAuthority, PublishesReplacedSelectionWithSubmission) {
  // Given: player A has a selection G
  auto [state, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});

  // When: player A submits H
  auto [next_state, h] = order_selection_authority::submit(state, Submission{PLAYER_A, Point{8, 1}, StepId{0}});

  // Then: H's record names G as replaced
  EXPECT_EQ(h.replaced, g);
}

/// @brief Only a selection can be replaced; a transmitted order has left and cannot be recalled
TEST(OrderSelectionAuthority, ReplacesNothingAfterTransmission) {
  // Given: player A's G was transmitted in step 1
  auto [state, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  order_selection_authority::State transmitted = transmitAt(state, StepId{1});

  // When: player A submits H
  auto [next_state, h] = order_selection_authority::submit(transmitted, Submission{PLAYER_A, Point{8, 1}, StepId{1}});

  // Then: H replaces nothing
  EXPECT_FALSE(h.replaced.has_value());
}

/// @section order_selection_authority::transmit

TEST(OrderSelectionAuthority, TransmitsSelectionsOfAllPlayers) {
  // Given: A selected G in step 0, B selected K in step 15
  auto [with_g, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [state, k] = submitOrder(with_g, Submission{PLAYER_B, Point{5, 5}, StepId{15}});

  // When: the selections are transmitted in step 21
  auto [transmitted, transmissions] = order_selection_authority::transmit(state, StepId{21});

  // Then: both are transmitted, each keeping its own submission step, and nothing is selected any more
  EXPECT_EQ(transmissions,
            (std::vector<OrderTransmitted>{
                OrderTransmitted{ROUND, g, PLAYER_A, Point{3, 4}, StepId{0}, StepId{21}, std::nullopt},
                OrderTransmitted{ROUND, k, PLAYER_B, Point{5, 5}, StepId{15}, StepId{21}, std::nullopt}}));
  EXPECT_TRUE(order_selection_authority::view(transmitted).selections.empty());
}

/// @brief The latest replacement is what is sent, with its own submission step instead of the first selection's, and
/// it names the selection it replaced
TEST(OrderSelectionAuthority, TransmitsLatestReplacementWithItsOwnSubmissionStep) {
  // Given: player A selected G in step 0 and replaced it by H in step 15
  auto [with_g, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [state, h] = submitOrder(with_g, Submission{PLAYER_A, Point{8, 1}, StepId{15}});

  // When: the selections are transmitted in step 21
  auto [transmitted, transmissions] = order_selection_authority::transmit(state, StepId{21});

  // Then: only H is transmitted, with its submission step 15 and naming G
  EXPECT_EQ(
      transmissions,
      (std::vector<OrderTransmitted>{OrderTransmitted{ROUND, h, PLAYER_A, Point{8, 1}, StepId{15}, StepId{21}, g}}));
}

/// @brief An unchanged selection is sent once; a later opportunity sends nothing for it
TEST(OrderSelectionAuthority, TransmitsNothingWithoutSelection) {
  // Given: player A's selection was transmitted in step 1
  auto [state, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  order_selection_authority::State transmitted = transmitAt(state, StepId{1});

  // When: the next opportunity comes in step 21
  auto [next_state, transmissions] = order_selection_authority::transmit(transmitted, StepId{21});

  // Then: nothing is transmitted
  EXPECT_TRUE(transmissions.empty());
}

/// @brief A step's submissions come after the step's opportunity, so what is chosen there waits for the next one
TEST(OrderSelectionAuthority, KeepsSelectionSubmittedInTransmissionStep) {
  // Given: player A selected G in step 21
  auto [state, g] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{21}});

  // When: the selections are transmitted in step 21
  auto [next_state, transmissions] = order_selection_authority::transmit(state, StepId{21});

  // Then: nothing is transmitted and G stays selected
  EXPECT_TRUE(transmissions.empty());
  EXPECT_EQ(selectionOf(next_state, PLAYER_A)->order.id, g);
}

/// @section order_selection_authority::cancel

TEST(OrderSelectionAuthority, EndsEverySelection) {
  // Given: A and B have a selection
  auto [with_a, a] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  auto [state, b] = submitOrder(with_a, Submission{PLAYER_B, Point{8, 1}, StepId{0}});

  // When: the selections are cancelled
  order_selection_authority::State cancelled = order_selection_authority::cancel(state);

  // Then: no player has a selection, and the next opportunity transmits nothing
  EXPECT_TRUE(order_selection_authority::view(cancelled).selections.empty());
  EXPECT_TRUE(std::get<1>(order_selection_authority::transmit(cancelled, StepId{1})).empty());
}

/// @brief Numbers identify orders across the whole round, so a cancellation does not restart them
TEST(OrderSelectionAuthority, ContinuesNumberingAfterCancel) {
  // Given: player A submitted an order, and the selections were cancelled
  auto [state, first] =
      submitOrder(order_selection_authority::startRound(ROUND), Submission{PLAYER_A, Point{3, 4}, StepId{0}});
  order_selection_authority::State cancelled = order_selection_authority::cancel(state);

  // When: player A submits again
  auto [next_state, submitted] =
      order_selection_authority::submit(cancelled, Submission{PLAYER_A, Point{8, 1}, StepId{1}});

  // Then: the new order gets the next number and replaces nothing
  EXPECT_EQ(submitted, (OrderSubmitted{ROUND, OrderNumber{2}, PLAYER_A, StepId{1}, std::nullopt}));
}

}  // namespace orbital
}  // namespace snake
