#pragma once

#include <chrono>
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
 * part in the next step, and what is submitted in the step is sent at a later opportunity.
 *
 * The schedule is known in advance, so players can plan around the coming opportunities. What is sent
 * at an opportunity is the Order Selection Authority's; the Policy keeps no orders.
 */

constexpr std::chrono::milliseconds MIN_INTERVAL{500};   // At the end of the game
constexpr std::chrono::milliseconds MAX_INTERVAL{4000};  // At the start of the game
constexpr StepId FIRST_OPPORTUNITY{1};

/**
 * @brief Number of steps from an opportunity to the next one
 *
 * @param opportunity Step of an opportunity (not negative)
 * @return Steps on the line from MAX_INTERVAL at the start of the game down to MIN_INTERVAL at its end
 *         (and after it), linear in the opportunity's time, rounded to the nearest whole number of steps
 *         (halves round up), at least one: 20 steps at the start, 3 at the end
 */
constexpr int intervalAfter(StepId opportunity) {
  const std::chrono::milliseconds duration = pacing_policy::GAME_DURATION;
  const StepId elapsed = opportunity < pacing_policy::LAST_STEP ? opportunity : pacing_policy::LAST_STEP;
  // The interval scaled by the duration stays whole, so it is rounded to steps only once
  const std::chrono::milliseconds::rep scaled =
      MAX_INTERVAL.count() * duration.count() -
      (MAX_INTERVAL - MIN_INTERVAL).count() * pacing_policy::timeOfStep(elapsed).count();
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
 * @brief Step a transmission is scheduled to take part in
 *
 * The opportunity in a step comes after the step's movement, so what is sent there takes part in the next step.
 *
 * @param transmitted_in Step whose opportunity sent the order (not negative)
 * @return The step after transmitted_in
 */
constexpr StepId scheduledStepOf(StepId transmitted_in) { return StepId{transmitted_in.value + 1}; }

/**
 * @brief Whether a transmission takes part in a step
 *
 * @param transmitted_in Step whose opportunity sent the order (not negative)
 * @param step Step in question (not negative)
 * @return True if step is the transmission's scheduled step
 */
constexpr bool takesPartIn(StepId transmitted_in, StepId step) { return step == scheduledStepOf(transmitted_in); }

/**
 * @brief First opportunity after a completion
 *
 * A completion happens in its step's movement, which comes before the step's opportunity, so an opportunity in
 * the completion step itself already follows it.
 *
 * @param completed_in Step whose movement completed the order (not negative)
 * @return Step of the earliest opportunity not before completed_in
 */
constexpr StepId firstOpportunityAfterCompletion(StepId completed_in) { return firstOpportunityFrom(completed_in); }

/**
 * @brief Opportunity that sends a submission, if it is still selected then
 *
 * A step's submissions come after the step's opportunity, so a submission in an opportunity's step is sent at a
 * later one.
 *
 * @param submitted_in Step in which the submission happened (not negative)
 * @return Step of the earliest opportunity strictly after submitted_in
 */
constexpr StepId opportunityOfSubmission(StepId submitted_in) { return nextOpportunityAfter(submitted_in); }

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
