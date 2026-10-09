// Tests for the Snake Authority (snake_model): evolution of one snake's body and life.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "classic/snake_model_evolve.hpp"
#include "test_printers.hpp"

namespace snake {

namespace {

const Board BOARD{60, 20};

}  // namespace

/// @section snake_model
///
/// Evolution of one snake's body and life.

/// @subsection initial and kill

/// @brief Ensures that a new snake is laid out straight behind its head
TEST(SnakeModel, ExtendsInitialBodyBackwardsFromHead) {
  // When: a snake is created
  snake_model::Snake s = snake_model::initial(Point{5, 10}, Direction::RIGHT, 3);

  // Then: its body extends backwards from the head
  EXPECT_EQ(snake_model::head(s), (Point{5, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{4, 10}, {3, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
  EXPECT_TRUE(snake_model::alive(s));
}

/// @brief Ensures that killing a snake marks it dead
TEST(SnakeModel, MarksKilledSnakeDead) {
  // When: the snake is killed
  snake_model::Snake s = snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3));

  // Then: it is dead
  EXPECT_FALSE(snake_model::alive(s));
}

/// @subsection move and grow

/// @brief Ensures that moving advances the head and keeps the snake's length
TEST(SnakeModel, MovesByAdvancingHeadAndKeepingLength) {
  // When: the snake moves
  snake_model::Snake s =
      snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::DOWN, BOARD);

  // Then: the head advances and the length stays
  EXPECT_EQ(snake_model::head(s), (Point{5, 11}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::DOWN);
}

/// @brief Ensures that a snake of length two or more cannot reverse into its own body
TEST(SnakeModel, IgnoresReverseTurn) {
  // When: the snake moves in its reverse direction
  snake_model::Snake s =
      snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 2), Direction::LEFT, BOARD);

  // Then: it keeps its heading
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::RIGHT);
}

/// @brief Ensures that a snake of length one may reverse, as it has no body to run into
TEST(SnakeModel, ReversesSnakeOfLengthOne) {
  // When: a snake of length one moves in its reverse direction
  snake_model::Snake s =
      snake_model::move(snake_model::initial(Point{5, 10}, Direction::RIGHT, 1), Direction::LEFT, BOARD);

  // Then: it reverses
  EXPECT_EQ(snake_model::head(s), (Point{4, 10}));
  EXPECT_EQ(snake_model::currentDirection(s), Direction::LEFT);
}

/// @brief Ensures that dead snakes do not move
TEST(SnakeModel, DoesNotMoveDeadSnake) {
  // When: a dead snake moves
  snake_model::Snake s = snake_model::move(
      snake_model::kill(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3)), Direction::RIGHT, BOARD);

  // Then: it stays in place
  EXPECT_EQ(snake_model::head(s), (Point{5, 10}));
}

/// @brief Ensures that growing advances the head and keeps the tail tip
TEST(SnakeModel, GrowsByKeepingTailTip) {
  // When: the snake grows
  snake_model::Snake s =
      snake_model::grow(snake_model::initial(Point{5, 10}, Direction::RIGHT, 3), Direction::RIGHT, BOARD);

  // Then: the head advances and the tail tip stays
  EXPECT_EQ(snake_model::head(s), (Point{6, 10}));
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{5, 10}, {4, 10}, {3, 10}}));
}

/// @brief Ensures that dead snakes do not grow
TEST(SnakeModel, DoesNotGrowDeadSnake) {
  // When: a dead snake grows
  snake_model::Snake s = snake_model::grow(
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

class SnakeModelWrap : public ::testing::TestWithParam<WrapCase> {};

}  // namespace

/// @brief Ensures that the board wraps around at every edge
TEST_P(SnakeModelWrap, WrapsAtBoardEdge) {
  // When: the next head is computed towards a board edge
  Point next = snake_model::nextHead(snake_model::initial(GetParam().head, GetParam().dir, 3), GetParam().dir, BOARD);

  // Then: it wraps to the opposite edge
  EXPECT_EQ(next, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(AllEdges,
                         SnakeModelWrap,
                         ::testing::Values(WrapCase{"Right", {59, 5}, Direction::RIGHT, {0, 5}},
                                           WrapCase{"Left", {0, 5}, Direction::LEFT, {59, 5}},
                                           WrapCase{"Top", {5, 0}, Direction::UP, {5, 19}},
                                           WrapCase{"Bottom", {5, 19}, Direction::DOWN, {5, 0}}),
                         [](const ::testing::TestParamInfo<WrapCase>& info) { return info.param.edge; });

/// @subsection cutAt

/// @brief Ensures that cutting a tail returns the segments from the cut point onwards
TEST(SnakeModel, CutsOffSegmentsFromCutPointOnwards) {
  // When: the snake is cut at a tail point
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{7, 5});

  // Then: the segments from that point onwards are cut off
  EXPECT_EQ(snake_model::tail(s), (std::vector<Point>{{9, 5}, {8, 5}}));
  EXPECT_EQ(cut, (std::vector<Point>{{7, 5}, {6, 5}}));
}

/// @brief Ensures that a head cannot be cut off
TEST(SnakeModel, KeepsSnakeWhenCutAtHead) {
  // When: the snake is cut at its head
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{10, 5});

  // Then: the snake is unchanged and nothing is cut off
  EXPECT_EQ(snake_model::length(s), 5u);
  EXPECT_TRUE(cut.empty());
}

/// @brief Ensures that cutting outside the body has no effect
TEST(SnakeModel, KeepsSnakeWhenCutOutsideBody) {
  // When: the snake is cut at a point outside its body
  auto [s, cut] = snake_model::cutAt(snake_model::initial(Point{10, 5}, Direction::RIGHT, 5), Point{0, 0});

  // Then: the snake is unchanged and nothing is cut off
  EXPECT_EQ(snake_model::length(s), 5u);
  EXPECT_TRUE(cut.empty());
}

}  // namespace snake
