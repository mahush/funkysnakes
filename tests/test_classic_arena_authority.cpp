// Characterization tests for the Classic Arena Authority.
//
// These tests pin the CURRENT behavior of the classic arena through the Authority's
// transitions only (initial, steer, requestFoodReposition, tick), so they stay valid
// however the arena step is implemented internally. A failing test here means gameplay
// changed, not that a bug was fixed. Intentional rule changes require a separate,
// explicit decision.

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

#include "snake/classic_arena_authority.hpp"
#include "snake/game_boundary.hpp"
#include "snake/game_messages.hpp"
#include "snake/snake_model_evolve.hpp"
#include "test_printers.hpp"

namespace snake {

namespace {

using classic_arena_authority::State;

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

DirectionCommand steer(const PlayerId& player_id, Direction dir) { return DirectionCommand{player_id, dir}; }

/**
 * @brief Arena with explicit snakes and food, scores at zero
 *
 * Filler food is added in row 0 (x >= 50) up to MIN_FOOD_COUNT so that replenishment
 * does not draw random positions unless food is eaten.
 */
State arena(PerPlayerSnakes snakes, FoodItems food) {
  State state;
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

/**
 * @brief Arena with explicit snakes and exactly the given food (no filler)
 */
State arenaWithExactFood(PerPlayerSnakes snakes, FoodItems food) {
  State state = arena(std::move(snakes), {});
  state.food_items = std::move(food);
  return state;
}

FoodItems withoutFiller(FoodItems food) {
  food.erase(std::remove_if(food.begin(), food.end(), [](const Point& p) { return p.y == 0 && p.x >= 50; }),
             food.end());
  return food;
}

/**
 * @brief Snake of length 5 heading UP whose next step lands on its own tail
 *
 * Body: (4,6) (5,6) (5,5) (4,5) (3,5); moving UP puts the head on (4,5).
 */
Snake snakeAboutToBiteItself() {
  Snake s = snake_model::initial(Point{5, 5}, Direction::RIGHT, 5);
  s = snake_model::move(s, Direction::DOWN, BOARD);
  s = snake_model::move(s, Direction::LEFT, BOARD);
  return s;
}

}  // namespace

/// @section classic_arena_authority::initial

/// @brief Ensures that the classic arena starts with both players and the minimum amount of food
TEST(ClassicArenaAuthority, PlacesBothPlayersAndMinimumFood) {
  // When: the arena is created
  State state = classic_arena_authority::initial(makeScriptedRandom({30, 1, 31, 1, 32, 1, 33, 1, 34, 1}), BOARD);

  // Then: both players and the minimum food are placed
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{5, 10}));
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_B)), (Point{5, 15}));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 7u);
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_B)), 7u);
  EXPECT_EQ(snake_model::currentDirection(state.snakes.at(PLAYER_A)), Direction::RIGHT);
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 0}, {PLAYER_B, 0}}));
  EXPECT_EQ(state.food_items, (FoodItems{{30, 1}, {31, 1}, {32, 1}, {33, 1}, {34, 1}}));
}

/// @section classic_arena_authority::steer
///
/// Steering intentions are applied one turn per arena step.

/// @brief Ensures that only the first of several steered turns is applied in one step
TEST(ClassicArenaAuthority, AppliesOneSteeredTurnPerStep) {
  // Given: two turns were steered
  State state = arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: only the first turn is applied
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{10, 9}));
}

/// @brief Ensures that a second steered turn is applied in the following step
TEST(ClassicArenaAuthority, AppliesSecondSteeredTurnOnSecondStep) {
  // Given: two turns were steered and one step done
  State state = arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));
  state = classic_arena_authority::tick(noRandom(), state);

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the second turn is applied
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{9, 9}));
}

/// @brief Ensures that a snake keeps its last heading once all steered turns are applied
TEST(ClassicArenaAuthority, ContinuesHeadingOnceAllTurnsAreApplied) {
  // Given: all steered turns were applied
  State state = arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::LEFT));
  state = classic_arena_authority::tick(noRandom(), state);
  state = classic_arena_authority::tick(noRandom(), state);

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the snake continues in its heading
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{8, 9}));
}

/// @brief Ensures that steering one player does not affect the other
TEST(ClassicArenaAuthority, TurnsOnlySteeredSnake) {
  // Given: a turn was steered for player A
  State state = arena({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 3)},
                       {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 3)}},
                      {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: player B keeps its heading
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{10, 4}));
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_B)), (Point{11, 15}));
}

