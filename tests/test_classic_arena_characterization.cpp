// Characterization tests for classic arena behavior.
//
// These tests pin the CURRENT behavior of the building blocks that make up the
// classic arena step (steering filter, snake evolution, collisions, food, scores)
// and of the complete step owned by the Classic Arena Authority. A failing test
// here means gameplay changed, not that a bug was fixed. Intentional rule changes
// require a separate, explicit decision.

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <ostream>
#include <string>
#include <vector>

#include "snake/classic_arena_authority.hpp"
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

/**
 * @brief Random source that fails the test when consulted
 */
RandomIntGeneratorFn noRandom() {
  return [](int /* min */, int /* max */) -> int {
    ADD_FAILURE() << "unexpected random draw";
    return 0;
  };
}

DirectionCommand steer(const PlayerId& player_id, Direction dir) {
  return DirectionCommand{"game_001", player_id, dir};
}

PerPlayerSnakes oneSnake(Direction heading) { return {{PLAYER_A, snake_model::initial(Point{10, 10}, heading, 5)}}; }

/**
 * @brief Read all queued turns of a player by draining a copy of the filter state
 */
std::vector<Direction> queued(direction_command_filter::State state, const PlayerId& player_id) {
  std::vector<Direction> result;
  while (true) {
    auto [next_state, consumed] = direction_command_filter::try_consume_next(state);
    state = next_state;
    auto it = consumed.find(player_id);
    if (it == consumed.end()) {
      break;
    }
    result.push_back(it->second);
  }
  return result;
}

/**
 * @brief Snake of length 5 whose head ends on its own tail after DOWN, LEFT, UP (head at (4,5))
 */
Snake selfBitingSnake() {
  Snake s = snake_model::initial(Point{5, 5}, Direction::RIGHT, 5);
  s = snake_model::move(s, Direction::DOWN, BOARD);
  s = snake_model::move(s, Direction::LEFT, BOARD);
  s = snake_model::move(s, Direction::UP, BOARD);
  return s;
}

PerPlayerScores startScores() { return {{PLAYER_A, 100}, {PLAYER_B, 100}}; }

/**
 * @brief Arena with explicit snakes and food, scores at zero
 *
 * Filler food is added in row 0 (x >= 50) up to MIN_FOOD_COUNT so that replenishment
 * does not draw random positions unless food is eaten.
 */
classic_arena_authority::State arena(PerPlayerSnakes snakes, FoodItems food) {
  classic_arena_authority::State state;
  state.board = BOARD;
  state.snakes = std::move(snakes);
  for (const auto& entry : state.snakes) {
    state.scores[entry.first] = 0;
  }
  for (int i = static_cast<int>(food.size()); i < classic_arena_authority::MIN_FOOD_COUNT; ++i) {
    food.push_back(Point{50 + i, 0});
  }
  state.food_items = std::move(food);
  return state;
}

FoodItems withoutFiller(FoodItems food) {
  food.erase(std::remove_if(food.begin(), food.end(), [](const Point& p) { return p.y == 0 && p.x >= 50; }),
             food.end());
  return food;
}

}  // namespace

/// @section direction_command_filter::try_add
///
/// Acceptance and cancellation of buffered steering intentions.

/// @brief Ensures that a perpendicular turn is buffered
TEST(ClassicArenaSteering, QueuesPerpendicularTurn) {
  // When: a perpendicular turn is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // Then: it is queued
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP}));
}

