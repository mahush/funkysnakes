// Tests for the Orbital Arena Authority: activation, movement along routes, collection and completion.

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

#include "orbital/arena_authority.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {

namespace {

using arena_authority::RoundSetup;
using arena_authority::SnakeSetup;
using arena_authority::State;

constexpr RoundId ROUND{1};
const Board BOARD{20, 10};

/**
 * @brief Deterministic random source replaying a fixed sequence of values
 */
RandomIntGeneratorFn makeScriptedRandom(std::vector<int> values) {
  auto queue = std::make_shared<std::deque<int>>(values.begin(), values.end());
  return [queue](int /* min */, int /* max */) {
    int value = queue->front();
    queue->pop_front();
    return value;
  };
}

/**
 * @brief Random source that fails the test when consulted
 */
RandomIntGeneratorFn noRandom() {
  return [](int /* min */, int /* max */) -> int {
    ADD_FAILURE() << "unexpected random draw";
    return 0;
  };
}

/**
 * @brief Arena of round ROUND on BOARD with Player A's snake and the given food
 *
 * Filler food is added in row 9 (x >= 15) up to MIN_FOOD_COUNT so that replenishment does not
 * draw random positions unless food is collected.
 */
State arena(SnakeSetup snake_a, FoodItems food) {
  for (int i = static_cast<int>(food.size()); i < arena_authority::MIN_FOOD_COUNT; ++i) {
    food.push_back(Point{15 + i, 9});
  }
  return arena_authority::startRound(noRandom(), RoundSetup{ROUND, BOARD, {{PLAYER_A, snake_a}}, food});
}

/**
 * @brief Arena with the snakes of Player A and Player B, filled up with food as above
 */
State arena(SnakeSetup snake_a, SnakeSetup snake_b, FoodItems food) {
  for (int i = static_cast<int>(food.size()); i < arena_authority::MIN_FOOD_COUNT; ++i) {
    food.push_back(Point{15 + i, 9});
  }
  return arena_authority::startRound(noRandom(),
                                     RoundSetup{ROUND, BOARD, {{PLAYER_A, snake_a}, {PLAYER_B, snake_b}}, food});
}

Order orderOfA(int id, Point target) { return Order{ROUND, OrderId{id}, PLAYER_A, target}; }

Point headOfA(const State& state) { return snake_model::head(state.snakes.at(PLAYER_A)); }

}  // namespace

/// @section arena_authority::startRound

TEST(OrbitalArenaAuthority, TopsUpFoodToMinimumAtRoundStart) {
  // When: a round starts with one food item
  State state = arena_authority::startRound(
      makeScriptedRandom({1, 1, 2, 1, 3, 1, 4, 1}),
      RoundSetup{ROUND, BOARD, {{PLAYER_A, SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}}}, {Point{0, 0}}});

  // Then: the board holds the minimum amount of food
  EXPECT_EQ(state.food.size(), static_cast<size_t>(arena_authority::MIN_FOOD_COUNT));
}

/// @section arena_authority::tick

/// @subsection movement

TEST(OrbitalArenaAuthority, MovesStraightOnWithoutExecutingOrder) {
  // Given: a snake heading right without an order
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {});

  // When: a step passes
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {});

  // Then: the snake moved one cell right
  EXPECT_EQ(headOfA(next), (Point{6, 5}));
  EXPECT_TRUE(outcomes.empty());
  EXPECT_TRUE(events.empty());
}

/// @brief Activation happens before movement, so the snake already turns on the activation step
TEST(OrbitalArenaAuthority, MovesAlongRouteFromActivationStep) {
  // Given: a snake heading right at (5,5)
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {});

  // When: an order to (5,2) activates
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {orderOfA(1, Point{5, 2})});

  // Then: the route turns up at once and the snake moved up
  ASSERT_EQ(outcomes.size(), 1U);
  EXPECT_EQ(std::get<OrderActivated>(outcomes[0]),
            (OrderActivated{OrderId{1}, std::nullopt, {Direction::UP, Direction::UP, Direction::UP}}));
  EXPECT_EQ(headOfA(next), (Point{5, 4}));
}

/// @subsection completion

TEST(OrbitalArenaAuthority, CompletesOrderWhenHeadReachesTarget) {
  // Given: an executing order to (7,5), two cells ahead
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{7, 5})});

  // When: the next step passes
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {});

  // Then: the order completed on arrival and no order executes any more
  EXPECT_EQ(headOfA(next), (Point{7, 5}));
  EXPECT_EQ(next_events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}));
  EXPECT_TRUE(next.active.empty());
}

