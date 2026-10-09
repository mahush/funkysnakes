#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "common/game_time.hpp"
#include "common/players.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/identities.hpp"
#include "orbital/scoring_policy.hpp"

namespace snake {
namespace orbital {
namespace game_authority {

/**
 * Orbital Game Authority - owns the evolution of an Orbital Orders game as a whole
 *
 * Owns whether the game runs, its duration, the credited and unbanked points and the final
 * result. A game is one round. It sits beside the Arena and Order Authorities: it reads what the
 * Arena reports, uses the Orbital Scoring Policy for point values and announces the conclusion as
 * an intent, which the realization routes to the Arena (end of execution) and to the Order
 * Authority (cancel pending orders).
 *
 * Within one step, the step's events are scored before conclusion is checked, so a step due at
 * the end of the game still counts. Unbanked points left at conclusion are lost.
 */

constexpr std::chrono::minutes GAME_DURATION{2};

/**
 * @brief Outcome of a concluded game
 */
struct Result {
  std::optional<PlayerId> winner;  // Highest credited score; empty for a draw

  bool operator==(const Result& other) const noexcept { return winner == other.winner; }
};

/**
 * @brief Intent announcing that the game concluded
 */
struct GameConcluded {
  RoundId round;
  Result result;
};

/**
 * @brief Game state owned by the Orbital Game Authority
 *
 * Fields may be read freely. Changes must go through the transitions below only.
 */
struct State {
  RoundId round;
  GameTime duration{GAME_DURATION};
  GameTime now{0};                // Time of the last observed step
  scoring_policy::Scores scores;  // Credited points per player, unbanked points per order
  std::optional<Result> result;   // Set once the game concluded
};

/**
 * @brief Start a game
 *
 * @param round Identity of the round
 * @param players Players taking part, all starting with zero points
 * @return Running game
 */
State startRound(RoundId round, const std::vector<PlayerId>& players);

/**
 * @brief Score one arena step and conclude the game if it is over
 *
 * The game is over when its duration has elapsed or no snake is alive any more.
 * Steps observed after conclusion change nothing.
 *
 * @param state Current game state
 * @param now Time of the step on the round's timeline
 * @param events Arena events of the step, in order
 * @param any_snake_alive Whether a snake is still alive after the step (Arena projection)
 * @return Tuple of (updated state, conclusion intent if the game concluded with this step)
 */
std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                            GameTime now,
                                                            const ArenaEvents& events,
                                                            bool any_snake_alive);

/**
 * @brief Whether the game is still running (not yet concluded)
 *
 * @param state Current game state
 * @return True until the game concluded
 */
bool isRunning(const State& state);

/**
 * @brief A player's points
 */
struct PlayerScore {
  int credited;
  int unbanked;  // Points of the executing order that count only if it completes
};

/**
 * @brief Projection of the game state for presentation
 */
struct View {
  std::map<PlayerId, PlayerScore> players;
  GameTime remaining;
  std::optional<Result> result;
};

/**
 * @brief Project the game state
 *
 * @param state Current game state
 * @return Points per player, remaining time and the result once concluded
 */
View view(const State& state);

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