/// @section classic_arena_authority::tick
///
/// Composition and ordering of the complete arena step.

/// @subsection Movement

/// @brief Ensures that dead snakes are not moved, even with a steered turn
TEST(ClassicArenaAuthority, LeavesDeadSnakeInPlace) {
  // Given: a turn was steered for a dead snake
  State state = arena({{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 3))},
                       {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}},
                      {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: the dead snake stays in place
  EXPECT_EQ(snake_model::head(state.snakes.at(PLAYER_A)), (Point{10, 5}));
}

/// @subsection Eating

/// @brief Ensures that eating grows the snake, scores and replenishes food within the same step
TEST(ClassicArenaAuthority, GrowsScoresAndReplenishesWhenEating) {
  // When: the arena steps with food at the next head
  State state = classic_arena_authority::tick(
      makeScriptedRandom({20, 3}),
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}}));

  // Then: the snake grows and scores, and food is replenished
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 4u);
  EXPECT_EQ(state.scores.at(PLAYER_A), 10);
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{20, 3}}));
  EXPECT_EQ(state.food_items.size(), static_cast<size_t>(classic_arena_authority::MIN_FOOD_COUNT));
}

/// @brief Ensures that only one of several food items on the same cell is eaten per step
TEST(ClassicArenaAuthority, EatsOnlyOneOfDuplicateFoodItems) {
  // When: the arena steps with duplicate food at the next head
  // Eating drops below the minimum food count, so one replacement is placed at (20,3)
  State state = classic_arena_authority::tick(
      makeScriptedRandom({20, 3}),
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}, {11, 10}}));

  // Then: only one item is eaten
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{11, 10}, {20, 3}}));
  EXPECT_EQ(state.scores.at(PLAYER_A), 10);
}

/// @brief Ensures that dead snakes do not eat food lying under their head
TEST(ClassicArenaAuthority, DoesNotFeedDeadSnake) {
  // When: the arena steps with food under a dead snake's head
  State state = classic_arena_authority::tick(
      noRandom(),
      arena({{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 10}, Direction::RIGHT, 1))},
             {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}},
            {{10, 10}}));

  // Then: nothing is scored
  EXPECT_EQ(state.scores.at(PLAYER_A), 0);
}

/// @subsection Collisions

/// @brief Ensures that biting yourself is fatal and costs points
TEST(ClassicArenaAuthority, KillsSelfBitingSnake) {
  // Given: player A is steered into its own tail
  State state = arena(
      {{PLAYER_A, snakeAboutToBiteItself()}, {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: it dies and loses ten points
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(state.snakes.at(PLAYER_B)));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, 0}}));
}

/// @brief Pins the ordering effect that a biter eats the first cut segment in the same step
///
/// Collisions run before food drops, and drops before eating: the cut segment under the biter's
/// head is eaten at once, so the victim loses 10 and the biter gains 10 without growing.
TEST(ClassicArenaAuthority, CutsBittenTailOfPlayerB) {
  // When: the arena steps with player A biting player B's tail
  // A moves DOWN from (7,4) to (7,5); B moves RIGHT to body (11,5) (10,5) (9,5) (8,5) (7,5)
  State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{7, 4}, Direction::DOWN, 3)},
                                           {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}},
                                          {}));

  // Then: player B is cut and player A eats the first cut segment
  EXPECT_TRUE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_TRUE(snake_model::alive(state.snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::tail(state.snakes.at(PLAYER_B)), (std::vector<Point>{{10, 5}, {9, 5}, {8, 5}}));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, -10}}));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 3u);
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{}));
}

/// @brief Ensures that tail biting works the same way with Player B as the biter
TEST(ClassicArenaAuthority, CutsBittenTailOfPlayerA) {
  // When: the arena steps with player B biting player A's tail
  State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
                                           {PLAYER_B, snake_model::initial(Point{7, 4}, Direction::DOWN, 3)}},
                                          {}));

  // Then: player A is cut and player B eats the first cut segment
  EXPECT_EQ(snake_model::tail(state.snakes.at(PLAYER_A)), (std::vector<Point>{{10, 5}, {9, 5}, {8, 5}}));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, 10}}));
}