/// @brief Ensures that a turn into the current heading is not buffered
TEST(ClassicArenaSteering, RejectsTurnIntoCurrentHeading) {
  // When: a turn into the current heading is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::RIGHT));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that a reverse turn of the current heading is not buffered
TEST(ClassicArenaSteering, RejectsReverseOfHeading) {
  // When: the reverse of the heading is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that a new turn is checked against the last queued turn, not the snake's heading
TEST(ClassicArenaSteering, ChecksTurnAgainstLastQueuedTurn) {
  // Given: a turn is queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // When: the reverse of the heading is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // Then: it is queued
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

/// @brief Ensures that at most two turns are buffered per player
TEST(ClassicArenaSteering, BuffersAtMostTwoTurns) {
  // Given: two turns are queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // When: another valid turn is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // Then: it is rejected
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

/// @brief Ensures that a player can change their mind: cancellation happens before the full-queue check
TEST(ClassicArenaSteering, ReplacesQueueWithOppositeOfFirstQueuedTurn) {
  // Given: two turns are queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // When: the opposite of the first queued turn is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::DOWN));

  // Then: the queue holds only the new turn
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::DOWN}));
}

/// @brief Ensures that cancellation persists even when the cancelling turn itself is then rejected
TEST(ClassicArenaSteering, KeepsCancellationWhenNewTurnIsRejected) {
  // Given: a turn is queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // When: its opposite is added, which reverses the heading
  state = direction_command_filter::try_add(state, oneSnake(Direction::UP), steer(PLAYER_A, Direction::DOWN));

  // Then: the queue is cleared and the new turn is not queued
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that turns of players without a snake are ignored
TEST(ClassicArenaSteering, RejectsTurnOfUnknownPlayer) {
  // When: a turn is added for an unknown player
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_B, Direction::UP));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_B).empty());
}

/// @section direction_command_filter::try_consume_next
///
/// One buffered turn per player is applied per arena step.

namespace {

PerPlayerSnakes twoSnakes() {
  return {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
          {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 5)}};
}

}  // namespace

/// @brief Ensures that each player's first buffered turn is consumed in one step
TEST(ClassicArenaSteering, ConsumesOneTurnPerPlayer) {
  // Given: turns are queued for both players
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_B, Direction::DOWN));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: the first turn of each player is consumed
  EXPECT_EQ(consumed, (PerPlayerDirection{{PLAYER_A, Direction::UP}, {PLAYER_B, Direction::DOWN}}));
}

/// @brief Ensures that a second buffered turn is kept for the next step
TEST(ClassicArenaSteering, ConsumesQueuedTurnsInOrder) {
  // Given: two turns are queued and the first was consumed
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_B, Direction::DOWN));
  state = std::get<0>(direction_command_filter::try_consume_next(state));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: the second turn is consumed
  EXPECT_EQ(consumed, (PerPlayerDirection{{PLAYER_A, Direction::LEFT}}));
}

/// @brief Ensures that nothing is consumed once all buffered turns are applied
TEST(ClassicArenaSteering, ConsumesNothingOnceAllTurnsAreConsumed) {
  // Given: all queued turns were consumed
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = std::get<0>(direction_command_filter::try_consume_next(state));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: nothing is consumed
  EXPECT_TRUE(consumed.empty());
}

/// @section snake_model
///
/// Evolution of one snake's body and life.

/// @subsection initial and kill

/// @brief Ensures that a new snake is laid out straight behind its head
TEST(ClassicArenaSnake, ExtendsInitialBodyBackwardsFromHead) {
  // When: a snake is created
  Snake s = snake_model::initial(Point{5, 10}, Direction::RIGHT, 3);

  // Then: its body extends backwards from the head
  EXPECT_EQ(snake_model::head(s), (Point{5, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{4, 10}, {3, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
  EXPECT_TRUE(snake_model::alive(s));
}

/// @brief Ensures that killing a snake marks it dead
TEST(ClassicArenaSnake, MarksKilledSnakeDead) {
  // When: the snake is killed
  Snake s = snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3));

  // Then: it is dead
  EXPECT_FALSE(snake_model::alive(s));
}

/// @subsection move and grow

/// @brief Ensures that moving advances the head and keeps the snake's length
TEST(ClassicArenaSnake, MovesByAdvancingHeadAndKeepingLength) {
  // When: the snake moves
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::DOWN, BOARD);

  // Then: the head advances and the length stays
  EXPECT_EQ(snake_model::head(s), (Point{5, 11}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::DOWN);
}

/// @brief Ensures that a snake of length two or more cannot reverse into its own body
TEST(ClassicArenaSnake, IgnoresReverseTurn) {
  // When: the snake moves in its reverse direction
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 2), Direction::LEFT, BOARD);

  // Then: it keeps its heading
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
}

