// Tests for the Orbital Game Domain Application: whole-game histories across Order, Arena and Game.

#include <gtest/gtest.h>

#include <chrono>
#include <tuple>

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
RandomIntGeneratorFn cornerRandom() {
  return [](int /* min */, int max) { return max; };
}

/**
 * @brief Board 20x10: A in row 2 heading right, B in row 7 heading right, the given food
 *
 * Filler food in row 9 brings the food to the minimum without random draws.
 */
RoundSetup lanes(FoodItems food) {
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
  return std::get<0>(app::startWithSetup(cornerRandom(), app::initial(), setup));
}

app::State elapse(app::State state, int ms) {
  return std::get<0>(app::apply(cornerRandom(), std::move(state), app::TimeElapsed{milliseconds{ms}}));
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
/// Timing: an order issued at 0 ms is eligible after 400 ms; steps run every 200 ms.

/// @subsection unbanked points

/// @brief The defining history: F collects +20 while G is pending, and F completes first
TEST(OrbitalGameDomainApplication, BanksPointsOfOrderThatCompletesBeforeReplacementActivates) {
  // Given: F to (10,2) activates at 400 ms, collects food at (5,2) and (7,2) and arrives at 1600 ms;
  //        G is submitted at 1200 ms (eligible at 1808 ms)
  auto [with_f, f] = submit(started(lanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 1200), PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 2000 ms
  app::State state = elapse(with_g, 800);

  // Then: F completed and its 20 points count; G executes
  EXPECT_EQ(statusOf(state, f), Status::COMPLETED);
  EXPECT_EQ(statusOf(state, g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 20);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 0);
}

/// @brief The defining history: F collects +20 while G is pending, and G activates first
TEST(OrbitalGameDomainApplication, LosesPointsOfOrderInterruptedByReplacement) {
  // Given: F to (10,2) collected food at (5,2); G is submitted at 600 ms (eligible after 1104 ms)
  auto [with_f, f] = submit(started(lanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  auto [with_g, g] = submit(elapse(with_f, 600), PLAYER_A, Point{10, 6});

  // When: time passes until G has activated, after F collected the second food
  app::State state = elapse(with_g, 600);

  // Then: F was interrupted and its 20 points are lost, while the snake kept its growth
  EXPECT_EQ(statusOf(state, f), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(state, g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 0);
  EXPECT_EQ(scoreOf(state, PLAYER_A).unbanked, 0);
  EXPECT_EQ(snake_model::length(state.arena->snakes.at(PLAYER_A)), 5U);
}

/// @brief Changing one's mind costs nothing until the replacement actually activates
TEST(OrbitalGameDomainApplication, KeepsUnbankedPointsWhenReplacementIsOnlySubmitted) {
  // Given: F to (10,2) collected food at (5,2) and (7,2)
  auto [with_f, f] = submit(started(lanes({Point{5, 2}, Point{7, 2}})), PLAYER_A, Point{10, 2});
  app::State collected = elapse(with_f, 1000);

  // When: G is submitted
  auto [with_g, g] = submit(collected, PLAYER_A, Point{10, 6});

  // Then: F still holds its 20 unbanked points and G waits
  EXPECT_EQ(scoreOf(with_g, PLAYER_A).unbanked, 20);
  EXPECT_EQ(statusOf(with_g, f), Status::EXECUTING);
  EXPECT_EQ(statusOf(with_g, g), Status::PENDING);
}

/// @subsection pending orders

/// @brief A revision handled before the activation step prevents the old pending order from activating
TEST(OrbitalGameDomainApplication, SupersedesEligibleOrderWithRevisionBeforeItsStep) {
  // Given: G was submitted at 0 ms (eligible at 400 ms) and time is at 399 ms
  auto [with_g, g] = submit(started(lanes({})), PLAYER_A, Point{10, 6});
  app::State before_step = elapse(with_g, 399);

  // When: H is submitted and the step at 400 ms runs
  auto [with_h, h] = submit(before_step, PLAYER_A, Point{12, 2});
  app::State state = elapse(with_h, 1);

  // Then: G never activated and H waits for its own deadline
  EXPECT_EQ(statusOf(state, g), Status::SUPERSEDED);
  EXPECT_EQ(statusOf(state, h), Status::PENDING);
  EXPECT_TRUE(state.arena->active.empty());
}

/// @brief A refused replacement neither stops the executing order nor costs its points
TEST(OrbitalGameDomainApplication, KeepsExecutingOrderAndItsPointsWhenReplacementIsRejected) {
  // Given: F to (10,2) collected food at (5,2), and G with a target outside the board was submitted
  auto [with_f, f] = submit(started(lanes({Point{5, 2}})), PLAYER_A, Point{10, 2});
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
  RoundSetup setup = lanes({Point{8, 2}});
  setup.snakes.at(PLAYER_B) = SnakeSetup{Point{8, 1}, Direction::DOWN, 2};
  auto [with_order, order] = submit(started(setup), PLAYER_A, Point{8, 2});

  // When: A's snake arrives at (8,2)
  app::State state = elapse(with_order, 1200);

  // Then: B was credited for the food and A's order completed without points
  EXPECT_EQ(statusOf(state, order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(state, PLAYER_B).credited, 10);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 0);
}

/// @subsection conclusion

/// @brief The step due at the end of the game counts, including the completion it brings
TEST(OrbitalGameDomainApplication, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: at 118400 ms A heads right in row 2; an order to (19,5) collects food at (19,3) and arrives at 120000 ms
  auto [with_order, order] = submit(elapse(started(lanes({Point{19, 3}})), 118400), PLAYER_A, Point{19, 5});

  // When: the remaining time of the game passes
  auto [state, observations] = app::apply(cornerRandom(), with_order, app::TimeElapsed{milliseconds{1600}});

  // Then: the order completed on the final step, its 10 points count and A wins
  EXPECT_EQ(statusOf(state, order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(state, PLAYER_A).credited, 10);
  ASSERT_TRUE(observations.concluded.has_value());
  EXPECT_EQ(observations.concluded->result, (game_authority::Result{PlayerId{PLAYER_A}}));
  EXPECT_EQ(observations.steps.back().at, std::chrono::minutes{2});
}

TEST(OrbitalGameDomainApplication, ConcludesWithDrawWhenBothSnakesDie) {
  // Given: A and B head towards each other in row 5
  RoundSetup setup = lanes({});
  setup.snakes.at(PLAYER_A) = SnakeSetup{Point{5, 5}, Direction::RIGHT, 2};
  setup.snakes.at(PLAYER_B) = SnakeSetup{Point{7, 5}, Direction::LEFT, 2};

  // When: their heads meet
  auto [state, observations] = app::apply(cornerRandom(), started(setup), app::TimeElapsed{milliseconds{200}});

  // Then: the game concluded at once as a draw
  ASSERT_TRUE(observations.concluded.has_value());
  EXPECT_EQ(observations.concluded->result, (game_authority::Result{std::nullopt}));
}

/// @brief Once the game is over, executing orders end with it and pending ones are cancelled
TEST(OrbitalGameDomainApplication, EndsExecutingAndCancelsPendingOrdersAtConclusion) {
  // Given: F executes since 118800 ms, and G submitted at 119600 ms is still pending at the end
  auto [with_f, f] = submit(elapse(started(lanes({})), 118000), PLAYER_A, Point{12, 6});
  auto [with_g, g] = submit(elapse(with_f, 1600), PLAYER_A, Point{3, 3});

  // When: the game ends
  app::State state = elapse(with_g, 400);

  // Then: F ended with the round and G was cancelled
  EXPECT_EQ(statusOf(state, f), Status::ENDED_BY_ROUND);
  EXPECT_EQ(statusOf(state, g), Status::CANCELLED);
  EXPECT_TRUE(state.arena->active.empty());
}

TEST(OrbitalGameDomainApplication, RejectsSubmissionAfterConclusion) {
  // Given: the game is over
  app::State over = elapse(started(lanes({})), 120000);

  // When: a player submits an order
  auto [state, observations] = app::apply(over, app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is rejected because the round is over
  ASSERT_TRUE(observations.submission.has_value());
  EXPECT_EQ(std::get<SubmissionRejected>(*observations.submission).reason, SubmissionRejectionReason::ROUND_OVER);
}

TEST(OrbitalGameDomainApplication, RunsNoStepsAfterConclusion) {
  // Given: the game is over
  app::State over = elapse(started(lanes({})), 120000);

  // When: more time passes
  auto [state, observations] = app::apply(cornerRandom(), over, app::TimeElapsed{milliseconds{1000}});

  // Then: no step runs
  EXPECT_TRUE(observations.steps.empty());
}

/// @subsection time

/// @brief One time advance can run several steps; every step's report is kept
TEST(OrbitalGameDomainApplication, ReportsEveryStepOfOneTimeAdvance) {
  // When: one second passes at once
  auto [state, observations] = app::apply(cornerRandom(), started(lanes({})), app::TimeElapsed{milliseconds{1000}});

  // Then: five steps ran, at 200 ms intervals
  ASSERT_EQ(observations.steps.size(), 5U);
  EXPECT_EQ(observations.steps.front().at, milliseconds{200});
  EXPECT_EQ(observations.steps.back().at, milliseconds{1000});
}

/// @brief A submission's issue time is the current time of the game clock
TEST(OrbitalGameDomainApplication, IssuesSubmissionAtCurrentGameTime) {
  // Given: 1300 ms have passed
  app::State state = elapse(started(lanes({})), 1300);

  // When: a player submits an order
  auto [next, observations] = app::apply(state, app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is issued at 1300 ms
  EXPECT_EQ(std::get<SubmissionAccepted>(*observations.submission).issued_at, milliseconds{1300});
}

}  // namespace orbital
}  // namespace snake
