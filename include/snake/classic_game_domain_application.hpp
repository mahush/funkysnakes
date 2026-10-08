#pragma once

#include <chrono>
#include <optional>
#include <tuple>
#include <vector>

#include "snake/classic_arena_authority.hpp"
#include "snake/classic_game_lifecycle_authority.hpp"
#include "snake/utility.hpp"

namespace snake {
namespace classic_game_domain_application {

/**
 * Classic Game Domain Application - canonical single-process realization of classic snake
 *
 * Composes the Classic Arena Authority, the Classic Game Lifecycle Authority and their Policies
 * without actors, timers or rendering. It routes boundary interactions to the Authorities and
 * carries out their intents mechanically. It makes no gameplay decisions of its own, with one
 * documented exception: when an arena step and a cadence period fall due at the same instant,
 * the step happens first.
 *
 * Game time is supplied from outside as elapsed durations and kept by a virtual clock. The clock
 * is mechanics: the periods come from the lifecycle Authority, the step interval from the
 * Difficulty Policy, and pausing freezes it because the lifecycle Authority pauses the game.
 */

// ============================================================================
// Domain System Boundary - players end, inbound
// ============================================================================

/**
 * @brief Begin a new game
 */
struct Start {
  int starting_level{1};
};

/**
 * @brief A player's intended turn
 */
struct Steer {
  PlayerId player;
  Direction direction;
};

/**
 * @brief Pause or resume the game
 */
struct TogglePause {};

// ============================================================================
// Domain System Boundary - external facts, inbound
// ============================================================================

/**
 * @brief Game time has passed
 */
struct TimeElapsed {
  std::chrono::milliseconds duration;
};

// ============================================================================
// Domain System Boundary - players end, outbound
// ============================================================================

/**
 * @brief Board, snakes, food and scores after an arena step
 */
struct ArenaView {
  Board board;
  PerPlayerSnakes snakes;
  FoodItems food_items;
  PerPlayerScores scores;
};

/**
 * @brief Current level and whether the game is paused
 */
struct Status {
  int level;
  bool paused;
};

/**
 * @brief Final result of a concluded game
 */
struct GameOver {
  PerPlayerScores final_scores;
  int final_level;
};

/**
 * @brief Everything observable at the boundary after one interaction, in order of occurrence
 */
struct Observations {
  std::vector<ArenaView> arena_views;
  std::vector<Status> statuses;
  std::optional<GameOver> game_over;
};

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
  bool cadences{false};  // Cadences run (between cadence START and STOP)
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