TEST(OrbitalArenaAuthority, MovesStraightOnAfterCompletion) {
  // Given: an order to (6,5) completed on the activation step
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{6, 5})});

  // When: the next step passes
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {});

  // Then: the snake continued straight on
  EXPECT_EQ(headOfA(next), (Point{7, 5}));
  EXPECT_TRUE(next_events.empty());
}

/// @brief A head already at the target completes before movement; the snake then moves straight on
TEST(OrbitalArenaAuthority, CompletesOrderAtActivationWhenHeadIsAtTarget) {
  // Given: a snake with its head at (5,5)
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {});

  // When: an order to (5,5) activates
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {orderOfA(1, Point{5, 5})});

  // Then: it activated with an empty route, completed, and the snake moved on
  EXPECT_EQ(outcomes, (std::vector<ActivationOutcome>{OrderActivated{OrderId{1}, std::nullopt, {}}}));
  EXPECT_EQ(events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}));
  EXPECT_EQ(headOfA(next), (Point{6, 5}));
}

/// @brief Food collected after an immediate completion belongs to no order
TEST(OrbitalArenaAuthority, ReportsCompletionAtActivationBeforeCollectionWithoutOrder) {
  // Given: a snake at (5,5) with food at (6,5)
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {Point{6, 5}});

  // When: an order to (5,5) activates
  auto [next, outcomes, events] = arena_authority::tick(makeScriptedRandom({0, 0}), state, {orderOfA(1, Point{5, 5})});

  // Then: completion comes first, then collection without an executing order
  EXPECT_EQ(events,
            (ArenaEvents{OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt},
                         FoodCollected{PLAYER_A, Point{6, 5}, std::nullopt}}));
}

/// @subsection collection

TEST(OrbitalArenaAuthority, AttributesCollectionToExecutingOrder) {
  // Given: an executing order to (9,5) with food on the way at (7,5)
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {Point{7, 5}}), {orderOfA(1, Point{9, 5})});

  // When: the next step passes
  auto [next, next_outcomes, next_events] = arena_authority::tick(makeScriptedRandom({0, 0}), state, {});

  // Then: the food was collected during the order, and the snake grew
  EXPECT_EQ(next_events, (ArenaEvents{FoodCollected{PLAYER_A, Point{7, 5}, OrderId{1}}}));
  EXPECT_EQ(snake_model::length(next.snakes.at(PLAYER_A)), 4U);
}

TEST(OrbitalArenaAuthority, ReportsCollectionWithoutOrderWhenNoneExecutes) {
  // Given: a snake without an order and food ahead
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {Point{6, 5}});

  // When: a step passes
  auto [next, outcomes, events] = arena_authority::tick(makeScriptedRandom({0, 0}), state, {});

  // Then: the collection belongs to no order
  EXPECT_EQ(events, (ArenaEvents{FoodCollected{PLAYER_A, Point{6, 5}, std::nullopt}}));
}

/// @brief Collecting food at the target belongs to the order, and comes before its completion
TEST(OrbitalArenaAuthority, ReportsCollectionAtTargetBeforeCompletion) {
  // Given: a snake at (5,5) with food at (6,5)
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {Point{6, 5}});

  // When: an order to (6,5) activates
  auto [next, outcomes, events] = arena_authority::tick(makeScriptedRandom({0, 0}), state, {orderOfA(1, Point{6, 5})});

  // Then: the food is collected during the order, then the order completes
  EXPECT_EQ(events,
            (ArenaEvents{FoodCollected{PLAYER_A, Point{6, 5}, OrderId{1}},
                         OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}));
}

/// @brief Food vanishing at the target does not invalidate a positional order
TEST(OrbitalArenaAuthority, CompletesOrderAtTargetWithoutFood) {
  // Given: an executing order to (7,5) where no food lies
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{7, 5})});

  // When: the snake arrives
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {});

  // Then: the order completes
  EXPECT_EQ(next_events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}));
}

/// @subsection replacement

/// @brief A replacement does not wait for the executing order's destination
TEST(OrbitalArenaAuthority, InterruptsExecutingOrderWithReplacement) {
  // Given: F to (12,5) executes
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{12, 5})});

  // When: G to (6,2) activates
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {orderOfA(2, Point{6, 2})});

  // Then: G replaces F, F is interrupted by G, and the snake follows G
  ASSERT_EQ(next_outcomes.size(), 1U);
  EXPECT_EQ(std::get<OrderActivated>(next_outcomes[0]).replaced, OrderId{1});
  EXPECT_EQ(next_events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}}));
  EXPECT_EQ(headOfA(next), (Point{6, 4}));
  EXPECT_EQ(next.active.at(PLAYER_A).id, OrderId{2});
}

