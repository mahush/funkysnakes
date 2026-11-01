#pragma once

#include <chrono>

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

}  // namespace pacing_policy
}  // namespace orbital
}  // namespace snake
