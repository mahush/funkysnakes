// Tests for the loopback realization of distributed Orbital Orders: the host composition and two phone compositions,
// linked directly, run whole games beside the Domain Application, and what both report is compared.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "domain/common/snake_model.hpp"
#include "domain/orbital/orbital_game_domain_application.hpp"
#include "domain/orbital/pacing_policy.hpp"
#include "orbital_loopback/host_composition.hpp"
#include "orbital_loopback/phone_composition.hpp"
#include "orbital_recording_players.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {
namespace loopback {

using game_boundary::Start;
using game_boundary::Views;
using std::chrono::milliseconds;

namespace {

constexpr RoundId ROUND{1};
constexpr milliseconds T0{1000};  // Host game clock when the game started

RandomIntGeneratorFn makeFixedRandom() {
  return [](int /* min */, int max) { return max; };
}

// The host's side of a device's link, wired directly
struct DirectLink : placement_link::PhoneToHost {
  void forward(const OrderSubmitted& submitted) override { host->forward(submitted); }
  void forward(const OrderTransmitted& transmitted) override { host->forward(transmitted); }

  HostComposition* host{nullptr};
};

// The comparable content of the views: heads, food, executing targets and routes, step; selections and transmitted
// orders; points and remaining steps; expected completions, routes and prospects
auto makeComparableViews(const Views& views) {
  std::map<PlayerId, Point> heads;
  for (const auto& [player, snake] : views.arena.snakes) {
    heads[player] = snake_model::head(snake);
  }
  std::map<PlayerId, std::pair<Point, std::vector<Direction>>> executing;
  for (const auto& [player, active] : views.arena.active) {
    executing[player] = {active.target, active.remaining_route};
  }
  std::map<PlayerId, std::pair<int, int>> points;
  for (const auto& [player, score] : views.game.players) {
    points[player] = {score.banked, score.held};
  }
  // Across players the order of the transmitted orders carries no meaning
  std::vector<OrderTransmitted> transmitted = views.transmitted;
  std::sort(transmitted.begin(), transmitted.end(), [](const OrderTransmitted& lhs, const OrderTransmitted& rhs) {
    return std::make_pair(lhs.player, lhs.order.value) < std::make_pair(rhs.player, rhs.order.value);
  });
  return std::make_tuple(heads,
                         views.arena.food,
                         executing,
                         views.arena.step,
                         views.selections.selections,
                         transmitted,
                         points,
                         views.game.remaining_steps,
                         views.expected_completions,
                         views.selected_routes,
                         views.transmitted_routes,
                         views.handover_prospects);
}

using ComparableViews = decltype(makeComparableViews(std::declval<Views>()));

// Players' submissions by the step they happen in, after the step's opportunity
using SubmissionsByStep = std::map<int, std::vector<std::pair<PlayerId, Point>>>;

/**
 * @brief What the reference and the host reported over a whole game
 */
struct WholeGame {
  std::vector<ComparableViews> reference_views;  // After every step and its submissions
  std::vector<ComparableViews> host_views;
  std::vector<game_authority::HandoverGranted> reference_handovers;  // As reported with the steps, in order
  std::vector<game_authority::HandoverGranted> host_handovers;       // With the steps, then those counted on arrival
  std::vector<game_authority::GameConcluded> reference_conclusions;
  std::vector<game_authority::GameConcluded> host_conclusions;
};

/**
 * @brief Run a whole game in a Domain Application and in a host with two devices wired directly, both with the same
 *        randomness and the same submissions, each device transmitting at its opportunities on time
 */
WholeGame playWholeGame(SubmissionsByStep submissions) {
  namespace app = orbital_game_domain_application;
  RecordingPlayers reference_players;
  app::DomainApplication reference{reference_players, makeFixedRandom()};

  RecordingPlayers host_players;
  DirectLink link_a;
  DirectLink link_b;
  PhoneComposition phone_a{PLAYER_A, link_a};
  PhoneComposition phone_b{PLAYER_B, link_b};
  HostComposition host{host_players, {phone_a, phone_b}, makeFixedRandom()};
  link_a.host = &host;
  link_b.host = &host;
  std::map<PlayerId, PhoneComposition*> phones{{PLAYER_A, &phone_a}, {PLAYER_B, &phone_b}};

  WholeGame game;
  reference.start(app::Start{ROUND});
  host.start(Start{ROUND}, T0);
  for (int step = 1; step <= pacing_policy::LAST_STEP.value; ++step) {
    const milliseconds host_time = T0 + pacing_policy::timeOfStep(StepId{step});
    reference.elapse(app::TimeElapsed{pacing_policy::STEP_INTERVAL});
    host.runDueSteps(host_time);
    phone_a.transmitAtDueOpportunities(host_time);
    phone_b.transmitAtDueOpportunities(host_time);
    for (const auto& [player, target] : submissions[step]) {
      reference.submit(app::Submit{player, target});
      phones.at(player)->submit(target, host_time);
    }
    game.reference_views.push_back(makeComparableViews(*reference_players.views));
    game.host_views.push_back(makeComparableViews(*host_players.views));
  }

  for (const game_boundary::StepReport& step : reference_players.steps) {
    game.reference_handovers.insert(game.reference_handovers.end(), step.handovers.begin(), step.handovers.end());
  }
  for (const game_boundary::StepReport& step : host_players.steps) {
    game.host_handovers.insert(game.host_handovers.end(), step.handovers.begin(), step.handovers.end());
  }
  for (const TransmissionReceipt& receipt : host.record().receipts) {
    game.host_handovers.insert(game.host_handovers.end(), receipt.handovers.begin(), receipt.handovers.end());
  }
  game.reference_conclusions = reference_players.conclusions;
  game.host_conclusions = host_players.conclusions;
  return game;
}

}  // namespace

/// @section whole game
///
/// Whole games of the host and two devices beside the Domain Application

/// @brief With the devices wired directly, the host's views equal the reference's after every step
TEST(OrbitalLoopback, ShowsViewsOfDomainApplicationAfterEveryStep) {
  // When: the reference and the host run the whole game, the same players submitting in the same steps
  WholeGame game = playWholeGame({
      {5, {{PLAYER_A, Point{12, 8}}}},
      {30, {{PLAYER_B, Point{14, 20}}}},
      {45, {{PLAYER_A, Point{20, 3}}, {PLAYER_B, Point{2, 2}}}},
      {47, {{PLAYER_A, Point{2, 25}}}},
      {130, {{PLAYER_A, Point{25, 25}}, {PLAYER_B, Point{1, 14}}}},
      {590, {{PLAYER_B, Point{10, 10}}}},
      {599, {{PLAYER_A, Point{5, 5}}}},
  });

  // Then: the views agree after every step, and both concluded alike
  EXPECT_EQ(game.host_views, game.reference_views);
  ASSERT_EQ(game.host_conclusions.size(), 1U);
  ASSERT_EQ(game.reference_conclusions.size(), 1U);
  EXPECT_EQ(game.host_conclusions.front().result, game.reference_conclusions.front().result);
}

/// @brief Handover scoring and prospects depend on the host's wiring of selections, waiting transmissions and steps
TEST(OrbitalLoopback, CountsHandoverChainAsDomainApplicationDoes) {
  // When: both run the whole game; A's order 1 completes in step 25, its successor 2, submitted in step 23, is
  //       transmitted in step 40 and completes in step 46, and its successor 3, submitted in step 45, is transmitted
  //       in step 59
  WholeGame game = playWholeGame({
      {1, {{PLAYER_A, Point{3, 11}}}},
      {23, {{PLAYER_A, Point{24, 11}}}},
      {45, {{PLAYER_A, Point{14, 11}}}},
  });

  // Then: the reference counts a chain of two handovers, the second with twice the bonus, the host counts the same,
  //       and the views agree after every step
  EXPECT_EQ(game.reference_handovers,
            (std::vector<game_authority::HandoverGranted>{{PLAYER_A, OrderNumber{2}, 1, 10},
                                                          {PLAYER_A, OrderNumber{3}, 2, 20}}));
  EXPECT_EQ(game.host_handovers, game.reference_handovers);
  EXPECT_EQ(game.host_views, game.reference_views);
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
