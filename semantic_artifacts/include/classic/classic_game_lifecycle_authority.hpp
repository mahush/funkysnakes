#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <tuple>

#include "classic/game_types.hpp"

namespace snake {
namespace classic_game_lifecycle_authority {

/**
 * Classic Game Lifecycle Authority - owns the evolution of a classic game as a whole
 *
 * Owns the game phase (running, paused, over), the level, the conclusion check and the
 * meaning of game-time cadences (level-up and food reposition periods). It sits beside the
 * Classic Arena Authority: it decides when the arena is stepped and when food is repositioned,
 * and reads the arena's alive states to decide whether the game is over.
 *
 * Elapsed cadence periods are supplied by the realization as events; this Authority decides
 * what they mean and when the cadences run.
 */

// Game-time cadences
constexpr std::chrono::seconds LEVEL_PERIOD{60};
constexpr std::chrono::seconds REPOSITION_PERIOD{5};

/**
 * @brief Game-time work that can fall due: an arena step or the end of a cadence period
 */
enum class DueWork { STEP, LEVEL_PERIOD, REPOSITION_PERIOD };

/**
 * @brief Order in which work falling due at the same instant counts
 *
 * The arena step comes first, so a step that ends the game counts before a level-up at the
 * same moment; periods ending once the game is over are ignored.
 */
constexpr std::array<DueWork, 3> SAME_INSTANT_ORDER{DueWork::STEP, DueWork::LEVEL_PERIOD, DueWork::REPOSITION_PERIOD};

/**
 * @brief Lifecycle state owned by the Classic Game Lifecycle Authority
 *
 * Fields may be read freely. Changes must go through the transitions below only.
 *
 * `paused` and `over` are independent flags, mirroring current behavior: pausing is not
 * blocked once the game is over (see esa_classic_game.md, behavior to pin).
 */
struct State {
  int level{1};        // Current level
  bool paused{false};  // Stepping and cadences are suspended
  bool over{false};    // Conclusion was detected
};

/**
 * @brief Intent to start, stop, pause or resume stepping the arena
 */
enum class ClockIntent { START, STOP, PAUSE, RESUME };

/**
 * @brief Intent to change the arena's step interval
 */
struct StepIntervalIntent {
  int interval_ms;
};

/**
 * @brief Intent to start, stop, freeze or resume the game-time cadences (level-up and reposition periods)
 *
 * FREEZE keeps the elapsed part of each period; RESUME continues from there, because game time
 * stops while the game is paused.
 */
enum class CadenceIntent { START, STOP, FREEZE, RESUME };

/**
 * @brief Intent to request a food reposition from the arena
 */
struct RepositionIntent {};

/**
 * @brief Intent to conclude the game: announce game over, stop stepping and stop the cadences
 */
struct ConcludeIntent {
  ClockIntent clock{ClockIntent::STOP};
  CadenceIntent cadence{CadenceIntent::STOP};
};

/**
 * @brief Start a new game
 *
 * The arena is stepped at the Difficulty Policy's interval for the starting level.
 *
 * @param starting_level Level the game starts at
 * @param state Current lifecycle state
 * @return Tuple of (running state, start-clock intent, initial step interval intent, start-cadences intent)
 */
std::tuple<State, ClockIntent, StepIntervalIntent, CadenceIntent> start(int starting_level, State state);

/**
 * @brief Toggle between running and paused
 *
 * Pausing freezes the cadences and resuming continues them. Once the game is over the
 * cadences stay stopped.
 *
 * Note: toggling is not blocked after the game is over (see esa_classic_game.md).
 *
 * @param state Current lifecycle state
 * @return Tuple of (toggled state, pause or resume clock intent, optional freeze or resume cadence intent)
 */
std::tuple<State, ClockIntent, std::optional<CadenceIntent>> togglePause(State state);

/**
 * @brief React to an elapsed level period
 *
 * Ignored while paused or once the game is over. Otherwise the level increases and the step interval follows
 * the Difficulty Policy.
 *
 * @param state Current lifecycle state
 * @return Tuple of (state, optional step interval intent)
 */
std::tuple<State, std::optional<StepIntervalIntent>> levelPeriodElapsed(State state);

/**
 * @brief React to an elapsed reposition period
 *
 * Ignored while paused or once the game is over.
 *
 * @param state Current lifecycle state
 * @return Optional reposition intent (the lifecycle state does not change)
 */
std::optional<RepositionIntent> repositionPeriodElapsed(const State& state);

/**
 * @brief Observe the arena's alive states and conclude the game when no snake is alive
 *
 * Ignored if the game is already over.
 *
 * @param state Current lifecycle state
 * @param alive_states Alive state per player, as reported by the arena
 * @return Tuple of (state, optional conclude intent)
 */
std::tuple<State, std::optional<ConcludeIntent>> observeAliveStates(State state,
                                                                    const PerPlayerAliveStates& alive_states);

}  // namespace classic_game_lifecycle_authority
}  // namespace snake
