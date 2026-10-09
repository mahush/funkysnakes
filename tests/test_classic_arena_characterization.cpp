// Characterization tests for classic arena behavior.
//
// These tests pin the CURRENT behavior of the building blocks that make up the
// classic arena tick (steering filter, snake evolution, collisions, food, scores).
// They establish the extraction baseline for the Classic Arena Authority: a failing
// test here means gameplay changed, not that a bug was fixed. Intentional rule
// changes require a separate, explicit decision.

#include <gtest/gtest.h>

#include <deque>
#include <ostream>
#include <vector>

#include "snake/direction_command_filter.hpp"
#include "snake/game_logic.hpp"
#include "snake/game_messages.hpp"
#include "snake/snake_model_evolve.hpp"

namespace snake {

// Readable gtest output for points
inline void PrintTo(const Point& p, std::ostream* os) { *os << "(" << p.x << "," << p.y << ")"; }

namespace {

const Board BOARD{60, 20};

/**
 * @brief Deterministic random source replaying a fixed sequence of values
 *
 * Ignores the requested range; tests choose values that are valid for it.
 */
RandomIntGeneratorFn makeScriptedRandom(std::vector<int> values) {
  auto queue = std::make_shared<std::deque<int>>(values.begin(), values.end());
  return [queue](int /* min */, int /* max */) {
    int value = queue->front();
    queue->pop_front();
    return value;
  };
}

DirectionCommand steer(const PlayerId& player_id, Direction dir) {
  return DirectionCommand{"game_001", player_id, dir};
}

PerPlayerSnakes oneSnake(Direction heading) { return {{PLAYER_A, snake_model::initial(Point{10, 10}, heading, 5)}}; }

std::vector<Direction> queued(direction_command_filter::State state, const PlayerId& player_id) {
  std::vector<Direction> result;
  while (true) {
    auto [next_state, consumed] = direction_command_filter::try_consume_next(state);
    state = next_state;
    auto it = consumed.find(player_id);
    if (it == consumed.end()) break;
    result.push_back(it->second);
  }
  return result;
}

}  // namespace

// ============================================================================
// Steering (direction_command_filter)
// ============================================================================

TEST(ClassicArenaSteering, AcceptsPerpendicularTurn) {
  auto state = direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP}));
}

TEST(ClassicArenaSteering, RejectsSameDirectionAsHeading) {
  auto state = direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::RIGHT));
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

TEST(ClassicArenaSteering, RejectsReverseOfHeading) {
  auto state = direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

TEST(ClassicArenaSteering, ChecksAgainstLastQueuedTurnNotHeading) {
  PerPlayerSnakes snakes = oneSnake(Direction::RIGHT);
  auto state = direction_command_filter::try_add({}, snakes, steer(PLAYER_A, Direction::UP));
  // LEFT reverses the heading but is perpendicular to the queued UP -> accepted
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::LEFT));
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

TEST(ClassicArenaSteering, BuffersAtMostTwoTurns) {
  PerPlayerSnakes snakes = oneSnake(Direction::RIGHT);
  auto state = direction_command_filter::try_add({}, snakes, steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::LEFT));
  // UP is a valid turn after LEFT, but the queue is full
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::UP));
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

TEST(ClassicArenaSteering, OppositeOfFirstQueuedTurnCancelsQueueThenAddsNewTurn) {
  PerPlayerSnakes snakes = oneSnake(Direction::RIGHT);
  auto state = direction_command_filter::try_add({}, snakes, steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::DOWN));
  // Note: queue was full, but cancellation happens before the full-queue check
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::DOWN}));
}

TEST(ClassicArenaSteering, CancellationPersistsEvenWhenNewTurnIsThenRejected) {
  // Queue UP while heading RIGHT, then evaluate DOWN against a snake already heading UP:
  // DOWN cancels the queued UP, then is rejected as a reverse turn -> queue stays empty.
  auto state = direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, oneSnake(Direction::UP), steer(PLAYER_A, Direction::DOWN));
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

TEST(ClassicArenaSteering, IgnoresUnknownPlayer) {
  auto state = direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_B, Direction::UP));
  EXPECT_TRUE(queued(state, PLAYER_B).empty());
}

