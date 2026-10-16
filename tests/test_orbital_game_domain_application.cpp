// Tests for the Orbital Game Domain Application: whole-game histories across Order, Arena and Game.

#include <gtest/gtest.h>

#include <chrono>
#include <tuple>

#include "common/snake_model.hpp"
#include "orbital/latency_policy.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

namespace app = orbital_game_domain_application;
using arena_authority::RoundSetup;
using arena_authority::SnakeSetup;
using game_boundary::SubmissionAccepted;
using game_boundary::SubmissionRejected;
using game_boundary::SubmissionRejectionReason;
using order_authority::Status;

constexpr RoundId ROUND{1};

/**
 * @brief Random source that always picks the far corner of the board for new food
 */
RandomIntGeneratorFn makeCornerRandom() {
  return [](int /* min */, int max) { return max; };
}

/**
 * @brief Board 20x10: A in row 2 heading right, B in row 7 heading right, the given food
 *
 * Filler food in row 9 brings the food to the minimum without random draws.
 */
RoundSetup makeLanes(FoodItems food) {
  for (int x = 15; food.size() < static_cast<size_t>(arena_authority::MIN_FOOD_COUNT); ++x) {
    food.push_back(Point{x, 9});
  }
  return RoundSetup{ROUND,
                    Board{20, 10},
                    {{PLAYER_A, SnakeSetup{Point{2, 2}, Direction::RIGHT, 3}},
                     {PLAYER_B, SnakeSetup{Point{2, 7}, Direction::RIGHT, 3}}},
                    food};
}

app::State started(const RoundSetup& setup) {
  return std::get<0>(app::startWithSetup(makeCornerRandom(), app::initial(), setup));
}

app::State elapse(app::State state, int ms) {
  return std::get<0>(app::apply(makeCornerRandom(), std::move(state), app::TimeElapsed{milliseconds{ms}}));
}

std::tuple<app::State, OrderId> submit(app::State state, const PlayerId& player, Point target) {
  auto [next, observations] = app::apply(std::move(state), app::Submit{player, target});
  return {std::move(next), std::get<SubmissionAccepted>(*observations.submission).order};
}

Status statusOf(const app::State& state, OrderId id) {
  for (const order_authority::OrderRecord& record : order_authority::view(*state.orders).orders) {
    if (record.order.id == id) {
      return record.status;
    }
  }
  ADD_FAILURE() << "order " << id.value << " is not known";
  return Status::PENDING;
}

game_authority::PlayerScore scoreOf(const app::State& state, const PlayerId& player) {
  return game_authority::view(*state.game).players.at(player);
}

}  // namespace

/// @section orbital_game_domain_application::apply
///
/// Whole-game histories: Order eligibility → Arena execution → Game scoring.
/// Timing: an order issued at 0 ms is eligible after 200 ms; steps run every 200 ms.

/// @subsection start

/// @brief A square board looks square in the terminal, where each cell is drawn two columns wide
TEST(OrbitalGameDomainApplication, StartsOnSquareBoardWithSnakesSideBySide) {
  // When: a game starts with the default setup
  auto [state, observations] = app::apply(makeCornerRandom(), app::initial(), app::Start{RoundId{1}});

  // Then: the board is 28x28 and both snakes start heading right, five rows apart
  const arena_authority::View& arena = observations.views->arena;
  EXPECT_EQ(arena.board.width, 28);
  EXPECT_EQ(arena.board.height, 28);
  EXPECT_EQ(snake_model::head(arena.snakes.at(PLAYER_A)), (Point{5, 11}));
  EXPECT_EQ(snake_model::head(arena.snakes.at(PLAYER_B)), (Point{5, 16}));
}

/// @subsection unbanked points

/// @brief F's food points count once F completes; no successor was prepared, so no handover bonus
TEST(OrbitalGameDomainApplication, BanksPointsOfOrderThatCompletes) {
  // Given: F to (10,2) activates at 200 ms and collects food at (5,2) and (7,2)
  auto [with_f, f] = submit(started(makeLanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});

  // When: time passes until F has arrived at 1600 ms
  app::State state = elapse(with_f, 1600);

  // Then: F completed and its 20 points count
  EXPECT_EQ(statusOf(state, f), Status::COMPLETED);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 20);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 0);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 0);
}

