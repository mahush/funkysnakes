#pragma once

#include <chrono>

#include "common/game_time.hpp"

namespace snake {
namespace orbital {
namespace latency_policy {

/**
 * Orbital Latency Policy - decides when a submitted order becomes eligible
 *
 * Stateless decision: the communication delay follows a deterministic triangle wave over the
 * orbital cycle. A round starts at the lowest delay, reaches the highest delay at half the cycle
 * and returns to the lowest delay at the end of the cycle; the cycle then repeats.
 *
 * A submission's deadline is fixed by its own issue time. A later change of the orbital phase
 * does not move it; storing and waiting for the deadline is owned by the Order Authority.
 */

constexpr std::chrono::milliseconds MIN_DELAY{400};
constexpr std::chrono::milliseconds MAX_DELAY{3000};
constexpr std::chrono::milliseconds CYCLE{30000};

/**
 * @brief Communication delay for an order issued at the given time
 *
 * @param issued_at Issue time on the round's timeline (not negative)
 * @return Delay between MIN_DELAY and MAX_DELAY, rounded down to whole milliseconds
 */
constexpr std::chrono::milliseconds delayAt(GameTime issued_at) {
  const std::chrono::milliseconds half_cycle = CYCLE / 2;
  const std::chrono::milliseconds phase = issued_at % CYCLE;
  const std::chrono::milliseconds from_low = phase <= half_cycle ? phase : CYCLE - phase;
  return MIN_DELAY + (MAX_DELAY - MIN_DELAY) * from_low.count() / half_cycle.count();
}

/**
 * @brief Deadline after which an order issued at the given time is eligible
 *
 * @param issued_at Issue time on the round's timeline (not negative)
 * @return Issue time plus the delay at issue time
 */
constexpr GameTime deadlineFor(GameTime issued_at) { return issued_at + delayAt(issued_at); }

}  // namespace latency_policy
}  // namespace orbital
}  // namespace snake