TEST(ClassicArenaSteering, ConsumesOneTurnPerPlayerPerStep) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
                         {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 5)}};
  auto state = direction_command_filter::try_add({}, snakes, steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, snakes, steer(PLAYER_B, Direction::DOWN));

  auto [after_first, first] = direction_command_filter::try_consume_next(state);
  EXPECT_EQ(first, (PerPlayerDirection{{PLAYER_A, Direction::UP}, {PLAYER_B, Direction::DOWN}}));

  auto [after_second, second] = direction_command_filter::try_consume_next(after_first);
  EXPECT_EQ(second, (PerPlayerDirection{{PLAYER_A, Direction::LEFT}}));

  auto [after_third, third] = direction_command_filter::try_consume_next(after_second);
  EXPECT_TRUE(third.empty());
}

// ============================================================================
// Snake evolution (snake_model)
// ============================================================================

TEST(ClassicArenaSnake, InitialBodyExtendsBackwardsFromHead) {
  Snake s = snake_model::initial(Point{5, 10}, Direction::RIGHT, 3);
  EXPECT_EQ(snake_model::head(s), (Point{5, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{4, 10}, {3, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
  EXPECT_TRUE(snake_model::alive(s));
}

TEST(ClassicArenaSnake, MoveKeepsLength) {
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::DOWN, BOARD);
  EXPECT_EQ(snake_model::head(s), (Point{5, 11}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::DOWN);
}

TEST(ClassicArenaSnake, GrowKeepsTailTip) {
  Snake s = snake_model::grow(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::RIGHT, BOARD);
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}, {3, 10}}));
}

TEST(ClassicArenaSnake, WrapsAtAllBoardEdges) {
  EXPECT_EQ(snake_model::nextHead(snake_model::initial(Point{59, 5}, Direction::RIGHT, 3), Direction::RIGHT, BOARD),
            (Point{0, 5}));
  EXPECT_EQ(snake_model::nextHead(snake_model::initial(Point{0, 5}, Direction::LEFT, 3), Direction::LEFT, BOARD),
            (Point{59, 5}));
  EXPECT_EQ(snake_model::nextHead(snake_model::initial(Point{5, 0}, Direction::UP, 3), Direction::UP, BOARD),
            (Point{5, 19}));
  EXPECT_EQ(snake_model::nextHead(snake_model::initial(Point{5, 19}, Direction::DOWN, 3), Direction::DOWN, BOARD),
            (Point{5, 0}));
}

TEST(ClassicArenaSnake, ReverseRequestIgnoredForLengthTwoOrMore) {
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 2), Direction::LEFT, BOARD);
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
}

TEST(ClassicArenaSnake, ReverseRequestAllowedForLengthOne) {
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 1), Direction::LEFT, BOARD);
  EXPECT_EQ(snake_model::head(s), (Point{4, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::LEFT);
}

TEST(ClassicArenaSnake, DeadSnakeDoesNotMoveOrGrow) {
  Snake dead = snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3));
  EXPECT_FALSE(snake_model::alive(dead));
  EXPECT_EQ(snake_model::head(snake_model::move(dead, Direction::RIGHT, BOARD)), (Point{5, 10}));
  EXPECT_EQ(snake_model::length(snake_model::grow(dead, Direction::RIGHT, BOARD)), 3u);
}

TEST(ClassicArenaSnake, CutAtTailPointReturnsSegmentsFromCutOnwards) {
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{7, 5});
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

TEST(ClassicArenaSnake, CutAtHeadOrOutsideBodyChangesNothing) {
  Snake original = snake_model::initial(Point{10, 5}, Direction::RIGHT, 5);
  auto [at_head, cut_head] = snake_model::cutAt(original, Point{10, 5});
  EXPECT_EQ(snake_model::length(at_head), 5u);
  EXPECT_TRUE(cut_head.empty());
  auto [outside, cut_outside] = snake_model::cutAt(original, Point{0, 0});
  EXPECT_EQ(snake_model::length(outside), 5u);
  EXPECT_TRUE(cut_outside.empty());
}

// ============================================================================
// Movement (moveSnakes)
// ============================================================================

TEST(ClassicArenaMovement, UsesConsumedTurnOtherwiseHeading) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                         {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 3)}};
  PerPlayerSnakes moved = moveSnakes(snakes, BOARD, {}, {{PLAYER_A, Direction::UP}});
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_A)), (Point{10, 4}));
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_B)), (Point{11, 15}));
}

TEST(ClassicArenaMovement, GrowsWhenNextHeadIsOnFood) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}};
  PerPlayerSnakes moved = moveSnakes(snakes, BOARD, {{11, 5}}, {});
  EXPECT_EQ(snake_model::length(moved.at(PLAYER_A)), 4u);
}

