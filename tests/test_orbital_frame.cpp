// Tests for the Orbital Frame: what the Orbital Orders terminal shows.

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <set>
#include <vector>

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
using orbital_frame::HandoverHint;

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

/// @brief Players see where their selection would lead while they can still change it
TEST_F(OrbitalFrame, ShowsSelectedTargetAndRoute) {
  // Given: a game started
  start();

  // When: A selects (8,5) at 0 ms
  submit(PLAYER_A, Point{8, 5});
  Frame frame = orbital_frame::build(views(), {});

  // Then: the target and the selected route from the head at (4,2) show
  EXPECT_EQ(cellAt(frame, Point{8, 5}), (Cell{CellRole::TARGET, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{5, 2}), (Cell{CellRole::SELECTED_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 4}), (Cell{CellRole::SELECTED_ROUTE, PLAYER_A}));
  EXPECT_EQ(frame.players.at(PLAYER_A).selected_target, (Point{8, 5}));
  EXPECT_FALSE(frame.players.at(PLAYER_A).transmitted_target.has_value());
}

/// @brief Players see that their order has left and can no longer be changed
TEST_F(OrbitalFrame, ShowsSentTargetAndRoute) {
  // Given: A selected (8,5) at 0 ms
  start();
  submit(PLAYER_A, Point{8, 5});

  // When: the opportunity at 200 ms sent it, with the head moved on to (5,2)
  elapse(200);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the sent route from the head shows, and nothing is selected any more
  EXPECT_EQ(cellAt(frame, Point{6, 2}), (Cell{CellRole::SENT_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 4}), (Cell{CellRole::SENT_ROUTE, PLAYER_A}));
  EXPECT_EQ(frame.players.at(PLAYER_A).transmitted_target, (Point{8, 5}));
  EXPECT_FALSE(frame.players.at(PLAYER_A).selected_target.has_value());
}

/// @brief Players see the route the Arena Authority follows for an executing order
TEST_F(OrbitalFrame, ShowsExecutingTargetAndRemainingRoute) {
  // Given: A ordered the snake to (8,5)
  start();
  submit(PLAYER_A, Point{8, 5});

  // When: the order activated at 400 ms, when the snake moved on to (6,2)
  elapse(400);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the route continues right to (8,2), turns down to the target (8,5)
  EXPECT_EQ(cellAt(frame, Point{7, 2}), (Cell{CellRole::ACTIVE_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 3}), (Cell{CellRole::ACTIVE_ROUTE, PLAYER_A}));
  EXPECT_EQ(cellAt(frame, Point{8, 5}), (Cell{CellRole::TARGET, PLAYER_A}));
  EXPECT_EQ(frame.players.at(PLAYER_A).executing_target, (Point{8, 5}));
}

/// @brief Players see the chain their successors built and what the next handover is worth
TEST_F(OrbitalFrame, ShowsHandoverChainAndNextBonus) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms, and submitted a successor to (6,4) at 2800 ms
  start();
  submit(PLAYER_A, Point{19, 2});
  elapse(2800);
  submit(PLAYER_A, Point{6, 4});

  // When: the successor, sent at 4200 ms, took over at 4400 ms with the snake at (5,2) and reached (6,4) at 4800 ms
  elapse(2000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the chain has one handover, its bonus banked, and the next one would earn double
  EXPECT_EQ(frame.players.at(PLAYER_A).chain, 1);
  EXPECT_EQ(frame.players.at(PLAYER_A).banked, 10);
  EXPECT_EQ(frame.players.at(PLAYER_A).next_handover, 20);
}

/// @subsection uplink
///
/// The 10 rows of the board leave 8 timeline rows beside the labels: 8 overview stretches of 15 s and 16 lane steps.

TEST_F(OrbitalFrame, ShowsRemainingTime) {
  // Given: a game started
  start();

  // When: 3 s pass
  elapse(3000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: 117 s remain
  EXPECT_EQ(frame.remaining, milliseconds{117000});
}

/// @brief The lane shows each coming step, so players see an opportunity approach step by step
TEST_F(OrbitalFrame, ShowsComingOpportunityAtItsStepInLane) {
  // Given: a game started
  start();

  // When: 2 s pass; the next opportunity comes at 4.2 s, the eleventh step from now
  elapse(2000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the lane covers 3.2 s and marks the eleventh step only
  ASSERT_EQ(frame.uplink.lane.size(), 16U);
  EXPECT_EQ(frame.uplink.lane_span, milliseconds{3200});
  for (std::size_t slot = 0; slot < frame.uplink.lane.size(); ++slot) {
    EXPECT_EQ(frame.uplink.lane[slot].opportunity, slot == 10) << "slot " << slot;
  }
}

TEST_F(OrbitalFrame, MovesOpportunityOneLaneSlotPerStep) {
  // Given: 2 s passed
  start();
  elapse(2000);

  // When: one more step runs
  elapse(200);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the opportunity at 4.2 s is now the tenth step from now
  EXPECT_TRUE(frame.uplink.lane.at(9).opportunity);
  EXPECT_FALSE(frame.uplink.lane.at(10).opportunity);
}

/// @brief The overview shows the link growing busier: sparse opportunities at the start of the game, dense at its end
TEST_F(OrbitalFrame, ShowsSparseStartAndDenseEndInOverview) {
  // When: a game starts
  start();
  Frame frame = orbital_frame::build(views(), {});

  // Then: the first 15 s hold fewer opportunities than the last 15 s, both of 75 steps, and now lies in the first
  ASSERT_EQ(frame.uplink.overview.size(), 8U);
  EXPECT_EQ(frame.uplink.overview[0].steps, 75);
  EXPECT_EQ(frame.uplink.overview[7].steps, 75);
  EXPECT_LT(frame.uplink.overview[0].opportunities, frame.uplink.overview[7].opportunities);
  EXPECT_TRUE(frame.uplink.overview[0].now);
}

/// @brief The stretches are fixed for the whole game: time passing between two steps must not move their bounds
TEST_F(OrbitalFrame, KeepsOverviewStretchesBetweenSteps) {
  // Given: a game on a board of 9 rows, whose 7 overview stretches do not end on step times
  app::TestAccess::startWithSetup(game_,
                                  RoundSetup{RoundId{1},
                                             StartLayout{Board{20, 9},
                                                         {{PLAYER_A, SnakeSetup{Point{4, 2}, Direction::RIGHT, 3}},
                                                          {PLAYER_B, SnakeSetup{Point{4, 7}, Direction::RIGHT, 3}}}},
                                             {Point{15, 8}, Point{16, 8}, Point{17, 8}, Point{18, 8}, Point{10, 5}}});
  const std::vector<orbital_frame::OverviewRow> at_start = orbital_frame::build(views(), {}).uplink.overview;

  // When: 190 ms pass, not yet reaching the first step
  elapse(190);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the overview is the same as at the start
  EXPECT_EQ(frame.uplink.overview, at_start);
}

TEST_F(OrbitalFrame, MovesNowUpThroughOverview) {
  // Given: a game started
  start();

  // When: 29 s pass
  elapse(29000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the first 15 s have elapsed and now lies in the second stretch
  EXPECT_TRUE(frame.uplink.overview[0].elapsed);
  EXPECT_FALSE(frame.uplink.overview[0].now);
  EXPECT_TRUE(frame.uplink.overview[1].now);
  EXPECT_FALSE(frame.uplink.overview[1].elapsed);
}

/// @brief Players see their order's remaining execution next to the coming opportunities: the first opportunity above
/// its end sends the successor
TEST_F(OrbitalFrame, ShowsRemainingExecutionUpToExpectedArrivalInLane) {
  // Given: A ordered the snake to (8,5)
  start();
  submit(PLAYER_A, Point{8, 5});

  // When: the order activated at 400 ms with the snake at (6,2), five moves before the target
  elapse(400);
  Frame frame = orbital_frame::build(views(), {});

  // Then: the lane marks A's execution in the five coming steps, up to the arrival at 1400 ms
  for (std::size_t slot = 0; slot < 5; ++slot) {
    EXPECT_EQ(frame.uplink.lane.at(slot).executing, (std::set<PlayerId>{PLAYER_A})) << "slot " << slot;
  }
  EXPECT_TRUE(frame.uplink.lane.at(5).executing.empty());
}

/// @subsection handover prompt

/// @brief With no opportunity before the arrival, a successor chosen now is sent at the first one after it
TEST_F(OrbitalFrame, PromptsHandoverWhenNoOpportunityPrecedesArrival) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms; the next opportunity comes at 4200 ms
  start();
  submit(PLAYER_A, Point{19, 2});

  // When: 1 s passes
  elapse(1000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: A is prompted to choose a successor now
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::CHOOSE_NOW);
}

/// @brief A successor chosen before the last opportunity before the arrival would be sent there and interrupt
TEST_F(OrbitalFrame, HoldsHandoverPromptWhileOpportunityPrecedesArrival) {
  // Given: A ordered the snake the long way round to (3,5), arriving at 4600 ms, after the opportunity at 4200 ms
  start();
  submit(PLAYER_A, Point{3, 5});

  // When: 1 s passes
  elapse(1000);
  Frame frame = orbital_frame::build(views(), {});

  // Then: A is not prompted yet
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::NONE);
}

/// @brief After the arrival a successor still hands over until the opportunity that would send it
TEST_F(OrbitalFrame, KeepsHandoverPromptAfterArrivalUntilOpportunity) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms
  start();
  submit(PLAYER_A, Point{19, 2});

  // When: 3.4 s pass, after the arrival and before the opportunity at 4200 ms
  elapse(3400);
  Frame frame = orbital_frame::build(views(), {});

  // Then: A is still prompted to choose a successor
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::CHOOSE_NOW);
}

