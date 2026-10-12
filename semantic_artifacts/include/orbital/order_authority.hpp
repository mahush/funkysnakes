#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "common/game_time.hpp"
#include "orbital/arena_facts.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

/**
 * Order Authority - owns the delayed lifecycle of the players' orders
 *
 * Owns submitted orders, the latest pending order per player, its fixed eligibility deadline,
 * supersession, eligibility and the status of every order interpreted from what the Arena
 * reports. It does not own the actual executing route (Arena) or any points (Round).
 *
 * - The Orbital Latency Policy calculates a submission's deadline; this Authority stores it.
 * - A new submission supersedes the player's pending order; the superseded order never activates.
 * - Eligibility is a query: it does not consume the pending order. The pending order is resolved
 *   only when the Arena's outcome for it is observed.
 * - What the Arena actually did wins over this Authority's own expectation, and late reports for
 *   executing orders still count after pending orders were cancelled.
 */

/**
 * @brief Status of one order
 */
enum class Status {
  PENDING,         // Waiting for its deadline and activation
  SUPERSEDED,      // Replaced by a newer submission before it activated
  CANCELLED,       // Still pending when the round concluded
  REJECTED,        // The Arena refused to activate it
  EXECUTING,       // The Arena executes it
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
  GameTime issued_at;
  GameTime deadline;
  Status status;
  std::optional<RejectionReason> rejection;
};

/**
 * @brief Order state owned by the Order Authority
 *
 * Fields may be read freely. Changes must go through the transitions below only.
 */
struct State {
  RoundId round;
  int next_order_id{1};
  std::map<OrderId, OrderRecord> orders;  // Every order of the round by identity
  std::map<PlayerId, OrderId> pending;    // The latest pending order per player
};

/**
 * @brief A player's request to move their snake to a position
 */
struct Submission {
  PlayerId player;
  Point target;
  GameTime issued_at;  // Assigned from the round's logical clock
};

/**
 * @brief Start the order lifecycle of a round
 *
 * @param round Identity of the round
 * @return State without any orders
 */
State startRound(RoundId round);

/**
 * @brief Accept a submission as a new order
 *
 * The order gets a new identity and a deadline from the Orbital Latency Policy. A pending order
 * of the same player is superseded.
 *
 * @param state Current order state
 * @param submission The player's request
 * @return Tuple of (updated state, identity of the new order)
 */
std::tuple<State, OrderId> submit(State state, const Submission& submission);

/**
 * @brief Pending orders whose deadline has elapsed
 *
 * At most one per player: the latest pending order. Does not change the state.
 *
 * @param state Current order state
 * @param now Current time on the round's timeline
 * @return Eligible orders, ordered by player
 */
std::vector<Order> eligible(const State& state, GameTime now);

/**
 * @brief Interpret what the Arena reported for one step
 *
 * Activation outcomes resolve pending orders; order endings resolve executing orders.
 * Reports about unknown orders are ignored.
 *
 * @param state Current order state
 * @param outcomes Activation outcomes of the step
 * @param facts Arena facts of the step
 * @return Updated order state
 */
State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaFacts& facts);

/**
 * @brief Cancel all pending orders because the round concluded
 *
 * Executing orders are left alone: their ending is reported by the Arena.
 *
 * @param state Current order state
 * @return Order state without pending orders
 */
State cancel(State state);

/**
 * @brief A player's current order situation
 */
struct PlayerOrders {
  std::optional<OrderRecord> pending;    // Waiting for activation
  std::optional<OrderRecord> executing;  // Executing according to the Arena
};

/**
 * @brief Projection of the order state for presentation
 */
struct View {
  std::map<PlayerId, PlayerOrders> players;  // Every player who submitted an order this round
  std::vector<OrderRecord> orders;           // All orders of the round, by identity
};

/**
 * @brief Project the order state
 *
 * @param state Current order state
 * @return Pending and executing order per player, and all orders with their status
 */
View view(const State& state);

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
