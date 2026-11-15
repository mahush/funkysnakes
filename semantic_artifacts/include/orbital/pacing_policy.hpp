#pragma once

#include <chrono>

#include "orbital/identities.hpp"

namespace snake {
namespace orbital {
namespace pacing_policy {

/**
 * Orbital Pacing Policy - decides the pace of an Orbital Orders game
 *
 * Stateless decisions on the game timeline: how often the arena steps and how long a game lasts.
 * It keeps no clock and concludes nothing.
 */

// Interval between arena steps; steps fall due at its multiples, the first one interval after round start
constexpr std::chrono::milliseconds STEP_INTERVAL{200};

// Time limit of a game, counted from round start
constexpr std::chrono::minutes GAME_DURATION{2};

/**
 * @brief Time at which a step falls due
 *
 * @param step A step of the round; step 0 stands for round start
 * @return Time of the step on the round's timeline, step intervals after round start
 */
constexpr std::chrono::milliseconds timeOfStep(StepId step) { return STEP_INTERVAL * step.value; }

}  // namespace pacing_policy
}  // namespace orbital
}  // namespace snake
