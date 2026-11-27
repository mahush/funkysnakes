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
 * Transmission Reception Authority - owns which of a phone's facts take effect on arrival, and which transmissions of a
 * round wait for their step
 *
 * Platform Authority: where a phone's facts travel over a link, they can arrive early or late, also before the round
 * started or after it concluded. A fact takes effect only while the round runs; before and after, it is answered with
 * its receipt and changes nothing. A received transmission waits, and its order is handed over for the step it takes
 * part in, at most one order per player and step. Whether a transmission waits, takes part, also late, or is
 * superseded is the Transmission Participation Policy's answer. What differs from a transmission taking part in its
 * scheduled step is reported: as a deviation, or as a receipt other than RECEIVED.
 *
 * It relies on the transport guarantee: each of a phone's facts, its submission and transmission records, arrives at
 * most once, and each phone's facts arrive in the order the phone sent them. Delay is not limited, and facts of
 * different phones may arrive in any order. A transport that resends, for instance after a reconnect, does not keep
 * the guarantee by itself.
 *
 * It owns no selection (Order Selection Authority), decides no activation (Arena Authority) and no points (Game
 * Authority), and keeps no outcome of an order it handed over.
 */

/**
 * @brief What became of a phone's fact that arrived
 */
enum class Receipt {
  RECEIVED,           // It takes effect; a transmission waits for its step
  ROUND_NOT_STARTED,  // The round has not started yet
  ROUND_CONCLUDED     // The round has concluded
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
  bool started_{false};
  bool concluded_{false};

  // Construction
  friend State initial();

  // Transitions
  friend State startRound(State state);
  friend std::tuple<State, Receipt> receive(State state, const OrderTransmitted& transmitted);
  friend std::tuple<State, std::vector<Order>, std::vector<LateTransmission>, std::vector<SupersededTransmission>>
  release(State state, StepId step);
  friend std::tuple<State, std::vector<DiscardedTransmission>> conclude(State state);

  // Queries
  friend Receipt receiptOf(const State& state, const OrderSubmitted& submitted);
  friend std::vector<OrderTransmitted> waiting(const State& state);
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Set up the reception before its round started
 *
 * @return State without any waiting transmission, in which no fact takes effect yet
 */
State initial();

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

/**
 * @brief Start the reception of the round's facts
 *
 * From now on the facts that arrive take effect, until the conclusion. Starting a reception that started already
 * changes nothing.
 *
 * @param state Current reception state
 * @return State in which arriving facts take effect
 */
State startRound(State state);

/**
 * @brief Receive a transmission
 *
 * While the round runs, the transmission waits for its step, whether it arrives early, in time or late. Before the
 * start and after the conclusion it changes nothing.
 *
 * Admitted input: transmissions of one round, as the transport guarantee delivers them; in any order across players and
 * relative to the released steps.
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
 * @brief What becomes of a submission record that arrives now
 *
 * The record takes effect only while the round runs; the Authority keeps no submission.
 *
 * Admitted input: submission records of one round, as the transport guarantee delivers them.
 *
 * @param state Current reception state
 * @param submitted The submission record that arrived
 * @return RECEIVED while the round runs, otherwise ROUND_NOT_STARTED or ROUND_CONCLUDED
 */
Receipt receiptOf(const State& state, const OrderSubmitted& submitted);

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
