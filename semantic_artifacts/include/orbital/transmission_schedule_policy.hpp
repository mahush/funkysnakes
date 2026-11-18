#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "orbital/identities.hpp"
#include "orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {
namespace transmission_schedule_policy {

/**
 * Transmission Schedule Policy - decides when the orbital link offers a transmission opportunity
 *
 * Stateless decision on the game's steps. The first opportunity falls on the first arena step; the
 * interval from one opportunity to the next shrinks linearly over the game, from the longest interval
 * at its start to the shortest at its end, taken at the earlier opportunity and rounded to the
 * nearest whole number of steps, so the link grows busier as the game goes on. With 200 ms steps the
 * shortest interval comes out as 3 steps, 0.6 s.
 *
 * An order passes three events: it is submitted (selected, still replaceable), transmitted at an opportunity
 * (irrevocable) and activated by the arena at the start of a later step's movement. Within a step, the movement
 * comes first, then the step's opportunity, then the step's submissions: what is sent at the opportunity takes
 * part from the next step on, and what is submitted in the step is sent at a later opportunity.
 *
 * The schedule is known in advance, so players can plan around the coming opportunities. What is sent
 * at an opportunity is the Order Authority's; the Policy keeps no orders.
 */

constexpr std::chrono::milliseconds MIN_INTERVAL{500};   // At the end of the game
constexpr std::chrono::milliseconds MAX_INTERVAL{4000};  // At the start of the game
constexpr StepId FIRST_OPPORTUNITY{1};

/**
 * @brief Number of steps from an opportunity to the next one
 *
 * @param opportunity Step of an opportunity (not negative)
 * @return From MAX_INTERVAL at the start of the game down to MIN_INTERVAL at its end (and after it),
 *         linear in the opportunity's time, rounded to the nearest whole number of steps (halves round
 *         up), at least one
 */
constexpr int intervalAfter(StepId opportunity) {
  const std::chrono::milliseconds duration = pacing_policy::GAME_DURATION;
  const StepId elapsed = opportunity < pacing_policy::LAST_STEP ? opportunity : pacing_policy::LAST_STEP;
  // The interval scaled by the duration stays whole, so it is rounded to steps only once
  const std::chrono::milliseconds::rep scaled = MAX_INTERVAL.count() * duration.count() -
                                                (MAX_INTERVAL - MIN_INTERVAL).count() *
                                                    pacing_policy::timeOfStep(elapsed).count();
  const std::chrono::milliseconds::rep scaled_step = pacing_policy::STEP_INTERVAL.count() * duration.count();
  const std::chrono::milliseconds::rep steps = (scaled + scaled_step / 2) / scaled_step;
  return steps < 1 ? 1 : static_cast<int>(steps);
}

/**
 * @brief First opportunity at or after a given step
 *
 * @param at A step (not negative)
 * @return Step of the earliest opportunity not before at
 */
constexpr StepId firstOpportunityFrom(StepId at) {
  StepId opportunity = FIRST_OPPORTUNITY;
  while (opportunity < at) {
    opportunity = StepId{opportunity.value + intervalAfter(opportunity)};
  }
  return opportunity;
}

/**
 * @brief First opportunity after a given step
 *
 * @param at A step (not negative)
 * @return Step of the earliest opportunity strictly after at
 */
constexpr StepId nextOpportunityAfter(StepId at) { return firstOpportunityFrom(StepId{at.value + 1}); }

/**
 * @brief Whether the link offers an opportunity in a given step
 *
 * @param at A step (not negative)
 * @return True if an opportunity falls on at
 */
constexpr bool isOpportunity(StepId at) { return firstOpportunityFrom(at) == at; }

/**
 * @brief Last opportunity before a given step
 *
 * @param at A step (not negative)
 * @return Step of the latest opportunity strictly before at; empty if none precedes it
 */
constexpr std::optional<StepId> lastOpportunityBefore(StepId at) {
  if (at <= FIRST_OPPORTUNITY) {
    return std::nullopt;
  }
  StepId last = FIRST_OPPORTUNITY;
  while (StepId{last.value + intervalAfter(last)} < at) {
    last = StepId{last.value + intervalAfter(last)};
  }
  return last;
}

/**
 * @brief Every opportunity of a game
 *
 * @return Steps of the opportunities up to the last step of the game (Pacing Policy), in step order; computed once
 */
inline const std::vector<StepId>& opportunitiesOfGame() {
  static const std::vector<StepId> opportunities = [] {
    std::vector<StepId> result;
    for (StepId opportunity = FIRST_OPPORTUNITY; opportunity <= pacing_policy::LAST_STEP;
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