/// @brief Ensures that cut segments not eaten by the biter remain on the board as food
TEST(ClassicArenaAuthority, KeepsRemainingCutSegmentsAsFood) {
  // When: the arena steps with a long cut
  // A moves LEFT from (11,6) to (10,6); B moves UP to body (10,4) (10,5) (10,6) (10,7) (10,8)
  State state = classic_arena_authority::tick(noRandom(),
                                              arena({{PLAYER_A, snake_model::initial(Point{11, 6}, Direction::LEFT, 3)},
                                                     {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::UP, 5)}},
                                                    {}));

  // Then: the remaining cut segments stay as food
  EXPECT_EQ(snake_model::tail(state.snakes.at(PLAYER_B)), (std::vector<Point>{{10, 5}}));
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 7}, {10, 8}}));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, 10}, {PLAYER_B, -10}}));
}

/// @brief Ensures that snakes moving into the same cell both die and leave their bodies as food, one item per cell
TEST(ClassicArenaAuthority, KillsBothSnakesInHeadOnCollision) {
  // When: the arena steps with a head-on approach
  State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{9, 5}, Direction::RIGHT, 2)},
                                           {PLAYER_B, snake_model::initial(Point{11, 5}, Direction::LEFT, 2)}},
                                          {}));

  // Then: both die and their bodies become food
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_B)));
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, -10}}));
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{10, 5}, {9, 5}, {11, 5}}));
}

/// @brief Ensures that mutual tail bites are fatal for both snakes and cut nothing
TEST(ClassicArenaAuthority, KillsBothSnakesInMutualTailBite) {
  // When: the arena steps with snakes passing through each other
  // A moves DOWN from (5,5) to (5,6), B moves UP from (5,6) to (5,5): each head lands on the other's tail
  State state = classic_arena_authority::tick(noRandom(),
                                              arena({{PLAYER_A, snake_model::initial(Point{5, 5}, Direction::DOWN, 4)},
                                                     {PLAYER_B, snake_model::initial(Point{5, 6}, Direction::UP, 4)}},
                                                    {}));

  // Then: both die and nothing is cut
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_B)));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_A)), 4u);
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, -10}}));
}

/// @brief Ensures that a dead snake's body cannot be bitten
TEST(ClassicArenaAuthority, DoesNotCutDeadSnake) {
  // When: the arena steps with a snake moving onto a dead snake
  // A moves DOWN from (7,4) onto the dead body (10,5) ... (6,5) of B
  State state = classic_arena_authority::tick(
      noRandom(),
      arena({{PLAYER_A, snake_model::initial(Point{7, 4}, Direction::DOWN, 3)},
             {PLAYER_B, snake_model::kill(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5))}},
            {}));

  // Then: nothing is cut
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_B)), 5u);
  EXPECT_EQ(state.scores.at(PLAYER_B), 0);
}

/// @brief Ensures that a self-bite is resolved before snake-against-snake collisions
TEST(ClassicArenaAuthority, ResolvesSelfBiteBeforeSnakeVsSnake) {
  // Given: player A is steered into its own tail, which lies on player B
  // A's next head (4,5) is on its own tail and on B's body (4,8) (4,7) (4,6) (4,5) after B moves DOWN
  State state = arena(
      {{PLAYER_A, snakeAboutToBiteItself()}, {PLAYER_B, snake_model::initial(Point{4, 7}, Direction::DOWN, 4)}}, {});
  state = classic_arena_authority::steer(state, steer(PLAYER_A, Direction::UP));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: only the self-bite counts
  EXPECT_FALSE(snake_model::alive(state.snakes.at(PLAYER_A)));
  EXPECT_EQ(snake_model::length(state.snakes.at(PLAYER_B)), 4u);
  EXPECT_EQ(state.scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, 0}}));
}

/// @subsection Dead snakes

namespace {

// A dies in its next step by moving into its own tail; B stays far away
State arenaWithSnakeAboutToDie() {
  return arena(
      {{PLAYER_A, snakeAboutToBiteItself()}, {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}}, {});
}

}  // namespace

/// @brief Ensures that a snake's body becomes food in the step it dies
TEST(ClassicArenaAuthority, DropsBodyOfDyingSnakeAsFood) {
  // Given: player A is steered into its own tail
  State state = classic_arena_authority::steer(arenaWithSnakeAboutToDie(), steer(PLAYER_A, Direction::UP));

  // When: the arena steps
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: its body is dropped as food
  // Body after the fatal move: head (4,5), tail (4,6) (5,6) (5,5) (4,5); the shared cell holds one item
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{{4, 5}, {4, 6}, {5, 6}, {5, 5}}));
}

