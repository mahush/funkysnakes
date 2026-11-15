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
#include <vector>

#include "orbital/game_authority.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/orbital_game_domain_application_test_access.hpp"
#include "orbital_lanes.hpp"
#include "orbital_recording_players.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

namespace app = orbital_game_domain_application;

}  // namespace

/// @section game_authority::observeTransmission

/// @brief Distributed ordering: the Game Authority learns the successor's transmission only after its completion
TEST(OrbitalDeliveryOrder, ReachesCanonicalScoreWhenTransmissionReachesGameAuthorityAfterCompletion) {
  // Given: the canonical history, in which F completes at 3400 ms, G is submitted at 3000 ms, transmitted at 4200 ms
  //        and completes at 5200 ms
  RecordingPlayers players;
  app::DomainApplication canonical{players, makeCornerRandom()};
  const arena_authority::RoundSetup setup = makeLanes({Point{5, 2}, Point{7, 2}});
  app::TestAccess::startWithSetup(canonical, setup);
  canonical.submit(app::Submit{PLAYER_A, Point{19, 2}});
  canonical.elapse(app::TimeElapsed{milliseconds{3000}});
  canonical.submit(app::Submit{PLAYER_A, Point{4, 6}});
  canonical.elapse(app::TimeElapsed{milliseconds{2200}});
  //        and a Game Authority that learns F's transmission and all steps of that history first
  std::vector<OrderTransmitted> transmissions;
  for (const game_boundary::StepReport& report : players.steps) {
    transmissions.insert(transmissions.end(), report.transmissions.begin(), report.transmissions.end());
  }
  game_authority::State game = std::get<0>(game_authority::observeTransmission(
      game_authority::startRound(setup.round, {PLAYER_A, PLAYER_B}), transmissions.at(0)));
  for (const game_boundary::StepReport& report : players.steps) {
    game = std::get<0>(game_authority::observeStep(
        std::move(game), report.at, report.activations, report.events, {PLAYER_A, PLAYER_B}));
  }

  // When: G's transmission record reaches that Game Authority after G's completion
  game = std::get<0>(game_authority::observeTransmission(std::move(game), transmissions.at(1)));

  // Then: its score and chain equal those of the canonical history, which banked F's points and G's handover bonus
  const game_authority::PlayerScore& canonical_score = players.views->game.players.at(PLAYER_A);
  const game_authority::PlayerScore late_score = game_authority::view(game).players.at(PLAYER_A);
  EXPECT_EQ(late_score.banked, canonical_score.banked);
  EXPECT_EQ(late_score.chain, canonical_score.chain);
  EXPECT_EQ(canonical_score.banked, 20 + 10);
}

}  // namespace orbital
}  // namespace snake