TEST(ClassicArenaMovement, DeadSnakeStaysInPlace) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 3))}};
  PerPlayerSnakes moved = moveSnakes(snakes, BOARD, {}, {{PLAYER_A, Direction::UP}});
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_A)), (Point{10, 5}));
}

// ============================================================================
// Collisions (handleCollisions)
// ============================================================================

namespace {

// Snake of length 5 whose head ends on its own tail after DOWN, LEFT, UP
Snake selfBitingSnake() {
  Snake s = snake_model::initial(Point{5, 5}, Direction::RIGHT, 5);
  s = snake_model::move(s, Direction::DOWN, BOARD);
  s = snake_model::move(s, Direction::LEFT, BOARD);
  s = snake_model::move(s, Direction::UP, BOARD);
  return s;
}

PerPlayerScores startScores() { return {{PLAYER_A, 100}, {PLAYER_B, 100}}; }

}  // namespace

TEST(ClassicArenaCollision, SelfBiteKillsAndCostsTenPoints) {
  PerPlayerSnakes snakes{{PLAYER_A, selfBitingSnake()},
                         {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(new_snakes.at(PLAYER_B)));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_TRUE(cut.empty());
}

TEST(ClassicArenaCollision, BitingOtherTailCutsVictimAndOnlyVictimLosesPoints) {
  // B body: (10,5) (9,5) (8,5) (7,5) (6,5); A head lands on (7,5)
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{7, 5}, Direction::DOWN, 3)},
                         {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_TRUE(snake_model::alive(new_snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(new_snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::tail(new_snakes.at(PLAYER_B)), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(snake_model::length(new_snakes.at(PLAYER_A)), 3u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 100}, {PLAYER_B, 90}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

TEST(ClassicArenaCollision, BitingIsSymmetricForPlayerB) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
                         {PLAYER_B, snake_model::initial(Point{7, 5}, Direction::DOWN, 3)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_EQ(snake_model::tail(new_snakes.at(PLAYER_A)), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

TEST(ClassicArenaCollision, HeadOnKillsBothAndBothLoseTenPoints) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                         {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::LEFT, 3)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_B)));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
  EXPECT_TRUE(cut.empty());
}

TEST(ClassicArenaCollision, MutualTailBitesKillBothWithoutCutting) {
  // A body: (5,5) (4,5) (3,5) (2,5); B body: (3,5) (4,5) (5,5) (6,5)
  // A head (5,5) is on B's tail and B head (3,5) is on A's tail
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{5, 5}, Direction::RIGHT, 4)},
                         {PLAYER_B, snake_model::initial(Point{3, 5}, Direction::LEFT, 4)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::length(new_snakes.at(PLAYER_A)), 4u);
  EXPECT_EQ(snake_model::length(new_snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
  EXPECT_TRUE(cut.empty());
}

TEST(ClassicArenaCollision, NoSnakeVsSnakeCheckWhenOneIsDead) {
  // A is dead with its head on B's tail; dead snakes neither bite nor get bitten
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::kill(snake_model::initial(Point{7, 5}, Direction::DOWN, 3))},
                         {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}};
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_EQ(snake_model::length(new_snakes.at(PLAYER_B)), 5u);
  EXPECT_EQ(scores, startScores());
  EXPECT_TRUE(cut.empty());
}

TEST(ClassicArenaCollision, SelfBiteIsResolvedBeforeSnakeVsSnake) {
  // A bites itself and also has its head on B's tail: A dies, B is not cut
  Snake a = selfBitingSnake();  // head (4,5)
  PerPlayerSnakes snakes{{PLAYER_A, a}, {PLAYER_B, snake_model::initial(Point{4, 7}, Direction::DOWN, 4)}};
  // B body: (4,7) (4,6) (4,5) (4,4) -> contains A's head (4,5)
  auto [new_snakes, scores, cut] = handleCollisions(snakes, startScores());
  EXPECT_FALSE(snake_model::alive(new_snakes.at(PLAYER_A)));
  EXPECT_EQ(snake_model::length(new_snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_TRUE(cut.empty());
}

// ============================================================================
// Food and scores
// ============================================================================

TEST(ClassicArenaFood, EatingRemovesFoodAndAwardsTenPoints) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}};
  auto [food, scores] = handleFoodEating({{10, 5}, {20, 5}}, {{PLAYER_A, 0}}, snakes);
  EXPECT_EQ(food, (FoodItems{{20, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}}));
}