/// @brief Ensures that a snake of length one may reverse, as it has no body to run into
TEST(ClassicArenaSnake, ReversesSnakeOfLengthOne) {
  // When: a snake of length one moves in its reverse direction
  Snake s = snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 1), Direction::LEFT, BOARD);

  // Then: it reverses
  EXPECT_EQ(snake_model::head(s), (Point{4, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::LEFT);
}

/// @brief Ensures that dead snakes do not move
TEST(ClassicArenaSnake, DoesNotMoveDeadSnake) {
  // When: a dead snake moves
  Snake s = snake_model::move(
      snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3)), Direction::RIGHT, BOARD);

  // Then: it stays in place
  EXPECT_EQ(snake_model::head(s), (Point{5, 10}));
}

/// @brief Ensures that growing advances the head and keeps the tail tip
TEST(ClassicArenaSnake, GrowsByKeepingTailTip) {
  // When: the snake grows
  Snake s = snake_model::grow(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::RIGHT, BOARD);

  // Then: the head advances and the tail tip stays
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}, {3, 10}}));
}

/// @brief Ensures that dead snakes do not grow
TEST(ClassicArenaSnake, DoesNotGrowDeadSnake) {
  // When: a dead snake grows
  Snake s = snake_model::grow(
      snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3)), Direction::RIGHT, BOARD);

  // Then: its length stays
  EXPECT_EQ(snake_model::length(s), 3u);
}

/// @subsection nextHead

namespace {

struct WrapCase {
  std::string edge;
  Point head;
  Direction dir;
  Point expected;
};

class ClassicArenaSnakeWrap : public ::testing::TestWithParam<WrapCase> {};

}  // namespace

/// @brief Ensures that the board wraps around at every edge
TEST_P(ClassicArenaSnakeWrap, WrapsAtBoardEdge) {
  // When: the next head is computed towards a board edge
  Point next = snake_model::nextHead(snake_model::initial(GetParam().head, GetParam().dir, 3), GetParam().dir, BOARD);

  // Then: it wraps to the opposite edge
  EXPECT_EQ(next, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(AllEdges,
                         ClassicArenaSnakeWrap,
                         ::testing::Values(WrapCase{"Right", {59, 5}, Direction::RIGHT, {0, 5}},
                                           WrapCase{"Left", {0, 5}, Direction::LEFT, {59, 5}},
                                           WrapCase{"Top", {5, 0}, Direction::UP, {5, 19}},
                                           WrapCase{"Bottom", {5, 19}, Direction::DOWN, {5, 0}}),
                         [](const ::testing::TestParamInfo<WrapCase>& info) { return info.param.edge; });

/// @subsection cutAt

/// @brief Ensures that cutting a tail returns the segments from the cut point onwards
TEST(ClassicArenaSnake, CutsOffSegmentsFromCutPointOnwards) {
  // When: the snake is cut at a tail point
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{7, 5});

  // Then: the segments from that point onwards are cut off
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

/// @brief Ensures that a head cannot be cut off
TEST(ClassicArenaSnake, KeepsSnakeWhenCutAtHead) {
  // When: the snake is cut at its head
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{10, 5});

  // Then: the snake is unchanged and nothing is cut off
  EXPECT_EQ(snake_model::length(s), 5u);
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that cutting outside the body has no effect
TEST(ClassicArenaSnake, KeepsSnakeWhenCutOutsideBody) {
  // When: the snake is cut at a point outside its body
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{0, 0});

  // Then: the snake is unchanged and nothing is cut off
  EXPECT_EQ(snake_model::length(s), 5u);
  EXPECT_TRUE(cut.empty());
}

/// @section moveSnakes
///
/// Movement of all snakes in one step.

/// @brief Ensures that consumed turns apply per player and other snakes keep their heading
TEST(ClassicArenaMovement, TurnsOnlySnakeWithTurn) {
  // When: snakes move with a turn for one player
  PerPlayerSnakes moved = moveSnakes({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                                      {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 3)}},
                                     BOARD,
                                     {},
                                     {{PLAYER_A, Direction::UP}});

  // Then: only that snake turns
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_A)), (Point{10, 4}));
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_B)), (Point{11, 15}));
}

