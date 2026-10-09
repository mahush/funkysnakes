#pragma once

#include <chrono>

namespace snake {

/**
 * @brief Logical game time: elapsed time since the start of the round
 *
 * One logical clock drives a round. Durations on this timeline (delays, intervals) use the same
 * resolution.
 */
using GameTime = std::chrono::milliseconds;

}  // namespace snake
