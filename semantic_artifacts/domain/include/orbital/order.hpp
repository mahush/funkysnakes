#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/identities.hpp"

namespace snake {
namespace orbital {

/**
 * Order vocabulary shared by the Order Selection Authority and the Arena Authority
 *
 * The Order Selection Authority's transmitted orders reach the Arena Authority, and it publishes its submission and
 * transmission history; the Arena Authority reports what actually happened to the orders. Every item carries the
 * identities needed to correlate it, also when it arrives late in a distributed realization.
 */

/**
 * @brief An order: move a player's snake to a board position
 *
 * The target is a position, not a food item: food disappearing there does not change the order.
 */
struct Order {
  RoundId round;
  OrderNumber id;
  PlayerId player;
  Point target;

  bool operator==(const Order& other) const noexcept {
    return round == other.round && id == other.id && player == other.player && target == other.target;
  }
};

// ============================================================================
// Submission history: what the Order Selection Authority accepted from a player
// ============================================================================

/**
 * @brief A player submitted an order, which became their selected order
 *
 * Submission happens where the player's input is accepted; `submitted_at` is the step in which that
 * happened, and it stays the order's submission step however late the record travels. The record
 * carries the order's target, so the selected order is complete in it.
 * `replaced` names the player's previously selected, not yet transmitted order that this submission
 * replaced; a replaced order is never transmitted.
 */
struct OrderSubmitted {
  RoundId round;
  OrderNumber order;
  PlayerId player;
  Point target;
  StepId submitted_at;
  std::optional<OrderNumber> replaced;

  bool operator==(const OrderSubmitted& other) const noexcept {
    return round == other.round && order == other.order && player == other.player && target == other.target &&
           submitted_at == other.submitted_at && replaced == other.replaced;
  }
};

/**
 * @brief A selected order was transmitted at an opportunity of the orbital link and became irrevocable
 *
 * It carries the order's target, so the transmitted order is complete without its submission. It keeps its
 * own submission step and names the selected order it replaced, if any. Together with the Arena Authority's
 * activations and order endings, this history tells whether an order took over from its predecessor as a
 * handover, without the submissions that were never transmitted.
 */
struct OrderTransmitted {
  RoundId round;
  OrderNumber order;
  PlayerId player;
  Point target;
  StepId submitted_at;
  StepId transmitted_at;
  std::optional<OrderNumber> replaced;

  bool operator==(const OrderTransmitted& other) const noexcept {
    return round == other.round && order == other.order && player == other.player && target == other.target &&
           submitted_at == other.submitted_at && transmitted_at == other.transmitted_at && replaced == other.replaced;
  }
};

// ============================================================================
// Activation outcomes: what the Arena Authority did with a transmitted order
// ============================================================================

/**
 * @brief The order became the executing order of its player's snake
 *
 * `replaced` names the player's order that was executing until now and is interrupted by this one.
 */
struct OrderActivated {
  OrderNumber order;
  PlayerId player;
  std::optional<OrderNumber> replaced;
  std::vector<Direction> route;

  bool operator==(const OrderActivated& other) const noexcept {
    return order == other.order && player == other.player && replaced == other.replaced && route == other.route;
  }
};

/**
 * @brief Why the Arena Authority refused to activate an order
 */
enum class RejectionReason {
  SNAKE_DEAD,        // The player's snake is no longer alive
  TARGET_OFF_BOARD,  // The target position lies outside the board
  ROUND_ENDED        // The order belongs to another round, or execution of this round has ended
};

/**
 * @brief The order was not activated; whatever was executing keeps executing
 */
struct OrderRejected {
  OrderNumber order;
  PlayerId player;
  RejectionReason reason;

  bool operator==(const OrderRejected& other) const noexcept {
    return order == other.order && player == other.player && reason == other.reason;
  }
};

using ActivationOutcome = std::variant<OrderActivated, OrderRejected>;

// ============================================================================
// Order endings: how an executing order stopped executing
// ============================================================================

/**
 * @brief Why an executing order stopped executing
 */
enum class EndReason {
  COMPLETED,    // The snake was at the target when activated, or moved there and survived the step
  INTERRUPTED,  // A replacement was activated before the target was reached
  ELIMINATED,   // The snake died before reaching the target
  ROUND_ENDED   // Execution of the round ended before the target was reached
};

/**
 * @brief An executing order stopped executing
 *
 * `replacement` names the player's interrupting order for INTERRUPTED.
 */
struct OrderEnded {
  OrderNumber order;
  PlayerId player;
  EndReason reason;
  std::optional<OrderNumber> replacement;

  bool operator==(const OrderEnded& other) const noexcept {
    return order == other.order && player == other.player && reason == other.reason && replacement == other.replacement;
  }
};

}  // namespace orbital
}  // namespace snake
