// Domain Harness tests for the Classic Game Domain Application.
//
// These tests run complete histories of boundary interactions (Start, Steer, TogglePause,
// TimeElapsed) against the canonical single-process realization of classic snake and check
// what is observable at the boundary.

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "snake/classic_game_domain_application.hpp"
#include "snake/snake_model_evolve.hpp"
#include "test_printers.hpp"

namespace snake {

namespace {

namespace app = classic_game_domain_application;
using std::chrono::milliseconds;
using std::chrono::seconds;

/**
 * @brief Random source that always returns the lowest value of the requested range
 *
 * Deterministic and cheap; food is placed in the board's top-left corner area.
 */
RandomIntGeneratorFn lowestRandom() {
  return [](int min, int /* max */) { return min; };
}

app::State newApp() { return app::initial(lowestRandom()); }

app::State startedApp(int starting_level = 1) { return std::get<0>(app::apply(newApp(), app::Start{starting_level})); }

std::tuple<app::State, app::Observations> wait(app::State state, milliseconds duration) {
  return app::apply(lowestRandom(), std::move(state), app::TimeElapsed{duration});
}

Point headOf(const app::State& state, const PlayerId& player) {
  return snake_model::head(state.arena.snakes.at(player));
}

}  // namespace

/// @section Start

/// @brief Ensures that starting a game reports its starting level and running status
TEST(ClassicGameDomainApplication, ReportsRunningStatusAtStartingLevel) {
  // When: a game is started at level 3
  auto [state, observations] = app::apply(newApp(), app::Start{3});

  // Then: a running status at level 3 is reported
  ASSERT_EQ(observations.statuses.size(), 1u);
  EXPECT_EQ(observations.statuses[0].level, 3);
  EXPECT_FALSE(observations.statuses[0].paused);
}

/// @brief Ensures that time does not move the arena before a game is started
TEST(ClassicGameDomainApplication, DoesNotStepArenaBeforeStart) {
  // When: time passes before a game is started
  auto [state, observations] = wait(newApp(), seconds{1});

  // Then: the arena does not step
  EXPECT_TRUE(observations.arena_views.empty());
  EXPECT_EQ(headOf(state, PLAYER_A), (Point{5, 10}));
}

/// @section TimeElapsed
///
/// Game time drives arena steps and cadences.

/// @brief Ensures that the arena steps once per step interval of level 1 (200 ms)
TEST(ClassicGameDomainApplication, StepsArenaAtLevelOneInterval) {
  // When: one second passes
  auto [state, observations] = wait(startedApp(), seconds{1});

  // Then: the arena steps five times (200 ms each)
  EXPECT_EQ(observations.arena_views.size(), 5u);
  EXPECT_EQ(headOf(state, PLAYER_A), (Point{10, 10}));
}

/// @brief Ensures that time is accumulated across interactions, not lost between them
TEST(ClassicGameDomainApplication, AccumulatesTimeAcrossInteractions) {
  // Given: a game started and 150 ms passed
  app::State state = std::get<0>(wait(startedApp(), milliseconds{150}));

  // When: another 50 ms pass
  auto [new_state, observations] = wait(std::move(state), milliseconds{50});

  // Then: the arena steps once
  EXPECT_EQ(observations.arena_views.size(), 1u);
}

/// @brief Ensures that a game started at a higher level steps at that level's interval
TEST(ClassicGameDomainApplication, StepsAtIntervalOfStartingLevel) {
  // When: 510 ms pass
  auto [state, observations] = wait(startedApp(3), milliseconds{510});

  // Then: the arena steps three times (170 ms each)
  EXPECT_EQ(observations.arena_views.size(), 3u);
}

/// @brief Ensures that the level rises after one level period of game time
TEST(ClassicGameDomainApplication, RaisesLevelAfterOneLevelPeriod) {
  // When: one level period passes
  auto [state, observations] = wait(startedApp(), seconds{60});

  // Then: level 2 is reported and stepping speeds up
  ASSERT_EQ(observations.statuses.size(), 1u);
  EXPECT_EQ(observations.statuses[0].level, 2);
  EXPECT_EQ(state.clock.step_interval, milliseconds{185});
}

/// @brief Ensures that the arena steps faster after a level-up
TEST(ClassicGameDomainApplication, StepsFasterAfterLevelUp) {
  // Given: the level rose to 2
  app::State state = std::get<0>(wait(startedApp(), seconds{60}));

  // When: 370 ms pass
  auto [new_state, observations] = wait(std::move(state), milliseconds{370});

  // Then: the arena steps twice (185 ms each)
  EXPECT_EQ(observations.arena_views.size(), 2u);
}

/// @brief Ensures that food is repositioned once per reposition period
TEST(ClassicGameDomainApplication, RequestsRepositionAfterOneRepositionPeriod) {
  // When: one reposition period passes
  // 5 s is a multiple of 200 ms, so the 25th step falls on the same instant: the step happens first
  auto [state, observations] = wait(startedApp(), seconds{5});

  // Then: a food reposition is requested
  EXPECT_TRUE(state.arena.should_reposition_food);
}

/// @section TogglePause

/// @brief Ensures that pausing reports the paused status
TEST(ClassicGameDomainApplication, ReportsPausedStatus) {
  // When: pause is toggled
  auto [state, observations] = app::apply(startedApp(), app::TogglePause{});

  // Then: a paused status is reported
  ASSERT_EQ(observations.statuses.size(), 1u);
  EXPECT_TRUE(observations.statuses[0].paused);
}

/// @brief Ensures that the arena does not move while the game is paused
TEST(ClassicGameDomainApplication, DoesNotStepArenaWhilePaused) {
  // Given: the game is paused
  app::State state = std::get<0>(app::apply(startedApp(), app::TogglePause{}));

  // When: time passes
  auto [new_state, observations] = wait(std::move(state), seconds{10});

  // Then: the arena does not step
  EXPECT_TRUE(observations.arena_views.empty());
}

/// @brief Ensures that game time stops while paused, so a pause does not consume the level period
TEST(ClassicGameDomainApplication, StopsGameTimeWhilePaused) {
  // Given: the game was paused for two minutes after 55 s
  app::State state = std::get<0>(wait(startedApp(), seconds{55}));
  state = std::get<0>(app::apply(std::move(state), app::TogglePause{}));
  state = std::get<0>(wait(std::move(state), seconds{120}));
  state = std::get<0>(app::apply(std::move(state), app::TogglePause{}));

  // When: 5 s pass after resuming
  auto [new_state, observations] = wait(std::move(state), seconds{5});

  // Then: level 2 is reported
  ASSERT_EQ(observations.statuses.size(), 1u);
  EXPECT_EQ(observations.statuses[0].level, 2);
}

/// @section Steer

/// @brief Ensures that a steered turn is applied on the next arena step
TEST(ClassicGameDomainApplication, AppliesSteeredTurnOnNextStep) {
  // Given: a game started and player A steered up
  app::State state = std::get<0>(app::apply(startedApp(), app::Steer{PLAYER_A, Direction::UP}));

  // When: one step of time passes
  auto [new_state, observations] = wait(std::move(state), milliseconds{200});

  // Then: player A moves up
  EXPECT_EQ(headOf(new_state, PLAYER_A), (Point{5, 9}));
}

/// @section Game over

namespace {

// Steers both snakes DOWN, LEFT, UP, one turn per step: each head then lands on its own tail
// (A on (4,10), B on (4,15)), so both snakes die in the third step
app::State steerBothIntoOwnTails(app::State state) {
  for (Direction dir : {Direction::DOWN, Direction::LEFT, Direction::UP}) {
    state = std::get<0>(app::apply(std::move(state), app::Steer{PLAYER_A, dir}));
    state = std::get<0>(app::apply(std::move(state), app::Steer{PLAYER_B, dir}));
    if (dir != Direction::UP) {
      state = std::get<0>(wait(std::move(state), milliseconds{200}));
    }
  }
  return state;
}

}  // namespace

/// @brief Ensures that the game ends with a summary once no snake is alive
TEST(ClassicGameDomainApplication, ReportsGameOverOnceNoSnakeIsAlive) {
  // Given: both snakes are about to bite themselves
  app::State state = steerBothIntoOwnTails(startedApp());

  // When: one step of time passes
  auto [new_state, observations] = wait(std::move(state), milliseconds{200});

  // Then: the game over is reported with final scores and level
  ASSERT_TRUE(observations.game_over.has_value());
  EXPECT_EQ(observations.game_over->final_scores, (PerPlayerScores{{PLAYER_A, -10}, {PLAYER_B, -10}}));
  EXPECT_EQ(observations.game_over->final_level, 1);
}

/// @brief Ensures that the arena stops and the level stops rising once the game is over
TEST(ClassicGameDomainApplication, StopsSteppingAndLevelingUpAfterGameOver) {
  // Given: the game is over
  app::State state = std::get<0>(wait(steerBothIntoOwnTails(startedApp()), milliseconds{200}));

  // When: two minutes of time pass
  auto [new_state, observations] = wait(std::move(state), seconds{120});

  // Then: the arena does not step and the level does not rise
  EXPECT_TRUE(observations.arena_views.empty());
  EXPECT_TRUE(observations.statuses.empty());
  EXPECT_FALSE(observations.game_over.has_value());
}

}  // namespace snake
