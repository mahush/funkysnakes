#pragma once

#include <chrono>

#include "domain/orbital/identities.hpp"

namespace snake {
namespace orbital {
namespace pacing_policy {

/**
 * Orbital Pacing Policy - decides the pace of an Orbital Orders game
 *
 * Stateless decisions on the game timeline: how often the arena steps and how long a game lasts.
 * It keeps no clock and concludes nothing.
 *
 * Step k spans the time from the arena's k-th movement up to its next one; step 0 spans the time from
 * round start up to the first movement. Whatever happens in that time happens in step k, so the
 * domain's times are steps.
 */

// Interval between arena steps; steps fall due at its multiples, the first one interval after round start
constexpr std::chrono::milliseconds STEP_INTERVAL{200};

// Time limit of a game, counted from round start
constexpr std::chrono::minutes GAME_DURATION{2};

// Step that falls due when the time limit is reached; no step follows it
constexpr StepId LAST_STEP{static_cast<int>(GAME_DURATION / STEP_INTERVAL)};
static_assert(GAME_DURATION % STEP_INTERVAL == std::chrono::milliseconds{0}, "the time limit falls on a step");

/**
 * @brief Time at which a step falls due
 *
 * @param step A step of the round; step 0 stands for round start
 * @return Time of the step on the round's timeline, step intervals after round start
 */
constexpr std::chrono::milliseconds timeOfStep(StepId step) { return STEP_INTERVAL * step.value; }

/**
 * @brief Step that a time of the round falls in
 *
 * A time at which a step falls due belongs to that step, after its movement.
 *
 * @param since_round_start Time on the round's timeline; a time before round start belongs to step 0
 * @return The step whose time span contains since_round_start
 */
constexpr StepId stepAt(std::chrono::milliseconds since_round_start) {
  if (since_round_start < std::chrono::milliseconds{0}) {
    return StepId{0};
  }
  return StepId{static_cast<int>(since_round_start / STEP_INTERVAL)};
}

}  // namespace pacing_policy
}  // namespace orbital
}  // namespace snake
