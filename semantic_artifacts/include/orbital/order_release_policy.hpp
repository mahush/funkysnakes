#pragma once

#include <chrono>

#include "common/game_time.hpp"

namespace snake {
namespace orbital {
namespace order_release_policy {

/**
 * Order Release Policy - decides when a committed order is released for activation
 *
 * Stateless decision: the Order Delay follows a deterministic triangle wave over the orbital cycle.
 * A round starts at the lowest delay, reaches the highest delay at half the cycle and returns to
 * the lowest delay at the end of the cycle; the cycle then repeats.
 *
 * An order's release time is fixed by its own commitment time; a later change of the orbital phase
 * does not move it. A released order is only available for activation: the Arena Authority tries to activate
 * it at the first step at or after its release time and may refuse it. The Policy keeps no orders;
 * the pending orders are the Order Authority's.
 */

constexpr std::chrono::milliseconds MIN_DELAY{200};
constexpr std::chrono::milliseconds MAX_DELAY{2000};
constexpr std::chrono::milliseconds CYCLE{60000};

/**
 * @brief Order Delay of an order committed at the given time
 *
 * @param committed_at Commitment time on the round's timeline (not negative)
 * @return Delay between MIN_DELAY and MAX_DELAY, rounded down to whole milliseconds
 */
constexpr std::chrono::milliseconds delayAt(GameTime committed_at) {
  const std::chrono::milliseconds half_cycle = CYCLE / 2;
  const std::chrono::milliseconds phase = committed_at % CYCLE;
  const std::chrono::milliseconds from_low = phase <= half_cycle ? phase : CYCLE - phase;
  return MIN_DELAY + (MAX_DELAY - MIN_DELAY) * from_low.count() / half_cycle.count();
}

/**
 * @brief Order Release Time of an order committed at the given time
 *
 * @param committed_at Commitment time on the round's timeline (not negative)
 * @return Commitment time plus the Order Delay at commitment time
 */
constexpr GameTime releaseTime(GameTime committed_at) { return committed_at + delayAt(committed_at); }

/**
 * @brief Whether an order committed at the given time is released at a given time
 *
 * @param committed_at Commitment time on the round's timeline (not negative)
 * @param now Time on the round's timeline
 * @return True once the order's release time has been reached
 */
constexpr bool isReleased(GameTime committed_at, GameTime now) { return releaseTime(committed_at) <= now; }

}  // namespace order_release_policy
}  // namespace orbital
}  // namespace snake
