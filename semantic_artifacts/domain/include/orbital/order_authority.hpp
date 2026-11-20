#pragma once

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "orbital/arena_events.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

/**
 * Order Authority - owns the lifecycle of the players' orders from submission to transmission
 *
 * Owns the submitted orders with their submission and transmission steps, each player's selected
 * order until it is transmitted, the transmitted orders until the Arena Authority resolves them, and
 * the status of every order interpreted from what the Arena Authority reports. It does not own the
 * actual executing route (Arena Authority), when the orbital link offers an opportunity
 * (Transmission Schedule Policy) or any points (Game Authority).
 *
 * - An order is identified by its player and its number; each player's submissions are numbered from 1.
 * - A submission takes effect where the player's input is accepted: it becomes the player's selected
 *   order at once, with its own submission time, which is part of the domain history and published
 *   as OrderSubmitted.
 * - A new submission replaces the player's selected order; the replaced order is never transmitted.
 * - At a transmission, each player's selected order becomes irrevocable and is published as
 *   OrderTransmitted, keeping its own submission time. A transmitted order is never recalled; a
 *   correction is a new submission, transmitted later.
 * - Querying the transmitted orders does not consume them. A transmitted order is resolved only when
 *   the Arena Authority's outcome for it is observed.
 * - What the Arena Authority actually did wins over this Authority's own expectation, and late reports for
 *   executing orders still count after the remaining orders were cancelled.
 */

/**
 * @brief Status of one order
 */
enum class Status {
  SELECTED,        // Submitted, waiting for a transmission
  REPLACED,        // Replaced by a newer submission before it was transmitted
  TRANSMITTED,     // Transmitted, waiting for activation
  CANCELLED,       // Still selected or transmitted when the round concluded
  REJECTED,        // The Arena Authority refused to activate it
  EXECUTING,       // The Arena Authority executes it
  COMPLETED,       // Reached its target
  INTERRUPTED,     // Replaced while executing
  FAILED,          // Its snake died while executing it
  ENDED_BY_ROUND,  // Still executing when the round ended
};

/**
 * @brief What is known about one order
 */
struct OrderRecord {
  Order order;
  StepId submitted_at;
  std::optional<StepId> transmitted_at;  // Set once transmitted
  std::optional<OrderNumber> replaced;   // The selected order this submission replaced
  Status status;
  std::optional<RejectionReason> rejection;
};

struct Submission;
struct View;

/**
 * @brief Order state owned by the Order Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below. Its
 * fields are private: orders are read through the queries and evolve only through the
 * transitions.
 */
class State {
 private:
  RoundId round_;
  std::map<PlayerId, int> submitted_count_;                        // Orders submitted so far per player
  std::map<PlayerId, std::map<OrderNumber, OrderRecord>> orders_;  // Every order of the round per player, by number
  std::map<PlayerId, OrderNumber> selected_;                       // The selected, not yet transmitted order per player
  std::map<PlayerId, std::set<OrderNumber>>
      transmitted_;  // Transmitted orders the Arena Authority has not resolved yet

  // Construction
  friend State startRound(RoundId round);

  // Transitions
  friend std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission);
  friend std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at);
  friend State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events);
  friend State cancel(State state);

  // Queries
  friend std::vector<Order> transmittedOrders(const State& state, StepId at);
  friend View view(const State& state);
};

/**
 * @brief A player's request to move their snake to a position
 */
struct Submission {
  PlayerId player;
  Point target;
  StepId submitted_at;  // Step in which the player submitted it
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Start the order lifecycle of a round
 *
 * @param round Identity of the round
 * @return State without any orders
 */
State startRound(RoundId round);

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Select a submission as the player's new order
 *
 * The order gets the player's next number and becomes the player's selected order. A selected, not yet
 * transmitted order of the same player is replaced; transmitted and executing orders are unaffected.
 *
 * @param state Current order state
 * @param submission The player's request
 * @return Tuple of (updated state, submission record with the new order's number)
 */
std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission);

/**
 * @brief Transmit every player's selected order whose opportunity has come
 *
 * Each selected order becomes transmitted and irrevocable, keeping its own submission step and the
 * order it replaced; players
 * without a selected order transmit nothing, so an order is transmitted once. A selected order whose
 * submission is sent at a later opportunity (Transmission Schedule Policy), such as one submitted in
 * the transmission's own step, stays selected.
 *
 * Admitted input: the step of a transmission opportunity.
 *
 * @param state Current order state
 * @param at Step of the transmission
 * @return Tuple of (updated state, transmission records ordered by player)
 */
std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at);

/**
 * @brief Interpret what the Arena Authority reported for one step
 *
 * Activation outcomes resolve transmitted orders; order endings resolve executing orders.
 * An outcome or ending applies to the order its player and number name; reports about unknown orders are
 * ignored.
 *
 * Admitted input: the complete report of one step of this round, each step once and in step
 * order, with the step's outcomes before its events. Order numbers restart every round, so
 * keeping reports of another round out is the caller's guarantee. Duplicate, missing or
 * reordered steps are a concern of the realization delivering the reports.
 *
 * Valid status transitions:
 * - SELECTED → REPLACED (submit), TRANSMITTED (transmit) or CANCELLED (cancel)
 * - TRANSMITTED → CANCELLED (cancel), REJECTED or EXECUTING (outcome)
 * - Any status → EXECUTING: the Arena Authority actually activated it; what the Arena Authority did wins
 * - EXECUTING → COMPLETED, INTERRUPTED, FAILED or ENDED_BY_ROUND (OrderEnded), also after cancel
 * - A rejection reported for an order that is not transmitted changes nothing.
 * Final statuses are not yet protected against late OrderActivated or OrderEnded reports (Known Issues #3).
 *
 * @param state Current order state
 * @param outcomes Activation outcomes of the step
 * @param events Arena events of the step
 * @return Updated order state
 */
State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events);

/**
 * @brief Cancel all selected and transmitted orders
 *
 * Executing orders are left unchanged; their ending still counts when it is observed.
 *
 * @param state Current order state
 * @return Order state without selected or transmitted orders
 */
State cancel(State state);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief The transmitted orders the Arena Authority has not resolved yet, as far as they can activate at a step
 *
 * Which step a transmitted order first takes part in is the Transmission Schedule Policy's answer, whatever
 * order the calls come in.
 * Does not change the state; a transmitted order is resolved only when the Arena Authority's outcome
 * for it is observed. A player has several only if transmissions follow each other before the
 * outcome of the earlier one was observed.
 *
 * @param state Current order state
 * @param at Step the orders are handed to
 * @return Transmitted orders whose first step after transmission (Transmission Schedule Policy) is at or before at,
 *         by player, then by number; the order across players is fixed for determinism and carries no meaning
 */
std::vector<Order> transmittedOrders(const State& state, StepId at);

/**
 * @brief A player's current order situation
 */
struct PlayerOrders {
  std::optional<OrderRecord> selected;     // Waiting for a transmission
  std::optional<OrderRecord> transmitted;  // Latest transmitted order waiting for activation
  std::optional<OrderRecord> executing;    // Executing according to the Arena Authority
};

/**
 * @brief Projection of the order state for presentation
 */
struct View {
  std::map<PlayerId, PlayerOrders> players;  // Every player who submitted an order this round
  std::vector<OrderRecord> orders;           // All orders of the round, by player, then by number; the order
                                             // across players carries no meaning
};

/**
 * @brief Project the order state
 *
 * @param state Current order state
 * @return Selected, transmitted and executing order per player, and all orders with their status
 */
View view(const State& state);

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