/// @brief The replacement activates before movement, so food on that step belongs to the replacement
TEST(OrbitalArenaAuthority, ReportsInterruptionBeforeCollectionUnderReplacement) {
  // Given: F to (12,5) executes and food lies at (7,5)
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {Point{7, 5}}), {orderOfA(1, Point{12, 5})});

  // When: G to (9,5) activates on the step that reaches the food
  auto [next, next_outcomes, next_events] =
      arena_authority::tick(makeScriptedRandom({0, 0}), state, {orderOfA(2, Point{9, 5})});

  // Then: F is interrupted first, then the food is collected under G
  EXPECT_EQ(next_events,
            (ArenaEvents{OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}},
                         FoodCollected{PLAYER_A, Point{7, 5}, OrderId{2}}}));
}

/// @brief An eligible replacement on the arrival step interrupts the old order before it can arrive
TEST(OrbitalArenaAuthority, InterruptsOrderThatWouldArriveOnActivationStep) {
  // Given: F to (7,5) executes and would arrive on the next step
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{7, 5})});

  // When: G to (7,2) activates on that step
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {orderOfA(2, Point{7, 2})});

  // Then: F is interrupted, not completed
  EXPECT_EQ(next_events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}}));
}

/// @subsection rejection

TEST(OrbitalArenaAuthority, RejectsOrderWithTargetOffBoard) {
  // Given: a snake without an order
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {});

  // When: an order to a position outside the board is evaluated
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {orderOfA(1, Point{20, 5})});

  // Then: it is rejected and the snake moves straight on
  EXPECT_EQ(outcomes, (std::vector<ActivationOutcome>{OrderRejected{OrderId{1}, RejectionReason::TARGET_OFF_BOARD}}));
  EXPECT_EQ(headOfA(next), (Point{6, 5}));
}

/// @brief A refused replacement leaves the executing order and its route in place
TEST(OrbitalArenaAuthority, KeepsExecutingOrderWhenReplacementIsRejected) {
  // Given: F to (12,5) executes
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{12, 5})});

  // When: an invalid replacement is evaluated
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {orderOfA(2, Point{-1, 0})});

  // Then: F still executes and the snake follows it
  EXPECT_TRUE(next_events.empty());
  EXPECT_EQ(next.active.at(PLAYER_A).id, OrderId{1});
  EXPECT_EQ(headOfA(next), (Point{7, 5}));
}

/// @brief In a distributed realization an order of an earlier round may still arrive
TEST(OrbitalArenaAuthority, RejectsOrderOfAnotherRound) {
  // Given: a snake in round ROUND
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {});

  // When: an order of another round is evaluated
  auto [next, outcomes, events] =
      arena_authority::tick(noRandom(), state, {Order{RoundId{2}, OrderId{1}, PLAYER_A, Point{5, 2}}});

  // Then: it is rejected because its round has ended
  EXPECT_EQ(outcomes, (std::vector<ActivationOutcome>{OrderRejected{OrderId{1}, RejectionReason::ROUND_ENDED}}));
}

/// @subsection collisions

/// @brief Classic rule: only the victim of a bite is cut, and its cut segments become food
TEST(OrbitalArenaAuthority, CutsBittenSnakeAndDropsItsTailAsFood) {
  // Given: A at (4,4) heads up; B at (6,3) heads right with tail (5,3),(4,3),(3,3)
  State state = arena(SnakeSetup{Point{4, 4}, Direction::UP, 2}, SnakeSetup{Point{6, 3}, Direction::RIGHT, 4}, {});

  // When: a step passes
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {});

  // Then: A bit B at (4,3), B lost its tail from there, and A collected the dropped segment
  EXPECT_EQ(events, (ArenaEvents{Bitten{PLAYER_B, PLAYER_A}, FoodCollected{PLAYER_A, Point{4, 3}, std::nullopt}}));
  EXPECT_EQ(snake_model::length(next.snakes.at(PLAYER_B)), 3U);
}

/// @brief A nonlethal bite leaves the biter's executing order in place
TEST(OrbitalArenaAuthority, KeepsOrderOfSnakeInvolvedInNonlethalBite) {
  // Given: B executes an order to (12,3) and A will bite B's tail on the next step
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(),
      arena(SnakeSetup{Point{4, 5}, Direction::UP, 2}, SnakeSetup{Point{5, 3}, Direction::RIGHT, 4}, {}),
      {Order{ROUND, OrderId{1}, PLAYER_B, Point{12, 3}}});

  // When: the bite happens
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {});

  // Then: B's order still executes
  EXPECT_EQ(next_events.front(), (ArenaEvent{Bitten{PLAYER_B, PLAYER_A}}));
  EXPECT_EQ(next.active.at(PLAYER_B).id, OrderId{1});
}