/// @brief Ensures that a dead snake's body is dropped only once, not again in later steps
TEST(ClassicArenaAuthority, DropsDeadSnakeBodyAsFoodOnlyOnce) {
  // Given: a snake died in the previous step
  State state = classic_arena_authority::steer(arenaWithSnakeAboutToDie(), steer(PLAYER_A, Direction::UP));
  state = classic_arena_authority::tick(noRandom(), state);
  FoodItems food_after_death = state.food_items;

  // When: the arena steps again
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: its body is not dropped again
  EXPECT_EQ(state.food_items, food_after_death);
}

/// @brief Ensures that a snake that is already dead when the arena step starts drops nothing
TEST(ClassicArenaAuthority, DropsNothingForAlreadyDeadSnake) {
  // When: the arena steps with an already dead snake
  State state = classic_arena_authority::tick(
      noRandom(),
      arena({{PLAYER_A, snake_model::kill(snake_model::initial(Point{10, 10}, Direction::RIGHT, 2))},
             {PLAYER_B, snake_model::initial(Point{30, 15}, Direction::RIGHT, 3)}},
            {}));

  // Then: nothing is dropped
  EXPECT_EQ(withoutFiller(state.food_items), (FoodItems{}));
}

/// @subsection Replenishment

/// @brief Ensures that food is topped up to the minimum count within a step
TEST(ClassicArenaAuthority, ReplenishesFoodUpToMinimum) {
  // When: the arena steps below the minimum food
  State state = classic_arena_authority::tick(
      makeScriptedRandom({1, 1, 2, 2, 3, 3}),
      arenaWithExactFood({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{0, 1}, {0, 2}}));

  // Then: food is added up to the minimum
  EXPECT_EQ(state.food_items, (FoodItems{{0, 1}, {0, 2}, {1, 1}, {2, 2}, {3, 3}}));
}

/// @brief Ensures that food above the minimum count is left alone
TEST(ClassicArenaAuthority, DoesNotAddFoodAboveMinimum) {
  // When: the arena steps above the minimum food
  State state = classic_arena_authority::tick(
      noRandom(),
      arenaWithExactFood({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}},
                         {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}}));

  // Then: no food is added
  EXPECT_EQ(state.food_items.size(), 6u);
}

/// @brief Ensures that new food is never placed on a snake cell when a free cell is found
TEST(ClassicArenaAuthority, RetriesFoodPlacementOnSnakeCell) {
  // When: the arena steps and a placement candidate is on a snake
  // After moving, the snake occupies (11,10) (10,10) (9,10); first candidate (10,10) is retried
  State state = classic_arena_authority::tick(
      makeScriptedRandom({10, 10, 1, 1}),
      arenaWithExactFood({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}},
                         {{0, 1}, {0, 2}, {0, 3}, {0, 4}}));

  // Then: placement is retried
  EXPECT_EQ(state.food_items.back(), (Point{1, 1}));
}

/// @brief Ensures that new food is never placed on a cell that already holds food
TEST(ClassicArenaAuthority, RetriesFoodPlacementOnFoodCell) {
  // When: the arena steps and a placement candidate is on existing food
  // First candidate (0,1) already holds food -> retried with (0,5)
  State state = classic_arena_authority::tick(
      makeScriptedRandom({0, 1, 0, 5}),
      arenaWithExactFood({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}},
                         {{0, 1}, {0, 2}, {0, 3}, {0, 4}}));

  // Then: placement is retried
  EXPECT_EQ(state.food_items, (FoodItems{{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}}));
}

/// @brief Ensures that no food is placed when no free cell is found within 100 attempts
TEST(ClassicArenaAuthority, PlacesNoFoodWhenNoCellIsFree) {
  // When: the arena steps and all placement candidates are occupied
  // All 100 attempts hit the snake's head cell (11,10) after moving
  std::vector<int> draws;
  for (int i = 0; i < 100; ++i) {
    draws.push_back(11);
    draws.push_back(10);
  }
  State state = classic_arena_authority::tick(
      makeScriptedRandom(draws),
      arenaWithExactFood({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 1)}},
                         {{0, 1}, {0, 2}, {0, 3}, {0, 4}}));

  // Then: no food is placed
  EXPECT_EQ(state.food_items, (FoodItems{{0, 1}, {0, 2}, {0, 3}, {0, 4}}));
}

