// Tests for the Orbital Frame: what the Orbital Orders terminal shows.

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/orbital_game_domain_application_test_access.hpp"
#include "orbital_recording_players.hpp"
#include "snake/orbital_frame.hpp"

namespace snake {

using std::chrono::milliseconds;

namespace {

namespace app = orbital::orbital_game_domain_application;
using orbital::RecordingPlayers;
using orbital::RoundId;
using orbital::arena_authority::RoundSetup;
using orbital::game_boundary::Views;

RandomIntGeneratorFn makeCornerRandom() {
  return [](int /* min */, int max) { return max; };
}

using orbital_frame::Cell;
using orbital_frame::CellRole;
using orbital_frame::Frame;

const Cell& cellAt(const Frame& frame, Point p) { return frame.cells.at(p.y).at(p.x); }

/**
 * @brief A game played through the Domain Application, showing the views its players' side received
 */
class OrbitalFrame : public ::testing::Test {
 protected:
  // Board 20x10: A at (4,2) heading right, B at (4,7) heading right, food at (10,5) and filler in row 9
  void start() {
    app::TestAccess::startWithSetup(game_,
                                    RoundSetup{RoundId{1},
                                               StartLayout{Board{20, 10},
                                                           {{PLAYER_A, SnakeSetup{Point{4, 2}, Direction::RIGHT, 3}},
                                                            {PLAYER_B, SnakeSetup{Point{4, 7}, Direction::RIGHT, 3}}}},
                                               {Point{10, 5}, Point{15, 9}, Point{16, 9}, Point{17, 9}, Point{18, 9}}});
  }

  void submit(const PlayerId& player, Point target) { game_.submit(app::Submit{player, target}); }

  void elapse(int ms) { game_.elapse(app::TimeElapsed{milliseconds{ms}}); }

  const Views& views() const { return *players_.views; }

  RecordingPlayers players_;
  app::DomainApplication game_{players_, makeCornerRandom()};
};

}  // namespace

/// @section orbital_frame::build

TEST_F(OrbitalFrame, PlacesSnakesFoodAndCursorsOnBoard) {
  // Given: a game started
  start();

  // When: the frame is built with A's cursor at (12,4)
  Frame frame = orbital_frame::build(views(), {{PLAYER_A, Point{12, 4}}});

  // Then: heads, tails, food and the cursor appear at their board positions
  EXPECT_EQ(frame.cells.size(), 10U);
  EXPECT_EQ(frame.cells.front().size(), 20U);
  EXPECT_EQ(cellAt(frame, Point{4, 2}), (Cell{CellRole::SNAKE_HEAD, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{3, 2}), (Cell{CellRole::SNAKE_BODY, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{4, 7}), (Cell{CellRole::SNAKE_HEAD, PLAYER_B}));
  EXPECT_EQ(cellAt(frame, Point{10, 5}), (Cell{CellRole::FOOD, std::nullopt}));
  EXPECT_EQ(cellAt(frame, Point{12, 4}), (Cell{CellRole::CURSOR, PLAYER_A}));
}

/// @brief Players see where their order in transit will lead and when it can activate
TEST_F(OrbitalFrame, ShowsPendingTargetRouteAndCountdown) {
  // Given: a game started
  start();

  // When: A orders the snake to (8,5) at 0 ms, released at 200 ms
  submit(PLAYER_A, Point{8, 5});
  Frame frame = orbital_frame::build(views(), {});

  // Then: the target, the route from the head at (4,2) and the remaining delay show
  EXPECT_EQ(cellAt(frame, Point{8, 5}), (Cell{CellRole::TARGET, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{5, 2}), (Cell{CellRole::PENDING_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 4}), (Cell{CellRole::PENDING_ROUTE, PLAYER_A}));
  EXPECT_EQ(frame.players.at(PLAYER_A).pending_target, (Point{8, 5}));
  EXPECT_EQ(frame.players.at(PLAYER_A).pending_remaining, milliseconds{200});
}

/// @brief Players see the route the Arena Authority follows for an executing order
TEST_F(OrbitalFrame, ShowsExecutingTargetAndRemainingRoute) {
  // Given: A ordered the snake to (8,5)
  start();
  submit(PLAYER_A, Point{8, 5});

  // When: the order activated at 200 ms and by 400 ms the snake moved on to (6,2)
  elapse(400);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the route continues right to (8,2), turns down to the target (8,5)
  EXPECT_EQ(cellAt(frame, Point{7, 2}), (Cell{CellRole::ACTIVE_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 3}), (Cell{CellRole::ACTIVE_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 5}), (Cell{CellRole::TARGET, PLAYER_A}));
  EXPECT_EQ(frame.players.at(PLAYER_A).executing_target, (Point{8, 5}));
}

/// @brief Players see the chain their prepared successors built and what the next handover is worth
TEST_F(OrbitalFrame, ShowsHandoverChainAndNextBonus) {
  // Given: A ordered the snake to (8,5), arriving at 1400 ms, and committed a successor at 1200 ms
  start();
  submit(PLAYER_A, Point{8, 5});
  elapse(1200);
  submit(PLAYER_A, Point{12, 5});

  // When: the order arrives while the successor is pending
  elapse(200);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the chain has one handover, its bonus banked, and the next one would earn double
  EXPECT_EQ(frame.players.at(PLAYER_A).chain, 1);
  EXPECT_EQ(frame.players.at(PLAYER_A).banked, 10);
  EXPECT_EQ(frame.players.at(PLAYER_A).next_handover, 20);
}

TEST_F(OrbitalFrame, ShowsLinkDelayAndRemainingTime) {
  // Given: a game started
  start();

  // When: 15 s pass, a quarter of the orbital cycle
  elapse(15000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the delay is 1.1 s within 0.2-2 s and 105 s remain
  EXPECT_EQ(frame.delay.now, milliseconds{1100});
  EXPECT_EQ(frame.delay.min, milliseconds{200});
  EXPECT_EQ(frame.delay.max, milliseconds{2000});
  EXPECT_EQ(frame.remaining, milliseconds{105000});
}

TEST_F(OrbitalFrame, ShowsResultOnceGameIsOver) {
  // Given: a game started
  start();

  // When: the two minutes pass without anyone scoring
  elapse(120000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the game is over without a winner
  ASSERT_TRUE(frame.game_over.has_value());
  EXPECT_EQ(frame.game_over->winner, std::nullopt);
}

}  // namespace snake