TEST(OrbitalArenaAuthority, EndsOrderOfSnakeThatDied) {
  // Given: A executes an order to (12,5); A's and B's heads are next to each other, moving towards each other
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(),
      arena(SnakeSetup{Point{4, 5}, Direction::RIGHT, 2}, SnakeSetup{Point{7, 5}, Direction::LEFT, 2}, {}),
      {orderOfA(1, Point{12, 5})});

  // When: the heads meet
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {});

  // Then: both died, and A's order ended with its elimination right after the collision
  ASSERT_GE(next_events.size(), 2U);
  EXPECT_EQ(next_events[0], (ArenaEvent{MutualBite{PLAYER_A, PLAYER_B}}));
  EXPECT_EQ(next_events[1], (ArenaEvent{OrderEnded{OrderId{1}, EndReason::ELIMINATED, std::nullopt}}));
  EXPECT_TRUE(next.active.empty());
}

/// @brief Reaching the target in a lethal collision fails the order instead of completing it
TEST(OrbitalArenaAuthority, FailsOrderOnLethalArrival) {
  // Given: A at (5,5) heading right, B at (6,4) heading down; both heads will enter (6,5)
  State state = arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 2}, SnakeSetup{Point{6, 4}, Direction::DOWN, 2}, {});

  // When: an order of A to (6,5) activates on that step
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), state, {orderOfA(1, Point{6, 5})});

  // Then: both died head-on and A's order is eliminated, not completed
  EXPECT_EQ(headOfA(next), (Point{6, 5}));
  EXPECT_EQ(events.front(), (ArenaEvent{MutualBite{PLAYER_A, PLAYER_B}}));
  EXPECT_EQ(events.at(1), (ArenaEvent{OrderEnded{OrderId{1}, EndReason::ELIMINATED, std::nullopt}}));
  EXPECT_EQ(
      std::count(events.begin(), events.end(), ArenaEvent{OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}),
      0);
}

TEST(OrbitalArenaAuthority, RejectsOrderForDeadSnake) {
  // Given: A and B died in a head-on collision
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(),
      arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 2}, SnakeSetup{Point{7, 5}, Direction::LEFT, 2}, {}),
      {});

  // When: an order for A is evaluated
  auto [next, next_outcomes, next_events] = arena_authority::tick(noRandom(), state, {orderOfA(1, Point{2, 2})});

  // Then: it is rejected
  EXPECT_EQ(next_outcomes, (std::vector<ActivationOutcome>{OrderRejected{OrderId{1}, RejectionReason::SNAKE_DEAD}}));
}

/// @section arena_authority::anySnakeAlive

TEST(OrbitalArenaAuthority, ReportsNoSnakeAliveAfterBothDied) {
  // Given: A and B died in a head-on collision
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(),
      arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 2}, SnakeSetup{Point{7, 5}, Direction::LEFT, 2}, {}),
      {});

  // When: the Arena is asked whether a snake is alive
  bool alive = arena_authority::anySnakeAlive(state);

  // Then: none is
  EXPECT_FALSE(alive);
}

/// @section arena_authority::endRound

TEST(OrbitalArenaAuthority, EndsExecutingOrdersWhenRoundEnds) {
  // Given: an executing order
  auto [state, outcomes, events] = arena_authority::tick(
      noRandom(), arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}), {orderOfA(1, Point{12, 5})});

  // When: the round ends
  auto [ended, end_events] = arena_authority::endRound(state);

  // Then: the order ended with the round, no order executes and the world is unchanged
  EXPECT_EQ(end_events, (ArenaEvents{OrderEnded{OrderId{1}, EndReason::ROUND_ENDED, std::nullopt}}));
  EXPECT_TRUE(ended.active.empty());
  EXPECT_TRUE(ended.ended);
  EXPECT_EQ(headOfA(ended), headOfA(state));
}

/// @brief In a distributed realization an eligible order may arrive after the round ended
TEST(OrbitalArenaAuthority, RejectsOrdersAndKeepsWorldAfterRoundEnded) {
  // Given: the round ended
  auto [ended, end_events] = arena_authority::endRound(arena(SnakeSetup{Point{5, 5}, Direction::RIGHT, 3}, {}));

  // When: a step with an order is requested
  auto [next, outcomes, events] = arena_authority::tick(noRandom(), ended, {orderOfA(1, Point{5, 2})});

  // Then: the order is rejected and nothing moves
  EXPECT_EQ(outcomes, (std::vector<ActivationOutcome>{OrderRejected{OrderId{1}, RejectionReason::ROUND_ENDED}}));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(headOfA(next), (Point{5, 5}));
  EXPECT_EQ(next.step, ended.step);
}

}  // namespace orbital
}  // namespace snake