TEST(ClassicArenaFood, DeadSnakeDoesNotEat) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 3))}};
  auto [food, scores] = handleFoodEating({{10, 5}}, {{PLAYER_A, 0}}, snakes);
  EXPECT_EQ(food, (FoodItems{{10, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 0}}));
}

TEST(ClassicArenaFood, EatingOneOfDuplicateFoodItemsRemovesOnlyOne) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}};
  auto [food, scores] = handleFoodEating({{10, 5}, {10, 5}}, {{PLAYER_A, 0}}, snakes);
  EXPECT_EQ(food, (FoodItems{{10, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}}));
}

TEST(ClassicArenaFood, SharedFoodCellGoesToPlayerAInIsolation) {
  // Only reachable in isolation: in a full tick, equal heads are a head-on collision first
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                         {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::LEFT, 3)}};
  auto [food, scores] = handleFoodEating({{10, 5}}, {{PLAYER_A, 0}, {PLAYER_B, 0}}, snakes);
  EXPECT_TRUE(food.empty());
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, 0}}));
}

TEST(ClassicArenaFood, CutTailsAreAppendedAsFood) {
  EXPECT_EQ(dropCutTailsAsFood({{1, 1}}, {{7, 5}, {6, 5}}), (FoodItems{{1, 1}, {7, 5}, {6, 5}}));
}

TEST(ClassicArenaFood, DeadSnakeBodyIsDroppedAsFoodOnEveryCall) {
  // The dead snake stays in the snakes map, so each tick drops its body again
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 2))},
                         {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}};
  FoodItems once = dropDeadSnakesAsFood({}, snakes);
  EXPECT_EQ(once, (FoodItems{{10, 5}, {9, 5}}));
  FoodItems twice = dropDeadSnakesAsFood(once, snakes);
  EXPECT_EQ(twice, (FoodItems{{10, 5}, {9, 5}, {10, 5}, {9, 5}}));
}

TEST(ClassicArenaFood, ReplenishAddsUntilTargetCount) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}};
  FoodItems food = replenishFood(makeScriptedRandom({1, 1, 2, 2}), 3, {{0, 0}}, BOARD, snakes);
  EXPECT_EQ(food, (FoodItems{{0, 0}, {1, 1}, {2, 2}}));
}

TEST(ClassicArenaFood, ReplenishDoesNothingAtOrAboveTarget) {
  FoodItems food = replenishFood(makeScriptedRandom({}), 1, {{0, 0}, {1, 1}}, BOARD, {});
  EXPECT_EQ(food, (FoodItems{{0, 0}, {1, 1}}));
}

TEST(ClassicArenaFood, PlacementRetriesSnakeCellsButMayStackOnExistingFood) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}};
  // First candidate (9,5) is a snake cell -> retried; second (0,0) already holds food -> accepted
  FoodItems food = replenishFood(makeScriptedRandom({9, 5, 0, 0}), 2, {{0, 0}}, BOARD, snakes);
  EXPECT_EQ(food, (FoodItems{{0, 0}, {0, 0}}));
}

TEST(ClassicArenaFood, PlacementFallsBackToUncheckedPositionAfterHundredAttempts) {
  PerPlayerSnakes snakes{{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 1)}};
  std::vector<int> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(10);
    values.push_back(5);
  }
  values.push_back(10);
  values.push_back(5);
  Point pos = generateRandomFoodPosition(BOARD, snakes, makeScriptedRandom(values));
  EXPECT_EQ(pos, (Point{10, 5}));
}

TEST(ClassicArenaFood, RepositionReplacesRandomlySelectedItem) {
  FoodItems food = repositionRandomFood(makeScriptedRandom({1, 7, 7}), {{0, 0}, {1, 1}, {2, 2}}, BOARD, {});
  EXPECT_EQ(food, (FoodItems{{0, 0}, {7, 7}, {2, 2}}));
}

TEST(ClassicArenaFood, RepositionOfEmptyFoodDoesNothing) {
  EXPECT_TRUE(repositionRandomFood(makeScriptedRandom({}), {}, BOARD, {}).empty());
}

// ============================================================================
// Player setup
// ============================================================================

TEST(ClassicArenaSetup, AddPlayerCreatesSnakeAndZeroScore) {
  auto [snakes, scores] = addPlayer(PLAYER_A, Point{5, 10}, Direction::RIGHT, 7, {}, {});
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_A)), 7u);
  EXPECT_EQ(snake_model::head(snakes.at(PLAYER_A)), (Point{5, 10}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 0}}));
}

}  // namespace snake