/// @brief Ensures that a snake grows when its next head position holds food
TEST(ClassicArenaMovement, GrowsSnakeMovingOntoFood) {
  // When: a snake moves onto food
  PerPlayerSnakes moved =
      moveSnakes({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}}, BOARD, {{11, 5}}, {});

  // Then: it grows
  EXPECT_EQ(snake_model::length(moved.at(PLAYER_A)), 4u);
}

/// @brief Ensures that dead snakes are not moved, even with a consumed turn
TEST(ClassicArenaMovement, LeavesDeadSnakeInPlaceWhenMoving) {
  // When: snakes move with one dead
  PerPlayerSnakes moved =
      moveSnakes({{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 3))}},
                 BOARD,
                 {},
                 {{PLAYER_A, Direction::UP}});

  // Then: the dead snake stays in place
  EXPECT_EQ(snake_model::head(moved.at(PLAYER_A)), (Point{10, 5}));
}

/// @section handleCollisions
///
/// Collision consequences for snakes and scores.

/// @brief Ensures that biting yourself is fatal and costs points
TEST(ClassicArenaCollision, KillsSelfBitingSnake) {
  // When: a self-bite is resolved
  auto [snakes, scores, cut] = handleCollisions(
      {{PLAYER_A, selfBitingSnake()}, {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}},
      startScores());

  // Then: the snake dies and loses ten points
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(snakes.at(PLAYER_B)));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that biting another snake's tail cuts the victim and only the victim loses points
TEST(ClassicArenaCollision, CutsBittenTailOfPlayerB) {
  // When: player A bites player B's tail
  // B body: (10,5) (9,5) (8,5) (7,5) (6,5); A head on (7,5)
  auto [snakes, scores, cut] = handleCollisions({{PLAYER_A, snake_model::initial(Point{7, 5}, Direction::DOWN, 3)},
                                                 {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}},
                                                startScores());

  // Then: player B is cut and only player B loses points
  EXPECT_TRUE(snake_model::alive(snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::tail(snakes.at(PLAYER_B)), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_A)), 3u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 100}, {PLAYER_B, 90}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

/// @brief Ensures that tail biting works the same way for Player B as the biter
TEST(ClassicArenaCollision, CutsBittenTailOfPlayerA) {
  // When: player B bites player A's tail
  auto [snakes, scores, cut] = handleCollisions({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
                                                 {PLAYER_B, snake_model::initial(Point{7, 5}, Direction::DOWN, 3)}},
                                                startScores());

  // Then: player A is cut and only player A loses points
  EXPECT_EQ(snake_model::tail(snakes.at(PLAYER_A)), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

/// @brief Ensures that a head-on collision is fatal for both snakes
TEST(ClassicArenaCollision, KillsBothSnakesInHeadOnCollision) {
  // When: a head-on collision is resolved
  auto [snakes, scores, cut] = handleCollisions({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                                                 {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::LEFT, 3)}},
                                                startScores());

  // Then: both die and lose ten points each
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_B)));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that mutual tail bites are fatal for both snakes and cut nothing
TEST(ClassicArenaCollision, KillsBothSnakesInMutualTailBite) {
  // When: mutual tail bites are resolved
  // A body: (5,5) (4,5) (3,5) (2,5); B body: (3,5) (4,5) (5,5) (6,5)
  auto [snakes, scores, cut] = handleCollisions({{PLAYER_A, snake_model::initial(Point{5, 5}, Direction::RIGHT, 4)},
                                                 {PLAYER_B, snake_model::initial(Point{3, 5}, Direction::LEFT, 4)}},
                                                startScores());

  // Then: both die and nothing is cut
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_A)), 4u);
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 90}}));
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that dead snakes neither bite nor get bitten
TEST(ClassicArenaCollision, IgnoresDeadSnakeInSnakeVsSnakeCheck) {
  // When: collisions are resolved with a dead snake's head on another tail
  auto [snakes, scores, cut] =
      handleCollisions({{PLAYER_A, snake_model::kill(snake_model::initial(Point{7, 5}, Direction::DOWN, 3))},
                        {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}},
                       startScores());

  // Then: nothing changes
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_B)), 5u);
  EXPECT_EQ(scores, startScores());
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that a self-bite is resolved before snake-against-snake collisions
TEST(ClassicArenaCollision, ResolvesSelfBiteBeforeSnakeVsSnake) {
  // When: a self-bite and a bite on another snake are resolved
  // A head (4,5) is on its own tail and on B's body (4,7) (4,6) (4,5) (4,4)
  auto [snakes, scores, cut] = handleCollisions(
      {{PLAYER_A, selfBitingSnake()}, {PLAYER_B, snake_model::initial(Point{4, 7}, Direction::DOWN, 4)}},
      startScores());

  // Then: only the self-bite counts
  EXPECT_FALSE(snake_model::alive(snakes.at(PLAYER_A)));
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 90}, {PLAYER_B, 100}}));
  EXPECT_TRUE(cut.empty());
}

