#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "orbital/arena_events.hpp"
#include "orbital/forwarded_order_facts.hpp"
#include "orbital/order.hpp"
#include "orbital/order_authority.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace order_replica_authority {

/**
 * Order Replica Authority - owns what the host knows of the players' orders
 *
 * Platform Authority of the distributed realization. Each player's Order Authority runs on the player's device
 * and decides submission, replacement and transmission there; the device forwards the published facts
 * (ForwardedSubmission, ForwardedTransmission). The replica records them and applies no Order rule of its own:
 * - A submission becomes the player's selected order; the order it replaced is the one its `replaced` names.
 * - A transmission keeps its own steps; it leaves the selection, and it takes part in the step the Transmission
 *   Schedule Policy names, or, if that step already ran on the host, where the Late Transmission Policy places it.
 * - What the Arena Authority's reports mean for an order's status is the Order Resolution Policy's answer.
 * - At the conclusion, what is still selected or transmitted is cancelled, as the Order Authority does.
 *
 * Its view has the shape of the Order Authority's view, so the host composes the game's views as the Domain
 * Application does. It does not own the facts it records (the players' Order Authorities), the executing route
 * (Arena Authority) or any points (Game Authority).
 */

/**
 * @brief A transmission that reached the host after the step it was scheduled for had run: a platform deviation
 */
struct LateTransmission {
  PlayerId player;
  OrderNumber order;
  StepId transmitted_at;
  StepId scheduled_step;  // First step after the transmission (Transmission Schedule Policy)
  StepId first_step;      // First step it took part in (Late Transmission Policy)

  bool operator==(const LateTransmission& other) const noexcept {
    return player == other.player && order == other.order && transmitted_at == other.transmitted_at &&
           scheduled_step == other.scheduled_step && first_step == other.first_step;
  }
};

/**
 * @brief Order state owned by the Order Replica Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below; its fields are
 * private.
 */
class State {
 private:
  RoundId round_;
  StepId first_unprocessed_step_;  // First step the host has not run yet
  bool concluded_{false};
  std::map<PlayerId, std::map<OrderNumber, order_authority::OrderRecord>> orders_;  // Every recorded order
  std::map<PlayerId, OrderNumber> selected_;  // The selected, not yet transmitted order per player
  std::map<PlayerId, std::map<OrderNumber, StepId>>
      transmitted_;  // Transmitted orders not yet resolved, with the first step each takes part in

  // Construction
  friend State startRound(RoundId round, StepId first_step);

  // Transitions
  friend State recordSubmission(State state, const ForwardedSubmission& forwarded);
  friend std::tuple<State, std::optional<LateTransmission>> recordTransmission(
      State state, const ForwardedTransmission& forwarded);
  friend State observe(State state,
                       StepId step,
                       const std::vector<ActivationOutcome>& outcomes,
                       const ArenaEvents& events);
  friend State conclude(State state, const ArenaEvents& end_events);

  // Queries
  friend std::vector<Order> transmittedOrders(const State& state, StepId at);
  friend order_authority::View view(const State& state);
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Start recording the orders of a round
 *
 * @param round Identity of the round
 * @param first_step First step the host will run
 * @return State without any orders
 */
State startRound(RoundId round, StepId first_step);

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Record that a player's Order Authority selected a submission
 *
 * The order becomes the player's selected order; the order its `replaced` names, if still selected, is replaced.
 *
 * Admitted input: a player's facts in the order their Order Authority published them. Ignored, without effect:
 * a submission of another round, one whose Order and fact name different orders, an order already recorded, and
 * anything after the conclusion (the Game Authority ignores it as well).
 *
 * @param state Current replica state
 * @param forwarded The forwarded submission
 * @return Updated replica state
 */
State recordSubmission(State state, const ForwardedSubmission& forwarded);

/**
 * @brief Record that a player's Order Authority transmitted an order
 *
 * The order becomes transmitted with its own steps and stops being selected. A transmission whose submission was
 * never recorded is recorded from the Order it carries. Which step it takes part in is the Late Transmission
 * Policy's placement against the first step the host has not run yet.
 *
 * Admitted input: as for recordSubmission; a transmission of an order that is already transmitted or resolved
 * is ignored as well.
 *
 * @param state Current replica state
 * @param forwarded The forwarded transmission
 * @return Tuple of (updated state, the deviation if the transmission arrived late)
 */
std::tuple<State, std::optional<LateTransmission>> recordTransmission(State state,
                                                                      const ForwardedTransmission& forwarded);

/**
 * @brief Interpret what the Arena Authority reported for one step that ran on the host
 *
 * Statuses change as the Order Resolution Policy decides; reports about unknown orders are ignored. Afterwards
 * the step counts as run, so a transmission arriving later than the step it was scheduled for is late.
 *
 * Admitted input: the complete report of each step once and in step order, outcomes before events.
 *
 * @param state Current replica state
 * @param step The step that ran
 * @param outcomes Activation outcomes of the step
 * @param events Arena events of the step
 * @return Updated replica state
 */
State observe(State state, StepId step, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events);

/**
 * @brief Interpret the endings at the conclusion, then cancel all selected and transmitted orders
 *
 * Executing orders end as the events report. Forwarded facts are ignored from now on.
 *
 * @param state Current replica state
 * @param end_events Order endings because execution of the round ended
 * @return Concluded replica state
 */
State conclude(State state, const ArenaEvents& end_events);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief The transmitted orders not yet resolved, as far as they take part in a step
 *
 * Does not change the state; a transmitted order is resolved only when the Arena Authority's outcome for it is
 * observed, so a resolved order is never handed out again.
 *
 * @param state Current replica state
 * @param at Step the orders are handed to
 * @return Transmitted, unresolved orders whose first step is at or before at, by player, then by number
 */
std::vector<Order> transmittedOrders(const State& state, StepId at);

/**
 * @brief Project the recorded orders, in the shape of the Order Authority's view
 *
 * @param state Current replica state
 * @return Selected, transmitted and executing order per player, and all recorded orders with their status
 */
order_authority::View view(const State& state);

}  // namespace order_replica_authority
}  // namespace platform
}  // namespace orbital
}  // namespace snake
