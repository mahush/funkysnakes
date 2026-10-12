// Tests for the Pursuit Policy: head, heading and target -> route of moves.

#include <gtest/gtest.h>

#include <vector>

#include "common/snake_model_evolve.hpp"
#include "orbital/pursuit_policy.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {

namespace {

using Route = std::vector<Direction>;

constexpr Direction U = Direction::UP;
constexpr Direction D = Direction::DOWN;
constexpr Direction L = Direction::LEFT;
constexpr Direction R = Direction::RIGHT;

}  // namespace

/// @section pursuit_policy::planRoute

/// @subsection route shape

TEST(PursuitPolicy, FollowsHeadingUntilAlignedThenTurnsOnce) {
  // When: a route to a target ahead and below is planned
  Route route = pursuit_policy::planRoute(Point{2, 2}, R, Point{5, 4}, Board{10, 10});

  // Then: the snake keeps its heading for three moves, then turns down for two
  EXPECT_EQ(route, (Route{R, R, R, D, D}));
}

/// @brief An already aligned snake turns at once: the first leg has length zero
TEST(PursuitPolicy, TurnsImmediatelyWhenAlreadyAligned) {
  // When: a route to a target straight above the head is planned
  Route route = pursuit_policy::planRoute(Point{2, 5}, R, Point{2, 3}, Board{10, 10});

  // Then: the snake turns up immediately
  EXPECT_EQ(route, (Route{U, U}));
}

TEST(PursuitPolicy, GoesStraightToTargetOnTheLineAhead) {
  // When: a route to a target in the row ahead is planned
  Route route = pursuit_policy::planRoute(Point{2, 2}, R, Point{4, 2}, Board{10, 10});

  // Then: the snake goes straight
  EXPECT_EQ(route, (Route{R, R}));
}

/// @brief The first leg only moves forwards, so a target behind on the same line is reached through the edge
TEST(PursuitPolicy, WrapsAroundToTargetOnTheLineBehind) {
  // When: a route to a target in the row behind is planned
  Route route = pursuit_policy::planRoute(Point{2, 2}, R, Point{0, 2}, Board{5, 5});

  // Then: the snake goes straight on through the edge
  EXPECT_EQ(route, (Route{R, R, R}));
}

TEST(PursuitPolicy, PlansEmptyRouteWhenHeadIsAtTarget) {
  // When: a route to the head's own position is planned
  Route route = pursuit_policy::planRoute(Point{3, 3}, U, Point{3, 3}, Board{10, 10});

  // Then: there is nothing to move
  EXPECT_TRUE(route.empty());
}

/// @subsection final leg

TEST(PursuitPolicy, TakesShorterWrappedWayOnFinalLeg) {
  // When: a route to a target that is closer across the top edge is planned
  Route route = pursuit_policy::planRoute(Point{2, 1}, R, Point{2, 8}, Board{10, 10});

  // Then: the snake turns up and wraps instead of going down seven cells
  EXPECT_EQ(route, (Route{U, U, U}));
}

namespace {

struct TieCase {
  Direction heading;
  Point target;
  Route expected_route;
};

class PursuitPolicyTie : public ::testing::TestWithParam<TieCase> {};

}  // namespace

/// @brief Equally long ways across are a fixed convention so that routes are deterministic
TEST_P(PursuitPolicyTie, TurnsClockwiseWhenBothWaysAreEqual) {
  // When: a route to a target half the board away across the heading is planned
  Route route = pursuit_policy::planRoute(Point{2, 2}, GetParam().heading, GetParam().target, Board{4, 4});

  // Then: the snake turns clockwise relative to its heading
  EXPECT_EQ(route, GetParam().expected_route);
}

INSTANTIATE_TEST_SUITE_P(Headings,
                         PursuitPolicyTie,
                         ::testing::Values(TieCase{R, Point{2, 0}, Route{D, D}},
                                           TieCase{D, Point{0, 2}, Route{L, L}},
                                           TieCase{L, Point{2, 0}, Route{U, U}},
                                           TieCase{U, Point{0, 2}, Route{R, R}}));

/// @subsection agreement with snake movement

namespace {

struct ReachCase {
  Direction heading;
  Point target;
};

class PursuitPolicyReach : public ::testing::TestWithParam<ReachCase> {};

}  // namespace

/// @brief The Arena executes routes with the snake model, which ignores reversals for longer snakes
TEST_P(PursuitPolicyReach, LeadsLongSnakeToTarget) {
  // Given: a snake of length four heading in some direction
  snake_model::Snake snake = snake_model::initial(Point{3, 3}, GetParam().heading, 4);

  // When: the snake follows a planned route move by move
  Route route = pursuit_policy::planRoute(Point{3, 3}, GetParam().heading, GetParam().target, Board{7, 6});
  for (Direction move : route) {
    snake = snake_model::move(snake, move, Board{7, 6});
  }

  // Then: its head ends on the target
  EXPECT_EQ(snake_model::head(snake), GetParam().target);
}

INSTANTIATE_TEST_SUITE_P(TargetsAroundTheHead,
                         PursuitPolicyReach,
                         ::testing::Values(ReachCase{R, Point{1, 1}},
                                           ReachCase{R, Point{5, 5}},
                                           ReachCase{L, Point{0, 3}},
                                           ReachCase{L, Point{6, 0}},
                                           ReachCase{U, Point{3, 5}},
                                           ReachCase{U, Point{0, 4}},
                                           ReachCase{D, Point{6, 2}},
                                           ReachCase{D, Point{3, 0}}));

}  // namespace orbital
}  // namespace snake