/// @section handleFoodEating
///
/// Food consumption and its score consequence.

/// @brief Ensures that eating removes the food and awards points
TEST(ClassicArenaFood, EatsFoodUnderHead) {
  // When: a head is on food
  auto [food, scores] = handleFoodEating(
      {{10, 5}, {20, 5}}, {{PLAYER_A, 0}}, {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}});

  // Then: the food is removed and the snake gains ten points
  EXPECT_EQ(food, (FoodItems{{20, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}}));
}

/// @brief Ensures that dead snakes do not eat
TEST(ClassicArenaFood, DoesNotFeedDeadSnake) {
  // When: a dead snake's head is on food
  auto [food, scores] =
      handleFoodEating({{10, 5}},
                       {{PLAYER_A, 0}},
                       {{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 3))}});

  // Then: nothing changes
  EXPECT_EQ(food, (FoodItems{{10, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 0}}));
}

/// @brief Ensures that only one of several food items on the same cell is eaten per step
TEST(ClassicArenaFood, EatsOnlyOneOfDuplicateFoodItems) {
  // When: a head is on duplicate food items
  auto [food, scores] = handleFoodEating(
      {{10, 5}, {10, 5}}, {{PLAYER_A, 0}}, {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}});

  // Then: only one item is removed
  EXPECT_EQ(food, (FoodItems{{10, 5}}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}}));
}

/// @brief Pins that a shared food cell goes to Player A (only reachable in isolation, as equal heads collide first)
TEST(ClassicArenaFood, AwardsSharedFoodCellToPlayerA) {
  // When: both heads are on the same food
  auto [food, scores] = handleFoodEating({{10, 5}},
                                         {{PLAYER_A, 0}, {PLAYER_B, 0}},
                                         {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                                          {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::LEFT, 3)}});

  // Then: only player A eats it
  EXPECT_TRUE(food.empty());
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, 0}}));
}

/// @section dropCutTailsAsFood and dropDeadSnakesAsFood
///
/// Conversion of snake segments into food in BITE_DROP_FOOD mode.

/// @brief Ensures that cut tail segments become food
TEST(ClassicArenaFood, AppendsCutTailsAsFood) {
  // When: cut tails are dropped as food
  FoodItems food = dropCutTailsAsFood({{1, 1}}, {{7, 5}, {6, 5}});

  // Then: their segments are appended to the food
  EXPECT_EQ(food, (FoodItems{{1, 1}, {7, 5}, {6, 5}}));
}

namespace {

PerPlayerSnakes oneDeadOneAlive() {
  return {{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 2))},
          {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}};
}

}  // namespace

/// @brief Ensures that a dead snake's body becomes food
TEST(ClassicArenaFood, DropsDeadSnakeBodyAsFood) {
  // When: dead snakes are dropped as food
  FoodItems food = dropDeadSnakesAsFood({}, oneDeadOneAlive());

  // Then: the dead snake's body is appended
  EXPECT_EQ(food, (FoodItems{{10, 5}, {9, 5}}));
}