TEST_F(OrbitalFrame, ClosesHandoverPromptAtOpportunityAfterArrival) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms
  start();
  submit(PLAYER_A, Point{19, 2});

  // When: 4.4 s pass, after the opportunity at 4200 ms
  elapse(4400);
  Frame frame = orbital_frame::build(views(), {});

  // Then: A is no longer prompted
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::NONE);
}

/// @brief A successor chosen in the window is shown as ready, so the player knows not to change it
TEST_F(OrbitalFrame, ShowsSuccessorReadyForQualifyingSelection) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms
  start();
  submit(PLAYER_A, Point{19, 2});
  elapse(1000);

  // When: A selects a successor at 1000 ms
  submit(PLAYER_A, Point{6, 4});
  Frame frame = orbital_frame::build(views(), {});

  // Then: the successor is ready
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::SUCCESSOR_READY);
}

/// @brief Changing one's mind costs the handover, so the panel stops promising it
TEST_F(OrbitalFrame, PromisesNoHandoverForReplacingSelection) {
  // Given: A ordered the snake to (19,2), arriving at 3000 ms, and selected a successor at 1000 ms
  start();
  submit(PLAYER_A, Point{19, 2});
  elapse(1000);
  submit(PLAYER_A, Point{6, 4});
  elapse(100);

  // When: A replaces the successor
  submit(PLAYER_A, Point{8, 4});
  Frame frame = orbital_frame::build(views(), {});

  // Then: no handover is promised
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::NONE);
}

