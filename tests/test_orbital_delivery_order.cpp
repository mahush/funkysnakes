// Tests for the delivery order of Orbital Orders: the same history, delivered to an owner in another order than the
// canonical Domain Application delivers it, leads to the same outcome.
//
// The canonical history comes from the Domain Application: started in a hand-picked world through the test setup seam,
// then driven through its two interfaces and observed through what its players' side received. The owner under test is
// fed those observations in the other order.

#include <gtest/gtest.h>

#include <chrono>
#include <tuple>
#include <utility>

#include "orbital/game_authority.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/orbital_game_domain_application_test_seam.hpp"
#include "orbital_lanes.hpp"
#include "orbital_recording_players.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

namespace app = orbital_game_domain_application;

}  // namespace

/// @section game_authority::observeCommitment

/// @brief Distributed ordering: the Game Authority learns the player's earlier commitment only after the completion
TEST(OrbitalDeliveryOrder, ReachesCanonicalScoreWhenCommitmentReachesGameAuthorityAfterCompletion) {
  // Given: the canonical history, in which G is committed at 1400 ms and F completes at 1600 ms
  RecordingPlayers players;
  app::DomainApplication canonical{players, makeCornerRandom()};
  const arena_authority::RoundSetup setup = makeLanes({Point{5, 2}, Point{7, 2}});
  app::TestSeam::startWithSetup(canonical, setup);
  canonical.submit(app::Submit{PLAYER_A, Point{10, 2}});
  canonical.elapse(app::TimeElapsed{milliseconds{1400}});
  canonical.submit(app::Submit{PLAYER_A, Point{10, 6}});
  canonical.elapse(app::TimeElapsed{milliseconds{400}});
  //        and a Game Authority that learns F's commitment and all steps of that history first
  game_authority::State game = game_authority::observeCommitment(
      game_authority::startRound(setup.round, {PLAYER_A, PLAYER_B}), players.commitments.at(0));
  for (const game_boundary::StepReport& report : players.steps) {
    game = std::get<0>(game_authority::observeStep(
        std::move(game), report.at, report.activations, report.events, {PLAYER_A, PLAYER_B}));
  }

  // When: G's commitment record reaches that Game Authority after F's completion
  game = game_authority::observeCommitment(std::move(game), players.commitments.at(1));

  // Then: its score and chain equal those of the canonical history, which banked F's points and handover bonus
  const game_authority::PlayerScore& canonical_score = players.views->game.players.at(PLAYER_A);
  const game_authority::PlayerScore late_score = game_authority::view(game).players.at(PLAYER_A);
  EXPECT_EQ(late_score.banked, canonical_score.banked);
  EXPECT_EQ(late_score.chain, canonical_score.chain);
  EXPECT_EQ(canonical_score.banked, 20 + 10);
}

}  // namespace orbital
}  // namespace snake