/// @brief Pins that a dead snake's body is dropped again on every call, as dead snakes stay in the arena
TEST(ClassicArenaFood, DropsDeadSnakeBodyAsFoodOnEveryCall) {
  // Given: a dead snake's body was dropped as food
  FoodItems food = dropDeadSnakesAsFood({}, oneDeadOneAlive());

  // When: dead snakes are dropped as food again
  food = dropDeadSnakesAsFood(food, oneDeadOneAlive());

  // Then: the body is appended again
  EXPECT_EQ(food, (FoodItems{{10, 5}, {9, 5}, {10, 5}, {9, 5}}));
}

/// @section replenishFood and generateRandomFoodPosition
///
/// Placement of new food.

/// @brief Ensures that food is topped up to the target count
TEST(ClassicArenaFood, ReplenishesFoodUpToTarget) {
  // When: food is replenished below the target
  FoodItems food = replenishFood(makeScriptedRandom({1, 1, 2, 2}),
                                 3,
                                 {{0, 0}},
                                 BOARD,
                                 {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}});

  // Then: items are added up to the target
  EXPECT_EQ(food, (FoodItems{{0, 0}, {1, 1}, {2, 2}}));
}

/// @brief Ensures that food above the target count is left alone
TEST(ClassicArenaFood, KeepsFoodAtOrAboveTarget) {
  // When: food is replenished above the target
  FoodItems food = replenishFood(noRandom(), 1, {{0, 0}, {1, 1}}, BOARD, {});

  // Then: nothing changes
  EXPECT_EQ(food, (FoodItems{{0, 0}, {1, 1}}));
}

/// @brief Ensures that food is never placed on a snake cell when a free cell is found
TEST(ClassicArenaFood, RetriesFoodPlacementOnSnakeCell) {
  // When: food is replenished and a candidate is on a snake
  // First candidate (9,5) is a tail cell of the snake
  FoodItems food = replenishFood(makeScriptedRandom({9, 5, 1, 1}),
                                 1,
                                 {},
                                 BOARD,
                                 {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)}});

  // Then: placement is retried
  EXPECT_EQ(food, (FoodItems{{1, 1}}));
}

/// @brief Pins that new food may be placed on a cell that already holds food
TEST(ClassicArenaFood, StacksReplenishedFoodOnExistingFood) {
  // When: food is replenished and a candidate is on existing food
  FoodItems food = replenishFood(makeScriptedRandom({0, 0}), 2, {{0, 0}}, BOARD, {});

  // Then: the food stacks
  EXPECT_EQ(food, (FoodItems{{0, 0}, {0, 0}}));
}

/// @brief Pins that placement gives up after 100 occupied candidates and uses an unchecked position
TEST(ClassicArenaFood, FallsBackToUncheckedPositionAfterHundredAttempts) {
  // When: a food position is drawn and all candidates are occupied
  // 100 attempts on the snake's head cell, then the unchecked fallback draw
  std::vector<int> draws;
  for (int i = 0; i < 101; ++i) {
    draws.push_back(10);
    draws.push_back(5);
  }
  Point pos = generateRandomFoodPosition(
      BOARD, {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 1)}}, makeScriptedRandom(draws));

  // Then: an unchecked position is used
  EXPECT_EQ(pos, (Point{10, 5}));
}

/// @section repositionRandomFood

/// @brief Ensures that one randomly selected food item is moved to a new position
TEST(ClassicArenaFood, RepositionsRandomlySelectedFoodItem) {
  // When: food is repositioned
  FoodItems food = repositionRandomFood(makeScriptedRandom({1, 7, 7}), {{0, 0}, {1, 1}, {2, 2}}, BOARD, {});

  // Then: a randomly selected item moves
  EXPECT_EQ(food, (FoodItems{{0, 0}, {7, 7}, {2, 2}}));
}

/// @brief Ensures that repositioning without food has no effect
TEST(ClassicArenaFood, KeepsEmptyFoodOnReposition) {
  // When: food is repositioned without any food
  FoodItems food = repositionRandomFood(noRandom(), {}, BOARD, {});

  // Then: nothing changes
  EXPECT_TRUE(food.empty());
}

/// @section addPlayer

