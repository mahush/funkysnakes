// Tests for the Orbital Game Authority: scoring of steps, conclusion and result.

#include <gtest/gtest.h>

#include <chrono>

#include "orbital/game_authority.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

using game_authority::Result;
using game_authority::State;

constexpr RoundId ROUND{1};

State runningGame() { return game_authority::startRound(ROUND, {PLAYER_A, PLAYER_B}); }

}  // namespace

/// @section game_authority::startRound

TEST(OrbitalGameAuthority, StartsRunningWithZeroPointsAndFullDuration) {
  // When: a game starts
  game_authority::View view = game_authority::view(runningGame());

  // Then: both players have no points and two minutes remain
  EXPECT_EQ(view.players.at(PLAYER_A).credited, 0);
  EXPECT_EQ(view.players.at(PLAYER_B).credited, 0);
  EXPECT_EQ(view.remaining, std::chrono::minutes{2});
  EXPECT_TRUE(game_authority::isRunning(runningGame()));
}

/// @section game_authority::observeStep

/// @subsection scoring

TEST(OrbitalGameAuthority, ShowsCreditedAndUnbankedPointsSeparately) {
  // When: a step with food during an order and food without an order is observed
  auto [state, concluded] = game_authority::observeStep(
      runningGame(),
      milliseconds{200},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_B, Point{2, 2}, std::nullopt}},
      true);

  // Then: A has 10 unbanked points, B 10 credited points, and the game goes on
  game_authority::View view = game_authority::view(state);
  EXPECT_EQ(view.players.at(PLAYER_A).credited, 0);
  EXPECT_EQ(view.players.at(PLAYER_A).unbanked, 10);
  EXPECT_EQ(view.players.at(PLAYER_B).credited, 10);
  EXPECT_FALSE(concluded.has_value());
}

TEST(OrbitalGameAuthority, CountsDownRemainingTime) {
  // When: a step at 30 s is observed
  auto [state, concluded] = game_authority::observeStep(runningGame(), milliseconds{30000}, {}, true);

  // Then: 90 s remain
  EXPECT_EQ(game_authority::view(state).remaining, milliseconds{90000});
}

/// @subsection conclusion

TEST(OrbitalGameAuthority, ConcludesWhenDurationHasElapsed) {
  // When: the step at two minutes is observed
  auto [state, concluded] = game_authority::observeStep(runningGame(), std::chrono::minutes{2}, {}, true);

  // Then: the game concluded
  EXPECT_TRUE(concluded.has_value());
  EXPECT_FALSE(game_authority::isRunning(state));
}

TEST(OrbitalGameAuthority, ConcludesWhenNoSnakeIsAlive) {
  // When: a step after which no snake is alive is observed
  auto [state, concluded] =
      game_authority::observeStep(runningGame(), milliseconds{5000}, {MutualBite{PLAYER_A, PLAYER_B}}, false);

  // Then: the game concluded early
  EXPECT_TRUE(concluded.has_value());
}

/// @brief The step due at the end of the game still counts, including a completion it brings
TEST(OrbitalGameAuthority, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: order F of A holds 20 unbanked points
  auto [state, first] = game_authority::observeStep(
      runningGame(),
      milliseconds{1000},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}},
      true);

  // When: F completes on the step at two minutes
  auto [final_state, concluded] = game_authority::observeStep(
      state, std::chrono::minutes{2}, {OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}, true);

  // Then: A's 20 points count and A wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).credited, 20);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_A}}));
}

/// @brief Unbanked points do not count for the winner
TEST(OrbitalGameAuthority, LosesUnbankedPointsAtConclusion) {
  // Given: B has 10 credited points and A's executing order holds 20 unbanked points
  auto [state, first] = game_authority::observeStep(runningGame(),
                                                    milliseconds{1000},
                                                    {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt},
                                                     FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}},
                                                     FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}},
                                                    true);

  // When: the game concludes while A's order still executes
  auto [final_state, concluded] = game_authority::observeStep(state, std::chrono::minutes{2}, {}, true);

  // Then: A's unbanked points are lost and B wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).unbanked, 0);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_B}}));
}

TEST(OrbitalGameAuthority, DrawsOnEqualCreditedScores) {
  // When: the game concludes with equal credited scores
  auto [state, concluded] = game_authority::observeStep(runningGame(), std::chrono::minutes{2}, {}, true);

  // Then: the result is a draw
  EXPECT_EQ(concluded->result, (Result{std::nullopt}));
}

/// @brief The Arena's ROUND_ENDED events after conclusion must not change the final scores
TEST(OrbitalGameAuthority, IgnoresStepsAfterConclusion) {
  // Given: a concluded game in which B leads
  auto [state, concluded] = game_authority::observeStep(
      runningGame(), std::chrono::minutes{2}, {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt}}, true);

  // When: further events are observed
  auto [later, again] = game_authority::observeStep(state,
                                                    std::chrono::minutes{2},
                                                    {OrderEnded{OrderId{1}, EndReason::ROUND_ENDED, std::nullopt},
                                                     FoodCollected{PLAYER_A, Point{1, 1}, std::nullopt}},
                                                    true);

  // Then: nothing changes and no second conclusion is announced
  EXPECT_FALSE(again.has_value());
  EXPECT_EQ(game_authority::view(later).players.at(PLAYER_A).credited, 0);
  EXPECT_EQ(game_authority::view(later).result, (Result{PlayerId{PLAYER_B}}));
}

}  // namespace orbital
}  // namespace snake
