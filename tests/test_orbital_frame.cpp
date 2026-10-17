// Tests for the Orbital Frame: terminal presentation of the Orbital Orders views.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <tuple>

#include "orbital/orbital_game_domain_application.hpp"
#include "snake/orbital_frame.hpp"

namespace snake {

using std::chrono::milliseconds;

namespace {

namespace app = orbital::orbital_game_domain_application;
using orbital::RoundId;
using orbital::arena_authority::RoundSetup;
using orbital::arena_authority::SnakeSetup;
using orbital::game_boundary::Views;

RandomIntGeneratorFn makeCornerRandom() {
  return [](int /* min */, int max) { return max; };
}

// Board 20x10: A at (4,2) heading right, B at (4,7) heading right, food at (10,5) and filler in row 9
std::tuple<app::State, Views> started() {
  RoundSetup setup{RoundId{1},
                   Board{20, 10},
                   {{PLAYER_A, SnakeSetup{Point{4, 2}, Direction::RIGHT, 3}},
                    {PLAYER_B, SnakeSetup{Point{4, 7}, Direction::RIGHT, 3}}},
                   {Point{10, 5}, Point{15, 9}, Point{16, 9}, Point{17, 9}, Point{18, 9}}};
  auto [state, observations] = app::startWithSetup(makeCornerRandom(), app::initial(), setup);
  return {std::move(state), *observations.views};
}

std::tuple<app::State, Views> submitted(app::State state, const PlayerId& player, Point target) {
  auto [next, observations] = app::apply(std::move(state), app::Submit{player, target});
  return {std::move(next), *observations.views};
}

std::tuple<app::State, Views> elapsed(app::State state, int ms) {
  auto [next, observations] = app::apply(makeCornerRandom(), std::move(state), app::TimeElapsed{milliseconds{ms}});
  return {std::move(next), *observations.views};
}

// Symbol at a board position; the frame's first line and column are the border
char cellAt(const std::vector<std::string>& frame, Point p) { return frame.at(p.y + 1).at(p.x + 1); }

bool hasLineContaining(const std::vector<std::string>& frame, const std::string& text) {
  for (const std::string& line : frame) {
    if (line.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

/// @section orbital_frame::render

TEST(OrbitalFrame, DrawsSnakesFoodAndCursorsInsideBorder) {
  // Given: a game started
  auto [state, views] = started();

  // When: the frame is rendered with A's cursor at (12,4)
  std::vector<std::string> frame = orbital_frame::render(views, {{PLAYER_A, Point{12, 4}}});

  // Then: heads, tails, food and the cursor appear at their board positions
  EXPECT_EQ(frame.front().substr(0, 22), "+" + std::string(20, '-') + "+");
  EXPECT_EQ(cellAt(frame, Point{4, 2}), 'A');
  EXPECT_EQ(cellAt(frame, Point{3, 2}), 'a');
  EXPECT_EQ(cellAt(frame, Point{4, 7}), 'B');
  EXPECT_EQ(cellAt(frame, Point{10, 5}), '*');
  EXPECT_EQ(cellAt(frame, Point{12, 4}), 'A');
}

/// @brief Players see where their order in transit will lead and when it can activate
TEST(OrbitalFrame, ShowsPendingTargetRouteAndCountdown) {
  // Given: a game started
  auto [state, views] = started();

  // When: A orders the snake to (8,5) at 0 ms, eligible after 400 ms
  auto [next, pending_views] = submitted(state, PLAYER_A, Point{8, 5});
  std::vector<std::string> frame = orbital_frame::render(pending_views, {});

  // Then: the target, the lightweight route from the head at (4,2) and the remaining delay show
  EXPECT_EQ(cellAt(frame, Point{8, 5}), '?');
  EXPECT_EQ(cellAt(frame, Point{5, 2}), '.');
  EXPECT_EQ(cellAt(frame, Point{8, 4}), '.');
  EXPECT_TRUE(hasLineContaining(frame, "pending (8,5) in 0.4s"));
}

/// @brief Players see the route the Arena follows for an executing order
TEST(OrbitalFrame, ShowsExecutingTargetAndRemainingRoute) {
  // Given: A ordered the snake to (8,5)
  auto [state, views] = started();
  auto [with_order, order_views] = submitted(state, PLAYER_A, Point{8, 5});

  // When: the order activated at 400 ms, where the snake moved on to (6,2)
  auto [next, executing_views] = elapsed(with_order, 400);
  std::vector<std::string> frame = orbital_frame::render(executing_views, {});

  // Then: the bold route continues right to (8,2), turns down to the target (8,5)
  EXPECT_EQ(cellAt(frame, Point{7, 2}), '+');
  EXPECT_EQ(cellAt(frame, Point{8, 3}), '+');
  EXPECT_EQ(cellAt(frame, Point{8, 5}), '@');
  EXPECT_TRUE(hasLineContaining(frame, "executing (8,5)"));
}

TEST(OrbitalFrame, ShowsLinkDelayAndRemainingTime) {
  // Given: a game started
  auto [state, views] = started();

  // When: 7.5 s pass, a quarter of the orbital cycle
  auto [next, later_views] = elapsed(state, 7500);
  std::vector<std::string> frame = orbital_frame::render(later_views, {});

  // Then: the delay of 1.7 s is marked halfway up the wedge beside the board, in board row 4 of
  //       rows 0-9 (row 9 is the fast end); the remaining time is as of the last step at 7.4 s
  EXPECT_NE(frame.at(4 + 1).find("< 1.7s"), std::string::npos);
  EXPECT_TRUE(hasLineContaining(frame, "Time left 112.6s"));
}

TEST(OrbitalFrame, ShowsResultOnceGameIsOver) {
  // Given: a game started
  auto [state, views] = started();

  // When: the two minutes pass without anyone scoring
  auto [next, over_views] = elapsed(state, 120000);
  std::vector<std::string> frame = orbital_frame::render(over_views, {});

  // Then: the result is a draw
  EXPECT_TRUE(hasLineContaining(frame, "Game over: draw"));
}

}  // namespace snake
