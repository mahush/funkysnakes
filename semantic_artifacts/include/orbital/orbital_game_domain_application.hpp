#pragma once

#include <chrono>
#include <optional>
#include <tuple>

#include "common/game_time.hpp"
#include "common/random_source.hpp"
#include "orbital/arena_authority.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {

// The Domain System Boundary is this application's interface
using game_boundary::Observations;
using game_boundary::Start;
using game_boundary::Submit;
using game_boundary::TimeElapsed;

/**
 * Orbital Game Domain Application - canonical single-process realization of Orbital Orders
 *
 * Composes the Arena, Order and Game Authorities and their Policies without actors, timers,
 * networking or rendering. It routes boundary interactions and Authority outputs mechanically and
 * makes no gameplay decision of its own.
 *
 * One logical clock drives the round. Each step that falls due runs:
 *   eligible orders (Order) → step (Arena) → interpret outcomes (Order) → score and check end (Game)
 *   → if concluded: end execution (Arena) → interpret endings (Order) → cancel pending (Order)
 * Submissions are handled when they arrive, so a submission at the time of a step is handled
 * before that step if it arrives before the time advance that reaches it.
 */

// Interval between arena steps on the game timeline
constexpr std::chrono::milliseconds STEP_INTERVAL{200};

/**
 * @brief Virtual clock of the round
 *
 * Mechanics only: it accumulates elapsed game time and reports when steps fall due.
 */
struct VirtualClock {
  GameTime now{0};
  GameTime next_step{STEP_INTERVAL};
};

/**
 * @brief State of the Domain Application
 */
struct State {
  std::optional<arena_authority::State> arena;  // Empty until a game started
  std::optional<order_authority::State> orders;
  std::optional<game_authority::State> game;
  VirtualClock clock;
};

/**
 * @brief Create a Domain Application before any game has started
 *
 * @return Initial state
 */
State initial();

/**
 * @brief Begin a new game with the classic setup
 *
 * Parameter order: bound parameters first (for bindFront), then state and interaction.
 *
 * @param random_int Random number source for initial food placement
 * @param state Current state
 * @param start The start interaction
 * @return Tuple of (state, observations)
 */
std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int, State state, const Start& start);

/**
 * @brief Begin a new game with a given world setup
 *
 * @param random_int Random number source for initial food placement
 * @param state Current state
 * @param setup World at round start; its round identity names the game
 * @return Tuple of (state, observations)
 */
std::tuple<State, Observations> startWithSetup(const RandomIntGeneratorFn& random_int,
                                               State state,
                                               const arena_authority::RoundSetup& setup);

/**
 * @brief Submit an order
 *
 * @param state Current state
 * @param submit The submission
 * @return Tuple of (state, observations with the submission outcome)
 */
std::tuple<State, Observations> apply(State state, const Submit& submit);

/**
 * @brief Let game time pass: run every arena step that falls due
 *
 * @param random_int Random number source for food placement during steps
 * @param state Current state
 * @param elapsed Elapsed game time
 * @return Tuple of (state, observations with a report for every step that ran)
 */
std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int, State state, const TimeElapsed& elapsed);

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
