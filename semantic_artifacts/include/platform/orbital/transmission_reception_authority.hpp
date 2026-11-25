#pragma once

#include <tuple>
#include <vector>

#include "domain/common/players.hpp"
#include "domain/orbital/identities.hpp"
#include "domain/orbital/order.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace transmission_reception_authority {

/**
 * Transmission Reception Authority - owns which transmissions of a round wait for their step
 *
 * Platform Authority: where transmissions travel over a link, they can arrive early or late. It keeps them
 * waiting and hands their orders over for the steps they take part in, at most one order per player and step. Whether
 * a transmission waits, takes part, also late, or is superseded is the Transmission Participation Policy's answer. What
 * differs from a transmission taking part in its scheduled step is reported: as a deviation, or as the receipt
 * ROUND_CONCLUDED of a transmission that arrives after the conclusion.
 *
 * It relies on the transport guarantee: each transmission arrives at most once, and a player's transmissions arrive in
 * the order they were sent. Delay is not limited, and transmissions of different players may arrive in any order.
 * A transport that resends, for instance after a reconnect, does not keep the guarantee by itself.
 *
 * It owns no selection (Order Selection Authority), decides no activation (Arena Authority) and no points (Game
 * Authority), and keeps no outcome of an order it handed over.
 */

/**
 * @brief What became of a transmission that arrived
 */
enum class Receipt {
  RECEIVED,        // It waits for its step
  ROUND_CONCLUDED  // The round has concluded
};

/**
 * @brief Deviation: an order was handed over after its scheduled step
 */
struct LateTransmission {
  OrderTransmitted transmitted;
  StepId step;  // Step it was handed over for

  bool operator==(const LateTransmission& other) const noexcept {
    return transmitted == other.transmitted && step == other.step;
  }
};

/**
 * @brief Deviation: an order is never handed over, because a newer one of its player took its place
 */
struct SupersededTransmission {
  OrderTransmitted transmitted;
  OrderNumber by;  // The newer order
  StepId step;     // Step for which it lost its place

  bool operator==(const SupersededTransmission& other) const noexcept {
    return transmitted == other.transmitted && by == other.by && step == other.step;
  }
};

/**
 * @brief Deviation: an order is never handed over, because it still waited when the round concluded
 */
struct DiscardedTransmission {
  OrderTransmitted transmitted;

  bool operator==(const DiscardedTransmission& other) const noexcept { return transmitted == other.transmitted; }
};

/**
 * @brief Reception state owned by the Transmission Reception Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below. Its fields are private: it
 * is read through the queries and evolves only through the transitions.
 */
class State {
 private:
  State() = default;

  std::vector<OrderTransmitted> waiting_;  // Received, not yet handed over, in order of receipt
  bool concluded_{false};

  // Construction
  friend State startRound();

  // Transitions
  friend std::tuple<State, Receipt> receive(State state, const OrderTransmitted& transmitted);
  friend std::tuple<State, std::vector<Order>, std::vector<LateTransmission>, std::vector<SupersededTransmission>>
  release(State state, StepId step);
  friend std::tuple<State, std::vector<DiscardedTransmission>> conclude(State state);

  // Queries
  friend std::vector<OrderTransmitted> waiting(const State& state);
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Start the reception of a round's transmissions
 *
 * @return State without any waiting transmission
 */
State startRound();

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Receive a transmission
 *
 * The transmission waits for its step, whether it arrives early, in time or late. After the conclusion it changes
 * nothing.
 *
 * Admitted input: transmissions of one round, each at most once, and each player's transmissions in the order they were
 * sent (the transport guarantee); in any order across players and relative to the released steps.
 *
 * @param state Current reception state
 * @param transmitted The transmission that arrived
 * @return Tuple of (updated state, what became of the transmission)
 */
std::tuple<State, Receipt> receive(State state, const OrderTransmitted& transmitted);

/**
 * @brief Hand over the orders for a step that is about to run
 *
 * Every waiting transmission is treated as the Transmission Participation Policy answers for the step: it keeps
 * waiting, its order is handed over, or it is superseded and dropped. Afterwards only transmissions that are not due
 * yet still wait.
 *
 * Admitted input: every step of the round from the first on, each exactly once, in step order and without gaps.
 *
 * Only the orders are to be handed over, all of them; the late and the superseded transmissions are reported only.
 *
 * @param state Current reception state
 * @param step Step about to run
 * @return Tuple of (updated state, orders to hand over at most one per player ordered by player, those of the orders
 *         handed over after their scheduled step, transmissions superseded and never handed over; both in order of
 *         receipt)
 */
std::tuple<State, std::vector<Order>, std::vector<LateTransmission>, std::vector<SupersededTransmission>> release(
    State state, StepId step);

/**
 * @brief Conclude the round
 *
 * Orders that still wait are discarded; nothing is received or handed over any more.
 *
 * @param state Current reception state
 * @return Tuple of (concluded state, the discarded orders in order of receipt)
 */
std::tuple<State, std::vector<DiscardedTransmission>> conclude(State state);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief The received transmissions that wait to be handed over
 *
 * @param state Current reception state
 * @return Waiting transmissions in order of receipt
 */
std::vector<OrderTransmitted> waiting(const State& state);

}  // namespace transmission_reception_authority
}  // namespace platform
}  // namespace orbital
}  // namespace snake