/// @brief A successor chosen after the predecessor was sent but before it activated cannot hand over from it
TEST_F(OrbitalFrame, PromisesNoHandoverForSelectionBeforePredecessorActivated) {
  // Given: A's order to (19,2) was sent at 200 ms and activates at 400 ms; A selected a successor at 200 ms
  start();
  submit(PLAYER_A, Point{19, 2});
  elapse(200);
  submit(PLAYER_A, Point{6, 4});

  // When: time passes to 1000 ms, within the window around the arrival at 3000 ms
  elapse(800);
  Frame frame = orbital_frame::build(views(), {});

  // Then: no handover is promised
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::NONE);
}

/// @brief An order already sent takes over next; whatever is chosen now follows that order, not the executing one
TEST_F(OrbitalFrame, PromisesNoHandoverWhileSentOrderIsAboutToTakeOver) {
  // Given: A ordered the snake the long way round to (3,5), arriving at 4600 ms, and selected a successor at 3000 ms
  start();
  submit(PLAYER_A, Point{3, 5});
  elapse(3000);
  submit(PLAYER_A, Point{6, 4});

  // When: the opportunity at 4200 ms sent the successor, which will interrupt at 4400 ms
  elapse(1200);
  Frame frame = orbital_frame::build(views(), {});

  // Then: no handover is promised
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_hint, HandoverHint::NONE);
}

/// @subsection handover flash

TEST_F(OrbitalFrame, ShowsBonusOfFlashingHandover) {
  // Given: a game started
  start();

  // When: the frame is built while A's handover worth 20 is to be highlighted
  Frame frame = orbital_frame::build(views(), {}, {{PLAYER_A, 20}});

  // Then: A's panel carries the bonus and B's none
  EXPECT_EQ(frame.players.at(PLAYER_A).handover_flash, 20);
  EXPECT_FALSE(frame.players.at(PLAYER_B).handover_flash.has_value());
}

/// @subsection game over

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