/// @brief Ensures that a new player gets a snake and starts at zero points
TEST(ClassicArenaSetup, CreatesSnakeAndZeroScoreForNewPlayer) {
  // When: a player is added
  auto [snakes, scores] = addPlayer(PLAYER_A, Point{5, 10}, Direction::RIGHT, 7, {}, {});

  // Then: a snake and a zero score exist for them
  EXPECT_EQ(snake_model::length(snakes.at(PLAYER_A)), 7u);
  EXPECT_EQ(snake_model::head(snakes.at(PLAYER_A)), (Point{5, 10}));
  EXPECT_EQ(scores, (PerPlayerScores{{PLAYER_A, 0}}));
}

/// @section classic_arena_authority::initial

/// @brief Ensures that the classic arena starts with both players and the minimum amount of food
TEST(ClassicArenaTick, PlacesBothPlayersAndMinimumFood) {
  // When: the arena is created
  classic_arena_authority::State state =
      classic_arena_authority::initial(makeScriptedRandom({30, 1, 31, 1, 32, 1, 33, 1, 34, 1}), BOARD);

  // Then: both players and the minimum food are placed
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{5, 10}));
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_B)), (Point{5, 15}));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 7u);
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_B)), 7u);
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 0}, {PLAYER_B, 0}}));
  EXPECT_EQ(state.food_items, (FoodItems{{30, 1}, {31, 1}, {32, 1}, {33, 1}, {34, 1}}));
}

/// @section classic_arena_authority::steer and tick
///
/// Steering intentions are applied one turn per arena step.

/// @brief Ensures that only the first of several steered turns is applied in one step
TEST(ClassicArenaTick, AppliesOneSteeredTurnPerStep) {
  // Given: two turns were steered
  classic_arena_authority::State state =
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: only the first turn is applied
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{10, 9}));
}

/// @brief Ensures that a second steered turn is applied in the following step
TEST(ClassicArenaTick, AppliesSecondSteeredTurnOnSecondStep) {
  // Given: two turns were steered and one step done
  classic_arena_authority::State state =
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));
  state = classic_arena_authority::tick(noRandom(), state);

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the second turn is applied
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{9, 9}));
}

/// @brief Ensures that a snake keeps its last heading once all steered turns are applied
TEST(ClassicArenaTick, ContinuesHeadingOnceAllTurnsAreApplied) {
  // Given: all steered turns were applied
  classic_arena_authority::State state =
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));
  state = classic_arena_authority::tick(noRandom(), state);
  state = classic_arena_authority::tick(noRandom(), state);

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the snake continues in its heading
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{8, 9}));
}

/// @section classic_arena_authority::tick
///
/// Composition and ordering of the complete arena step.

/// @brief Ensures that eating grows the snake, scores and replenishes food within the same step
TEST(ClassicArenaTick, GrowsScoresAndReplenishesWhenEating) {
  // When: the arena steps with food at the next head
  classic_arena_authority::State state = classic_arena_authority::tick(
      makeScriptedRandom({20, 3}),
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}}));

  // Then: the snake grows and scores, and food is replenished
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 4u);
  EXPECT_EQ(state.scores.at(PLAYER_A), 10);
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{20, 3}}));
  EXPECT_EQ(state.food_items.size(), static_cast<size_t>(classic_arena_authority::MIN_FOOD_COUNT));
}

/// @brief Pins the ordering effect that a biter eats the first cut segment in the same step
///
/// Collisions run before food drops, and drops before eating: the cut segment under the biter's
/// head is eaten at once, so the victim loses 10 and the biter gains 10 without growing.
TEST(ClassicArenaTick, LetsBiterEatFirstCutSegment) {
  // When: the arena steps with a bite on another tail
  // A moves DOWN from (7,4) to (7,5); B moves RIGHT to body (11,5) (10,5) (9,5) (8,5) (7,5)
  classic_arena_authority::State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{7, 4}, Direction::DOWN, 3)},
                                           {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}},
                                          {}));

  // Then: the biter eats the first cut segment
  EXPECT_EQ(snake_model::tail(state.snakes.at(PLAYER_B)), (std::vector<Point>{{10, 5}, {9, 5}, {8, 5}}));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, -10}}));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 3u);
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{}));
}

