#pragma once

#include <chrono>
#include <optional>
#include <tuple>
#include <vector>

#include "snake/classic_arena_authority.hpp"
#include "snake/classic_game_lifecycle_authority.hpp"
#include "snake/game_boundary.hpp"
#include "snake/utility.hpp"

namespace snake {
namespace classic_game_domain_application {

// The Domain System Boundary is this application's interface
using game_boundary::ArenaView;
using game_boundary::GameOver;
using game_boundary::Observations;
using game_boundary::Start;
using game_boundary::Status;
using game_boundary::Steer;
using game_boundary::TimeElapsed;
using game_boundary::TogglePause;

/**
 * Classic Game Domain Application - canonical single-process realization of classic snake
 *
 * Composes the Classic Arena Authority, the Classic Game Lifecycle Authority and their Policies
 * without actors, timers or rendering. It routes boundary interactions to the Authorities and
 * carries out their intents mechanically. It makes no gameplay decisions of its own.
 *
 * Game time is supplied from outside as elapsed durations and kept by a virtual clock. The clock
 * is mechanics: the periods and the order of work due at the same instant come from the lifecycle
 * Authority, the step interval from the Difficulty Policy, and it freezes and resumes the cadences
 * when the lifecycle Authority says so.
 */

// ============================================================================
// Composition
// ============================================================================

/**
 * @brief Virtual clock carrying out the lifecycle Authority's clock and cadence intents
 *
 * Mechanics only: it accumulates elapsed game time and reports what falls due.
 */
struct VirtualClock {
  bool stepping{false};  // Arena is stepped (between clock START and STOP, unless paused)
  bool cadences{false};  // Cadences run (after cadence START or RESUME, until STOP or FREEZE)
  std::chrono::milliseconds step_interval{0};
  std::chrono::milliseconds until_step{0};
  std::chrono::milliseconds until_level_period{0};
  std::chrono::milliseconds until_reposition_period{0};
};

/**
 * @brief State of the Domain Application
 */
struct State {
  classic_game_lifecycle_authority::State lifecycle;
  classic_arena_authority::State arena;
  VirtualClock clock;
};

/**
 * @brief Create a Domain Application with an initial arena, before any game has started
 *
 * @param random_int Random number source for initial food placement
 * @return Initial state
 */
State initial(const RandomIntGeneratorFn& random_int);

/**
 * @brief Begin a new game
 */
std::tuple<State, Observations> apply(State state, const Start& start);

/**
 * @brief Register a player's steering intention
 */
std::tuple<State, Observations> apply(State state, const Steer& steer);

/**
 * @brief Pause or resume the game
 */
std::tuple<State, Observations> apply(State state, const TogglePause& toggle);

/**
 * @brief Let game time pass: run every arena step and cadence period that falls due
 *
 * Parameter order: bound parameters first (for bindFront), then state and interaction.
 *
 * @param random_int Random number source for food placement during steps
 * @param state Current state
 * @param elapsed Elapsed game time
 * @return Tuple of (state, observations)
 */
std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int, State state, const TimeElapsed& elapsed);

}  // namespace classic_game_domain_application
}  // namespace snake