/// @brief The defining history: F collects +20 while G is pending, and G activates first
TEST(OrbitalGameDomainApplication, LosesPointsOfOrderInterruptedByReplacement) {
  // Given: F to (10,2) collected food at (5,2); G is submitted at 800 ms (eligible after 1096 ms)
  auto [with_f, f] = submit(started(makeLanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 800), PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 1200 ms, after F collected the second food
  app::State state = elapse(with_g, 400);

  // Then: F was interrupted and its 20 points are lost, while the snake kept its growth
  EXPECT_EQ(statusOf(state, f), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(state, g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 0);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 0);
  EXPECT_EQ(snake_model::length(arena_authority::view(*state.arena).snakes.at(PLAYER_A)), 5U);
}

/// @brief Changing one's mind costs nothing until the replacement actually activates
TEST(OrbitalGameDomainApplication, KeepsUnbankedPointsWhenReplacementIsOnlySubmitted) {
  // Given: F to (10,2) collected food at (5,2) and (7,2)
  auto [with_f, f] = submit(started(makeLanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  app::State collected = elapse(with_f, 1000);

  // When: G is submitted
  auto [with_g, g] = submit(collected, PLAYER_A, Point{10, 6});

  // Then: F still holds its 20 unbanked points and G waits
  EXPECT_EQ(scoreOf(with_g, PLAYER_A).unbanked, 20);
  EXPECT_EQ(statusOf(with_g, f), Status::EXECUTING);
  EXPECT_EQ(statusOf(with_g, g), Status::PENDING);
}

/// @subsection handover chain
///
/// F to (10,2) activates at 200 ms and arrives at 1600 ms. A successor committed at 1400 ms is
/// eligible after 1684 ms and so still pending when F completes.

/// @brief The defining history of the refinement: G is committed before F completes and still pending then
TEST(OrbitalGameDomainApplication, AwardsHandoverBonusWhenSuccessorIsCommittedBeforeCompletion) {
  // Given: F collects food at (5,2) and (7,2); G is committed at 1400 ms
  auto [with_f, f] = submit(started(makeLanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1400), PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 1800 ms
  app::State state = elapse(with_g, 400);

  // Then: F's 20 food points and the base handover bonus count; the chain has one handover
  EXPECT_EQ(statusOf(state, f), Status::COMPLETED);
  EXPECT_EQ(statusOf(state, g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 20 + 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 1);
  EXPECT_EQ(scoreOf(state, PLAYER_A).next_handover, 20);
}

TEST(OrbitalGameDomainApplication, DoublesHandoverBonusForConsecutiveHandover) {
  // Given: F hands over to G, which activates at 1800 ms towards (10,6) and arrives at 2400 ms;
  //        H is committed at 2200 ms (eligible after 2530 ms)
  auto [with_f, f] = submit(started(makeLanes({})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1400), PLAYER_A, Point{10, 6});
  auto [with_h, h] = submit(elapse(with_g, 800), PLAYER_A, Point{3, 6});

  // When: time passes until G has completed
  app::State state = elapse(with_h, 200);

  // Then: both handovers count, the second worth twice the first
  EXPECT_EQ(statusOf(state, g), Status::COMPLETED);
  EXPECT_EQ(statusOf(state, h), Status::PENDING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10 + 20);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 2);
}

/// @brief Committing too late: F completes before its successor exists
TEST(OrbitalGameDomainApplication, BreaksChainWhenSuccessorIsCommittedAfterCompletion) {
  // Given: F hands over to G, which arrives at 2400 ms; H is committed only at 2400 ms, after that step
  auto [with_f, f] = submit(started(makeLanes({})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1400), PLAYER_A, Point{10, 6});
  auto [with_h, h] = submit(elapse(with_g, 1000), PLAYER_A, Point{3, 6});

  // When: time passes until H has activated
  app::State state = elapse(with_h, 600);

  // Then: only F's handover counts and the chain is broken
  EXPECT_EQ(statusOf(state, g), Status::COMPLETED);
  EXPECT_EQ(statusOf(state, h), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 0);
}

/// @brief Committing too early: the successor activates first and interrupts G
TEST(OrbitalGameDomainApplication, BreaksChainAndLosesPointsWhenSuccessorActivatesFirst) {
  // Given: F hands over to G, which collects food at (10,4) on its way to (10,6); H is committed at 1900 ms
  auto [with_f, f] = submit(started(makeLanes({Point{10, 4}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1400), PLAYER_A, Point{10, 6});
  auto [with_h, h] = submit(elapse(with_g, 500), PLAYER_A, Point{3, 6});

  // When: time passes until H has activated at 2400 ms, the step at which G would have arrived
  app::State state = elapse(with_h, 500);

  // Then: G was interrupted, its food points are lost, F's handover still counts and the chain is broken
  EXPECT_EQ(statusOf(state, g), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(state, h), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 0);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 0);
}

/// @brief Revising the prepared successor keeps the handover: the latest committed successor counts
TEST(OrbitalGameDomainApplication, KeepsHandoverWhenPendingSuccessorIsRevised) {
  // Given: G is committed at 1200 ms and superseded by G' at 1400 ms, both before F completes
  auto [with_f, f] = submit(started(makeLanes({})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1200), PLAYER_A, Point{10, 8});
  auto [with_revised, revised] = submit(elapse(with_g, 200), PLAYER_A, Point{10, 6});

  // When: time passes until G' has activated
  app::State state = elapse(with_revised, 400);

  // Then: G never activated, and F's completion is a handover to G'
  EXPECT_EQ(statusOf(state, g), Status::SUPERSEDED);
  EXPECT_EQ(statusOf(state, revised), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 1);
}

/// @brief A prepared successor that the Arena refuses ends the chain it was prepared for
TEST(OrbitalGameDomainApplication, BreaksChainWhenSuccessorIsRejected) {
  // Given: G, with a target outside the board, is committed at 1400 ms
  auto [with_f, f] = submit(started(makeLanes({})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1400), PLAYER_A, Point{25, 2});

  // When: time passes until G was evaluated at 1800 ms
  app::State state = elapse(with_g, 400);

  // Then: F's handover counts, but G was rejected and the chain is broken
  EXPECT_EQ(statusOf(state, g), Status::REJECTED);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).chain, 0);
}

/// @brief Distributed ordering: Game learns the player's earlier commitment only after the Arena's completion
TEST(OrbitalGameDomainApplication, ReachesSameScoreWhenCommitmentReachesGameAfterCompletion) {
  // Given: the canonical history, in which G is committed at 1400 ms and F completes at 1600 ms
  auto [with_f, f_observations] =
      app::apply(started(makeLanes({Point{5, 2}, Point{7, 2}})), app::Submit{PLAYER_A, Point{10, 2}});
  auto [before_g, early] = app::apply(makeCornerRandom(), std::move(with_f), app::TimeElapsed{milliseconds{1400}});
  auto [with_g, g_observations] = app::apply(std::move(before_g), app::Submit{PLAYER_A, Point{10, 6}});
  auto [canonical, late] = app::apply(makeCornerRandom(), std::move(with_g), app::TimeElapsed{milliseconds{400}});
  //        and a Game that learns F's commitment and all steps first
  game_authority::State game = game_authority::observeCommitment(
      game_authority::startRound(ROUND, {PLAYER_A, PLAYER_B}), *f_observations.commitment);
  for (const auto* steps : {&early.steps, &late.steps}) {
    for (const game_boundary::StepReport& report : *steps) {
      game =
          std::get<0>(game_authority::observeStep(std::move(game), report.at, report.activations, report.events, true));
    }
  }

  // When: G's commitment record reaches that Game after F's completion
  game = game_authority::observeCommitment(std::move(game), *g_observations.commitment);

  // Then: its score and chain equal those of the canonical Domain Application
  EXPECT_EQ(game_authority::view(game).players.at(PLAYER_A).credited, scoreOf(canonical, PLAYER_A).credited);
  EXPECT_EQ(game_authority::view(game).players.at(PLAYER_A).chain, scoreOf(canonical, PLAYER_A).chain);
  EXPECT_EQ(scoreOf(canonical, PLAYER_A).credited, 20 + 10);
}

/// @subsection pending orders

/// @brief A revision handled before the activation step prevents the old pending order from activating
TEST(OrbitalGameDomainApplication, SupersedesEligibleOrderWithRevisionBeforeItsStep) {
  // Given: G was submitted at 0 ms (eligible at 200 ms) and time is at 199 ms
  auto [with_g, g] = submit(started(makeLanes({})), PLAYER_A, Point{10, 6});
  app::State before_step = elapse(with_g, 199);

  // When: H is submitted and the step at 200 ms runs
  auto [with_h, h] = submit(before_step, PLAYER_A, Point{12, 2});
  app::State state = elapse(with_h, 1);

  // Then: G never activated and H waits for its own deadline
  EXPECT_EQ(statusOf(state, g), Status::SUPERSEDED);
  EXPECT_EQ(statusOf(state, h), Status::PENDING);
  EXPECT_TRUE(arena_authority::view(*state.arena).active.empty());
}

/// @brief A refused replacement neither stops the executing order nor costs its points
TEST(OrbitalGameDomainApplication, KeepsExecutingOrderAndItsPointsWhenReplacementIsRejected) {
  // Given: F to (10,2) collected food at (5,2), and G with a target outside the board was submitted
  auto [with_f, f] = submit(started(makeLanes({Point{5, 2}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 600), PLAYER_A, Point{25, 2});

  // When: time passes until G was evaluated
  app::State state = elapse(with_g, 600);

  // Then: G is rejected, F still executes with its 10 unbanked points
  EXPECT_EQ(statusOf(state, g), Status::REJECTED);
  EXPECT_EQ(statusOf(state, f), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 10);
}

/// @subsection shared board

/// @brief A food race: the loser's positional order stays valid after the food is gone
TEST(OrbitalGameDomainApplication, CompletesPositionalOrderAfterOpponentTookTheFood) {
  // Given: A orders its snake to the food at (8,2); B heads down column 8 and takes the food at once
  RoundSetup setup = makeLanes({Point{8, 2}});
  setup.snakes.at(PLAYER_B) = SnakeSetup{Point{8, 1}, Direction::DOWN, 2};
  auto [with_order, order] = submit(started(setup), PLAYER_A, Point{8, 2});

  // When: A's snake arrives at (8,2)
  app::State state = elapse(with_order, 1200);

  // Then: B, eating without an order, was credited 5 for the food and A's order completed without points
  EXPECT_EQ(statusOf(state, order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(state, PLAYER_B).credited, 5);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 0);
}

/// @subsection conclusion

/// @brief The step due at the end of the game counts, including the completion it brings
TEST(OrbitalGameDomainApplication, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: at 118400 ms A heads right in row 2; an order to (19,5) collects food at (19,3) and arrives at 120000 ms
  auto [with_order, order] = submit(elapse(started(makeLanes({Point{19, 3}})), 118400), PLAYER_A, Point{19, 5});

  // When: the remaining time of the game passes
  auto [state, observations] = app::apply(makeCornerRandom(), with_order, app::TimeElapsed{milliseconds{1600}});

  // Then: the order completed on the final step, its 10 points count and A wins
  EXPECT_EQ(statusOf(state, order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  ASSERT_TRUE(observations.concluded.has_value());
  EXPECT_EQ(observations.concluded->result, (game_authority::Result{PlayerId{PLAYER_A}}));
  EXPECT_EQ(observations.steps.back().at, std::chrono::minutes{2});
}

TEST(OrbitalGameDomainApplication, ConcludesWithDrawWhenBothSnakesDie) {
  // Given: A and B head towards each other in row 5
  RoundSetup setup = makeLanes({});
  setup.snakes.at(PLAYER_A) = SnakeSetup{Point{5, 5}, Direction::RIGHT, 2};
  setup.snakes.at(PLAYER_B) = SnakeSetup{Point{7, 5}, Direction::LEFT, 2};

  // When: their heads meet
  auto [state, observations] = app::apply(makeCornerRandom(), started(setup), app::TimeElapsed{milliseconds{200}});

  // Then: the game concluded at once as a draw
  ASSERT_TRUE(observations.concluded.has_value());
  EXPECT_EQ(observations.concluded->result, (game_authority::Result{std::nullopt}));
}

/// @brief Once the game is over, executing orders end with it and pending ones are cancelled
TEST(OrbitalGameDomainApplication, EndsExecutingAndCancelsPendingOrdersAtConclusion) {
  // Given: F executes since 118600 ms, and G submitted at 119800 ms (eligible after 120024 ms) is still
  //        pending at the end
  auto [with_f, f] = submit(elapse(started(makeLanes({})), 118000), PLAYER_A, Point{12, 6});
  auto [with_g, g] = submit(elapse(with_f, 1800), PLAYER_A, Point{3, 3});

  // When: the game ends
  app::State state = elapse(with_g, 200);

  // Then: F ended with the round and G was cancelled
  EXPECT_EQ(statusOf(state, f), Status::ENDED_BY_ROUND);
  EXPECT_EQ(statusOf(state, g), Status::CANCELLED);
  EXPECT_TRUE(arena_authority::view(*state.arena).active.empty());
}

TEST(OrbitalGameDomainApplication, RejectsSubmissionAfterConclusion) {
  // Given: the game is over
  app::State over = elapse(started(makeLanes({})), 120000);

  // When: a player submits an order
  auto [state, observations] = app::apply(over, app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is rejected because the round is over
  ASSERT_TRUE(observations.submission.has_value());
  EXPECT_EQ(std::get<SubmissionRejected>(*observations.submission).reason, SubmissionRejectionReason::ROUND_OVER);
}

TEST(OrbitalGameDomainApplication, RunsNoStepsAfterConclusion) {
  // Given: the game is over
  app::State over = elapse(started(makeLanes({})), 120000);

  // When: more time passes
  auto [state, observations] = app::apply(makeCornerRandom(), over, app::TimeElapsed{milliseconds{1000}});

  // Then: no step runs
  EXPECT_TRUE(observations.steps.empty());
}

/// @subsection time

/// @brief One time advance can run several steps; every step's report is kept
TEST(OrbitalGameDomainApplication, ReportsEveryStepOfOneTimeAdvance) {
  // When: one second passes at once
  auto [state, observations] =
      app::apply(makeCornerRandom(), started(makeLanes({})), app::TimeElapsed{milliseconds{1000}});

  // Then: five steps ran, at 200 ms intervals
  ASSERT_EQ(observations.steps.size(), 5U);
  EXPECT_EQ(observations.steps.front().at, milliseconds{200});
  EXPECT_EQ(observations.steps.back().at, milliseconds{1000});
}

namespace {

// Every step report of the given time advances, in order, and the state they lead to
std::tuple<std::vector<game_boundary::StepReport>, app::State> advance(app::State state,
                                                                       const std::vector<int>& advances_ms) {
  std::vector<game_boundary::StepReport> steps;
  for (int ms : advances_ms) {
    auto [next, observations] = app::apply(makeCornerRandom(), std::move(state), app::TimeElapsed{milliseconds{ms}});
    state = std::move(next);
    steps.insert(steps.end(), observations.steps.begin(), observations.steps.end());
  }
  return {std::move(steps), std::move(state)};
}

// The comparable content of the three views: heads, food, executing targets and step; order statuses; points and
// remaining time
auto viewedOutcome(const app::State& state) {
  arena_authority::View arena = arena_authority::view(*state.arena);
  std::map<PlayerId, Point> heads;
  for (const auto& [player, snake] : arena.snakes) {
    heads[player] = snake_model::head(snake);
  }
  std::map<PlayerId, Point> targets;
  for (const auto& [player, active] : arena.active) {
    targets[player] = active.target;
  }
  std::vector<Status> statuses;
  for (const order_authority::OrderRecord& record : order_authority::view(*state.orders).orders) {
    statuses.push_back(record.status);
  }
  game_authority::View game = game_authority::view(*state.game);
  std::map<PlayerId, std::pair<int, int>> points;
  for (const auto& [player, score] : game.players) {
    points[player] = {score.credited, score.unbanked};
  }
  return std::make_tuple(heads, arena.food, targets, arena.step, statuses, points, game.remaining);
}

}  // namespace

/// @brief Cutting the same game time into smaller advances does not change what happens
TEST(OrbitalGameDomainApplication, ReportsSameStepsAndViewsForOneAdvanceAndItsParts) {
  // Given: A's order to (7,2) is pending; it activates at 200 ms and collects the food at (6,2) on its way
  auto [state, order] = submit(started(makeLanes({Point{6, 2}})), PLAYER_A, Point{7, 2});

  // When: one second passes once at once and once in five parts
  auto [at_once, at_once_state] = advance(state, {1000});
  auto [in_parts, in_parts_state] = advance(state, {200, 200, 200, 200, 200});

  // Then: both report the same steps with the same activations and events, and lead to the same views
  ASSERT_EQ(at_once.size(), 5U);
  ASSERT_EQ(at_once[0].activations.size(), 1U);
  ASSERT_FALSE(at_once[3].events.empty());
  ASSERT_EQ(in_parts.size(), at_once.size());
  for (size_t i = 0; i < at_once.size(); ++i) {
    EXPECT_EQ(in_parts[i].step, at_once[i].step);
    EXPECT_EQ(in_parts[i].at, at_once[i].at);
    EXPECT_EQ(in_parts[i].activations, at_once[i].activations);
    EXPECT_EQ(in_parts[i].events, at_once[i].events);
  }
  EXPECT_EQ(viewedOutcome(in_parts_state), viewedOutcome(at_once_state));
}

/// @brief A submission's commitment time is the current time of the game clock
TEST(OrbitalGameDomainApplication, CommitsSubmissionAtCurrentGameTime) {
  // Given: 1300 ms have passed
  app::State state = elapse(started(makeLanes({})), 1300);

  // When: a player submits an order
  auto [next, observations] = app::apply(state, app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is committed at 1300 ms
  EXPECT_EQ(std::get<SubmissionAccepted>(*observations.submission).committed_at, milliseconds{1300});
}

/// @subsection views

/// @brief Players see how long a submission sent now would travel, so they can time it
TEST(OrbitalGameDomainApplication, ViewsDelayOfSubmissionIssuedNow) {
  // Given: 14900 ms have passed
  app::State state = elapse(started(makeLanes({})), 14900);

  // When: 100 ms more pass, reaching a quarter of the orbital cycle
  auto [next, observations] = app::apply(makeCornerRandom(), state, app::TimeElapsed{milliseconds{100}});

  // Then: the views carry the current time and the delay halfway between 200 ms and 2000 ms
  ASSERT_TRUE(observations.views.has_value());
  EXPECT_EQ(observations.views->now, milliseconds{15000});
  EXPECT_EQ(observations.views->submission_delay, milliseconds{1100});
}

/// @brief Game time ends with the game, so the link condition shown at the end stays as it was
TEST(OrbitalGameDomainApplication, StopsGameTimeAndLinkDelayAtConclusion) {
  // Given: the game ended at 120000 ms
  app::State over = elapse(started(makeLanes({})), 120000);

  // When: five more seconds pass
  auto [state, observations] = app::apply(makeCornerRandom(), over, app::TimeElapsed{milliseconds{5000}});

  // Then: the views still show the time and delay of the end of the game
  ASSERT_TRUE(observations.views.has_value());
  EXPECT_EQ(observations.views->now, milliseconds{120000});
  EXPECT_EQ(observations.views->submission_delay, latency_policy::delayAt(milliseconds{120000}));
}

/// @brief The preview of an order in transit follows the snake until the order activates
TEST(OrbitalGameDomainApplication, ViewsPendingRouteFromCurrentSnakePosition) {
  // Given: at 200 ms, with A's snake at (3,2) heading right, A ordered (8,5) (eligible after 424 ms)
  auto [with_order, order] = submit(elapse(started(makeLanes({})), 200), PLAYER_A, Point{8, 5});

  // When: the step at 400 ms moved the snake to (4,2)
  auto [state, observations] = app::apply(makeCornerRandom(), with_order, app::TimeElapsed{milliseconds{200}});

  // Then: the pending route starts from (4,2): four moves right, three down
  ASSERT_TRUE(observations.views.has_value());
  using D = Direction;
  EXPECT_EQ(observations.views->pending_routes.at(PLAYER_A),
            (std::vector<Direction>{D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::DOWN, D::DOWN, D::DOWN}));
}

/// @section orbital_game_domain_application::startWithSetup

/// @brief A concluded game can be followed by a fresh one
TEST(OrbitalGameDomainApplication, StartsFreshGameAfterConclusion) {
  // Given: the first game is over
  app::State over = elapse(started(makeLanes({})), 120000);

  // When: a new game starts
  auto [state, observations] =
      app::startWithSetup(makeCornerRandom(), over, RoundSetup{RoundId{2}, Board{20, 10}, makeLanes({}).snakes, {}});

  // Then: it runs from time zero with no orders and the full duration remaining
  ASSERT_TRUE(observations.views.has_value());
  EXPECT_TRUE(game_authority::isRunning(*state.game));
  EXPECT_EQ(observations.views->now, milliseconds{0});
  EXPECT_TRUE(observations.views->orders.orders.empty());
  EXPECT_EQ(observations.views->game.remaining, game_authority::view(*started(makeLanes({})).game).remaining);
}

}  // namespace orbital
}  // namespace snake