/// @brief Ensures that cut segments not eaten by the biter remain on the board as food
TEST(ClassicArenaTick, KeepsRemainingCutSegmentsAsFood) {
  // When: the arena steps with a long cut
  // A moves LEFT from (11,6) to (10,6); B moves UP to body (10,4) (10,5) (10,6) (10,7) (10,8)
  classic_arena_authority::State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{11, 6}, Direction::LEFT, 3)},
                                           {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::UP, 5)}},
                                          {}));

  // Then: the remaining cut segments stay as food
  EXPECT_EQ(snake_model::tail(state.snakes.at(PLAYER_B)), (std::vector<Point>{{10, 5}}));
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 7}, {10, 8}}));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, -10}}));
}

/// @brief Ensures that snakes moving into the same cell both die and leave their bodies as food
TEST(ClassicArenaTick, KillsBothSnakesInHeadOnCollision) {
  // When: the arena steps with a head-on approach
  classic_arena_authority::State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{9, 5}, Direction::RIGHT, 2)},
                                           {PLAYER_B, snake_model::initial(Point{11, 5}, Direction::LEFT, 2)}},
                                          {}));

  // Then: both die and their bodies become food
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_B)));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, -10}}));
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 5}, {9, 5}, {10, 5}, {11, 5}}));
}

namespace {

classic_arena_authority::State arenaWithDeadSnake() {
  return arena({{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 10}, Direction::RIGHT, 2))},
                {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}},
               {});
}

}  // namespace

/// @brief Ensures that a dead snake's body is dropped as food in a step
TEST(ClassicArenaTick, DropsDeadSnakeBodyAsFood) {
  // When: the arena steps with a dead snake
  classic_arena_authority::State state = classic_arena_authority::tick(noRandom(), arenaWithDeadSnake());

  // Then: its body is dropped as food
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 10}, {9, 10}}));
}

/// @brief Pins that a dead snake's body is dropped again on every step
TEST(ClassicArenaTick, DropsDeadSnakeBodyAsFoodOnEveryStep) {
  // Given: a dead snake's body was dropped as food
  classic_arena_authority::State state = classic_arena_authority::tick(noRandom(), arenaWithDeadSnake());

  // When: the arena steps again
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the body is dropped as food again
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 10}, {9, 10}, {10, 10}, {9, 10}}));
}

/// @section classic_arena_authority::requestFoodReposition
///
/// A reposition request is applied once, on the next step.

/// @brief Ensures that a requested reposition moves one food item in the next step
TEST(ClassicArenaTick, RepositionsOneFoodItemOnRequest) {
  // Given: a reposition was requested
  classic_arena_authority::State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {}));

  // When: the arena steps
  // Draws: item index 0, then new position (40,7)
  state = classic_arena_authority::tick(makeScriptedRandom({0, 40, 7}), state);

  // Then: one food item is repositioned
  EXPECT_EQ(state.food_items.front(), (Point{40, 7}));
}

/// @brief Ensures that a reposition request is consumed by the step that applies it
TEST(ClassicArenaTick, RepositionsFoodOnlyOncePerRequest) {
  // Given: a requested reposition was applied
  classic_arena_authority::State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {}));
  state = classic_arena_authority::tick(makeScriptedRandom({0, 40, 7}), state);

  // When: the arena steps again
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: no food is repositioned
  EXPECT_EQ(state.food_items.front(), (Point{40, 7}));
}

/// @brief Ensures that repositioning happens after eating and replenishment within the same step
TEST(ClassicArenaTick, RepositionsFoodAfterReplenishing) {
  // Given: a reposition was requested
  classic_arena_authority::State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}}));

  // When: the arena steps with food eaten
  // Draws: replenish one item at (20,3), then reposition index 4 (the replenished item) to (21,4)
  state = classic_arena_authority::tick(makeScriptedRandom({20, 3, 4, 21, 4}), state);

  // Then: food is replenished before it is repositioned
  EXPECT_EQ(state.food_items.back(), (Point{21, 4}));
  EXPECT_EQ(state.scores.at(PLAYER_A), 10);
}

}  // namespace snake
