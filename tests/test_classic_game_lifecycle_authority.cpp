// Characterization tests for the Classic Game Lifecycle Authority.
//
// These tests pin the CURRENT lifecycle behavior. A failing test here means gameplay changed,
// not that a bug was fixed. Intentional rule changes require a separate, explicit decision.

#include <gtest/gtest.h>

#include "snake/classic_game_lifecycle_authority.hpp"

namespace snake {

namespace {

namespace lifecycle = classic_game_lifecycle_authority;
using lifecycle::State;

State runningGame() { return std::get<0>(lifecycle::start("game_001", 1, {})); }

State pausedGame() { return std::get<0>(lifecycle::togglePause(runningGame())); }

State concludedGame() {
  return std::get<0>(lifecycle::observeAliveStates(runningGame(), {{PLAYER_A, false}, {PLAYER_B, false}}));
}

}  // namespace

/// @section lifecycle::start

/// @brief Ensures that starting a game makes it run at the requested level
TEST(ClassicGameLifecycleAuthority, RunsGameAtStartingLevel) {
  // When: a game is started at level 3
  auto [state, clock, interval, cadence] = lifecycle::start("game_001", 3, {});

  // Then: it runs at level 3 with clock and cadences started
  EXPECT_EQ(state.game_id, "game_001");
  EXPECT_EQ(state.level, 3);
  EXPECT_FALSE(state.paused);
  EXPECT_FALSE(state.over);
  EXPECT_EQ(clock, lifecycle::ClockIntent::START);
  EXPECT_EQ(cadence, lifecycle::CadenceIntent::START);
}

/// @brief Ensures that a game starts at the speed of its starting level
TEST(ClassicGameLifecycleAuthority, StartsAtSpeedOfStartingLevel) {
  // When: a game is started at level 3
  auto [state, clock, interval, cadence] = lifecycle::start("game_001", 3, {});

  // Then: the step interval is that of level 3
  EXPECT_EQ(interval.interval_ms, 170);
}

/// @brief Ensures that a new game resets a previous game's pause and conclusion
TEST(ClassicGameLifecycleAuthority, ResetsPauseAndConclusionOnNewGame) {
  // Given: a game is concluded and paused
  State state = std::get<0>(lifecycle::togglePause(concludedGame()));

  // When: a new game is started
  state = std::get<0>(lifecycle::start("game_001", 1, state));

  // Then: it is neither paused nor over
  EXPECT_FALSE(state.paused);
  EXPECT_FALSE(state.over);
}

/// @section lifecycle::togglePause

/// @brief Ensures that toggling a running game pauses stepping and freezes game time
TEST(ClassicGameLifecycleAuthority, PausesRunningGameAndFreezesCadences) {
  // When: pause is toggled
  auto [state, clock, cadence] = lifecycle::togglePause(runningGame());

  // Then: the game is paused, stepping pauses and the cadences freeze
  EXPECT_TRUE(state.paused);
  EXPECT_EQ(clock, lifecycle::ClockIntent::PAUSE);
  EXPECT_EQ(cadence, lifecycle::CadenceIntent::FREEZE);
}

/// @brief Ensures that toggling a paused game resumes stepping and continues game time
TEST(ClassicGameLifecycleAuthority, ResumesPausedGameAndItsCadences) {
  // When: pause is toggled
  auto [state, clock, cadence] = lifecycle::togglePause(pausedGame());

  // Then: the game runs, stepping and cadences resume
  EXPECT_FALSE(state.paused);
  EXPECT_EQ(clock, lifecycle::ClockIntent::RESUME);
  EXPECT_EQ(cadence, lifecycle::CadenceIntent::RESUME);
}

/// @brief Ensures that the stopped cadences of a finished game are not resumed by pausing
TEST(ClassicGameLifecycleAuthority, LeavesCadencesStoppedWhenPausedAfterGameOver) {
  // When: pause is toggled
  auto [state, clock, cadence] = lifecycle::togglePause(concludedGame());

  // Then: the cadences are neither frozen nor resumed
  EXPECT_FALSE(cadence.has_value());
}

/// @brief Pins that pausing is not blocked after the game is over, so toggling twice resumes stepping
TEST(ClassicGameLifecycleAuthority, ResumesSteppingWhenUnpausedAfterGameOver) {
  // Given: a game is concluded and paused
  State state = std::get<0>(lifecycle::togglePause(concludedGame()));

  // When: pause is toggled again
  auto [new_state, clock, cadence] = lifecycle::togglePause(state);

  // Then: stepping resumes although the game is over
  EXPECT_TRUE(new_state.over);
  EXPECT_EQ(clock, lifecycle::ClockIntent::RESUME);
}

/// @section lifecycle::levelPeriodElapsed

/// @brief Ensures that an elapsed level period raises the level and speeds up stepping
TEST(ClassicGameLifecycleAuthority, RaisesLevelAndSpeedPerLevelPeriod) {
  // When: a level period elapses
  auto [state, interval] = lifecycle::levelPeriodElapsed(runningGame());

  // Then: the level rises and stepping speeds up
  EXPECT_EQ(state.level, 2);
  ASSERT_TRUE(interval.has_value());
  EXPECT_EQ(interval->interval_ms, 185);
}

/// @brief Ensures that no level is gained while paused
TEST(ClassicGameLifecycleAuthority, IgnoresLevelPeriodsWhilePaused) {
  // When: a level period elapses
  auto [state, interval] = lifecycle::levelPeriodElapsed(pausedGame());

  // Then: the level stays and no interval change is requested
  EXPECT_EQ(state.level, 1);
  EXPECT_FALSE(interval.has_value());
}

/// @brief Ensures that the final level of a finished game does not change
TEST(ClassicGameLifecycleAuthority, IgnoresLevelPeriodsAfterGameOver) {
  // When: a level period elapses
  auto [state, interval] = lifecycle::levelPeriodElapsed(concludedGame());

  // Then: the level stays and no interval change is requested
  EXPECT_EQ(state.level, 1);
  EXPECT_FALSE(interval.has_value());
}

/// @section lifecycle::repositionPeriodElapsed

/// @brief Ensures that an elapsed reposition period requests a food reposition
TEST(ClassicGameLifecycleAuthority, RequestsRepositionPerRepositionPeriod) {
  // When: a reposition period elapses
  std::optional<lifecycle::RepositionIntent> reposition = lifecycle::repositionPeriodElapsed(runningGame());

  // Then: a food reposition is requested
  EXPECT_TRUE(reposition.has_value());
}

/// @brief Ensures that food is not repositioned while paused
TEST(ClassicGameLifecycleAuthority, IgnoresRepositionPeriodsWhilePaused) {
  // When: a reposition period elapses
  std::optional<lifecycle::RepositionIntent> reposition = lifecycle::repositionPeriodElapsed(pausedGame());

  // Then: no reposition is requested
  EXPECT_FALSE(reposition.has_value());
}

/// @brief Ensures that food of a finished game is not repositioned
TEST(ClassicGameLifecycleAuthority, IgnoresRepositionPeriodsAfterGameOver) {
  // When: a reposition period elapses
  std::optional<lifecycle::RepositionIntent> reposition = lifecycle::repositionPeriodElapsed(concludedGame());

  // Then: no reposition is requested
  EXPECT_FALSE(reposition.has_value());
}

/// @section SAME_INSTANT_ORDER

/// @brief Ensures that a step ending the game counts before cadence periods ending at the same instant
TEST(ClassicGameLifecycleAuthority, CountsStepBeforeCadencesAtSameInstant) {
  // When: the same-instant order is read
  lifecycle::DueWork first = lifecycle::SAME_INSTANT_ORDER.front();

  // Then: the step comes first
  EXPECT_EQ(first, lifecycle::DueWork::STEP);
}

/// @section lifecycle::observeAliveStates

/// @brief Pins that the game goes on while any snake is alive, even if only one is left
TEST(ClassicGameLifecycleAuthority, KeepsGameRunningWhileOneSnakeIsAlive) {
  // When: only one alive snake is observed
  auto [state, conclude] = lifecycle::observeAliveStates(runningGame(), {{PLAYER_A, false}, {PLAYER_B, true}});

  // Then: the game goes on
  EXPECT_FALSE(state.over);
  EXPECT_FALSE(conclude.has_value());
}

/// @brief Ensures that the game is over once no snake is alive
TEST(ClassicGameLifecycleAuthority, ConcludesGameWhenNoSnakeIsAlive) {
  // When: no alive snake is observed
  auto [state, conclude] = lifecycle::observeAliveStates(runningGame(), {{PLAYER_A, false}, {PLAYER_B, false}});

  // Then: the game is over and concludes
  EXPECT_TRUE(state.over);
  EXPECT_TRUE(conclude.has_value());
}

/// @brief Ensures that a game is concluded only once
TEST(ClassicGameLifecycleAuthority, ConcludesGameOnlyOnce) {
  // When: no alive snake is observed again
  auto [state, conclude] = lifecycle::observeAliveStates(concludedGame(), {{PLAYER_A, false}, {PLAYER_B, false}});

  // Then: the game is not concluded again
  EXPECT_FALSE(conclude.has_value());
}

/// @section lifecycle::concluded

/// @brief Ensures that finishing the conclusion stops stepping and the cadences
TEST(ClassicGameLifecycleAuthority, StopsClockAndCadencesOnConclusion) {
  // When: the conclusion is finished
  auto [clock, cadence] = lifecycle::concluded(concludedGame());

  // Then: the clock and the cadences stop
  EXPECT_EQ(clock, lifecycle::ClockIntent::STOP);
  EXPECT_EQ(cadence, lifecycle::CadenceIntent::STOP);
}

}  // namespace snake