/// @brief Ensures that a reposition keeps the food item in place when no free cell is found
TEST(ClassicArenaAuthority, KeepsFoodInPlaceWhenNoCellIsFree) {
  // Given: a reposition was requested
  State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {}));

  // When: the arena steps and all candidates are occupied
  // Draws: item index 0, then 100 candidates on existing food (51,0)
  std::vector<int> draws{0};
  for (int i = 0; i < 100; ++i) {
    draws.push_back(51);
    draws.push_back(0);
  }
  state = classic_arena_authority::tick(makeScriptedRandom(draws), state);

  // Then: the item stays in place
  EXPECT_EQ(state.food_items, (FoodItems{{50, 0}, {51, 0}, {52, 0}, {53, 0}, {54, 0}}));
}

/// @subsection Events

/// @brief Ensures that a step reports what happened as events, in the order they occurred
TEST(ClassicArenaAuthority, ReportsBiteBeforeEating) {
  // When: the arena steps with a bite on another tail
  // A bites B's tail and eats the first cut segment in the same step
  State state =
      classic_arena_authority::tick(noRandom(),
                                    arena({{PLAYER_A, snake_model::initial(Point{7, 4}, Direction::DOWN, 3)},
                                           {PLAYER_B, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)}},
                                          {}));

  // Then: the bite is reported before the eaten food
  ASSERT_EQ(state.events.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Bitten>(state.events[0]));
  EXPECT_EQ(std::get<Bitten>(state.events[0]).victim, PLAYER_B);
  EXPECT_EQ(std::get<Bitten>(state.events[0]).biter, PLAYER_A);
  ASSERT_TRUE(std::holds_alternative<FoodEaten>(state.events[1]));
  EXPECT_EQ(std::get<FoodEaten>(state.events[1]).player, PLAYER_A);
}

/// @brief Ensures that events describe only the latest step
TEST(ClassicArenaAuthority, ReportsOnlyEventsOfLastStep) {
  // Given: a step with events was done
  State state = classic_arena_authority::tick(
      makeScriptedRandom({20, 3}),
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}}));

  // When: the arena steps without incident
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: no events are reported
  EXPECT_TRUE(state.events.empty());
}

/// @section classic_arena_authority::requestFoodReposition
///
/// A reposition request is applied once, on the next step.

/// @brief Ensures that a requested reposition moves one randomly selected food item in the next step
TEST(ClassicArenaAuthority, RepositionsRandomlySelectedFoodItemOnRequest) {
  // Given: a reposition was requested
  State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {}));

  // When: the arena steps
  // Draws: item index 1, then new position (40,7)
  state = classic_arena_authority::tick(makeScriptedRandom({1, 40, 7}), state);

  // Then: a randomly selected food item is repositioned
  EXPECT_EQ(state.food_items, (FoodItems{{50, 0}, {40, 7}, {52, 0}, {53, 0}, {54, 0}}));
}

/// @brief Ensures that a reposition request is consumed by the step that applies it
TEST(ClassicArenaAuthority, RepositionsFoodOnlyOncePerRequest) {
  // Given: a requested reposition was applied
  State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {}));
  state = classic_arena_authority::tick(makeScriptedRandom({0, 40, 7}), state);

  // When: the arena steps again
  state = classic_arena_authority::tick(noRandom(), state);

  // Then: no food is repositioned
  EXPECT_EQ(state.food_items.front(), (Point{40, 7}));
}

/// @brief Ensures that repositioning happens after eating and replenishment within the same step
TEST(ClassicArenaAuthority, RepositionsFoodAfterReplenishing) {
  // Given: a reposition was requested
  State state = classic_arena_authority::requestFoodReposition(
      arena({{PLAYER_A, snake_model::initial(Point{10, 10}, Direction::RIGHT, 3)}}, {{11, 10}}));

  // When: the arena steps with food eaten
  // Draws: replenish one item at (20,3), then reposition index 4 (the replenished item) to (21,4)
  state = classic_arena_authority::tick(makeScriptedRandom({20, 3, 4, 21, 4}), state);

  // Then: food is replenished before it is repositioned
  EXPECT_EQ(state.food_items.back(), (Point{21, 4}));
  EXPECT_EQ(state.scores.at(PLAYER_A), 10);
}

}  // namespace snake
