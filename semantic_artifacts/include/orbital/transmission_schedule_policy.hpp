#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "common/game_time.hpp"
#include "orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {
namespace transmission_schedule_policy {

/**
 * Transmission Schedule Policy - decides when the orbital link offers a transmission opportunity
 *
 * Stateless decision on the game timeline. The first opportunity falls on the first arena step; the
 * interval from one opportunity to the next shrinks linearly over the game, from the longest interval
 * at its start to the shortest at its end, taken at the earlier opportunity and rounded to the
 * nearest whole step interval, so the link grows busier as the game goes on. Every opportunity
 * therefore falls on an arena step; with 200 ms steps the shortest interval comes out as 0.6 s.
 *
 * An opportunity at a step's time comes after that step: what is sent there takes part in a later step.
 *
 * The schedule is known in advance, so players can plan around the coming opportunities. What is sent
 * at an opportunity is the Order Authority's; the Policy keeps no orders.
 */

constexpr std::chrono::milliseconds MIN_INTERVAL{500};   // At the end of the game
constexpr std::chrono::milliseconds MAX_INTERVAL{4000};  // At the start of the game
constexpr GameTime FIRST_OPPORTUNITY{pacing_policy::STEP_INTERVAL};

/**
 * @brief Interval from an opportunity to the next one
 *
 * @param opportunity Time of an opportunity on the round's timeline (not negative)
 * @return From MAX_INTERVAL at the start of the game down to MIN_INTERVAL at its end (and after it),
 *         linear in the opportunity's time, rounded to the nearest whole step interval (halves round
 *         up), at least one step
 */
constexpr std::chrono::milliseconds intervalAfter(GameTime opportunity) {
  const GameTime duration = pacing_policy::GAME_DURATION;
  const GameTime elapsed = opportunity < duration ? opportunity : duration;
  // The interval scaled by the duration stays whole, so it is rounded to steps only once
  const std::chrono::milliseconds::rep scaled =
      MAX_INTERVAL.count() * duration.count() - (MAX_INTERVAL - MIN_INTERVAL).count() * elapsed.count();
  const std::chrono::milliseconds::rep scaled_step = pacing_policy::STEP_INTERVAL.count() * duration.count();
  const std::chrono::milliseconds rounded = pacing_policy::STEP_INTERVAL * ((scaled + scaled_step / 2) / scaled_step);
  return rounded < pacing_policy::STEP_INTERVAL ? pacing_policy::STEP_INTERVAL : rounded;
}

/**
 * @brief First opportunity at or after a given time
 *
 * @param at Time on the round's timeline (not negative)
 * @return Time of the earliest opportunity not before at
 */
constexpr GameTime firstOpportunityFrom(GameTime at) {
  GameTime opportunity = FIRST_OPPORTUNITY;
  while (opportunity < at) {
    opportunity += intervalAfter(opportunity);
  }
  return opportunity;
}

/**
 * @brief First opportunity after a given time
 *
 * @param at Time on the round's timeline (not negative)
 * @return Time of the earliest opportunity strictly after at
 */
constexpr GameTime nextOpportunityAfter(GameTime at) { return firstOpportunityFrom(at + GameTime{1}); }

/**
 * @brief Whether the link offers an opportunity at a given time
 *
 * @param at Time on the round's timeline (not negative)
 * @return True if an opportunity falls exactly on at
 */
constexpr bool isOpportunity(GameTime at) { return firstOpportunityFrom(at) == at; }

/**
 * @brief Last opportunity before a given time
 *
 * @param at Time on the round's timeline (not negative)
 * @return Time of the latest opportunity strictly before at; empty if none precedes it
 */
constexpr std::optional<GameTime> lastOpportunityBefore(GameTime at) {
  if (at <= FIRST_OPPORTUNITY) {
    return std::nullopt;
  }
  GameTime last = FIRST_OPPORTUNITY;
  while (last + intervalAfter(last) < at) {
    last += intervalAfter(last);
  }
  return last;
}

/**
 * @brief Every opportunity of a game
 *
 * @return Times of the opportunities up to the end of the game (Pacing Policy), in time order; computed once
 */
inline const std::vector<GameTime>& opportunitiesOfGame() {
  static const std::vector<GameTime> opportunities = [] {
    std::vector<GameTime> result;
    for (GameTime opportunity = FIRST_OPPORTUNITY; opportunity <= pacing_policy::GAME_DURATION;
         opportunity = nextOpportunityAfter(opportunity)) {
      result.push_back(opportunity);
    }
    return result;
  }();
  return opportunities;
}

}  // namespace transmission_schedule_policy
}  // namespace orbital
}  // namespace snake
