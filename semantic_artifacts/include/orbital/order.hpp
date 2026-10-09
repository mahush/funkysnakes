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
 * Order vocabulary shared by the Order Authority and the Arena Authority
 *
 * The Order Authority hands eligible orders to the Arena; the Arena reports what actually
 * happened to them. Every item carries the identities needed to correlate it, also when it
 * arrives late in a distributed realization.
 */

/**
 * @brief An order: move a player's snake to a board position
 *
 * The target is a position, not a food item: food disappearing there does not change the order.
 */
struct Order {
  RoundId round;
  OrderId id;
  PlayerId player;
  Point target;

  bool operator==(const Order& other) const noexcept {
    return round == other.round && id == other.id && player == other.player && target == other.target;
  }
};

// ============================================================================
// Activation outcomes: what the Arena did with an eligible order
// ============================================================================

/**
 * @brief The order became the executing order of its snake
 *
 * `replaced` names the order that was executing until now and is interrupted by this one.
 */
struct OrderActivated {
  OrderId order;
  std::optional<OrderId> replaced;
  std::vector<Direction> route;

  bool operator==(const OrderActivated& other) const noexcept {
    return order == other.order && replaced == other.replaced && route == other.route;
  }
};

/**
 * @brief Why the Arena refused to activate an order
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
  OrderId order;
  RejectionReason reason;

  bool operator==(const OrderRejected& other) const noexcept { return order == other.order && reason == other.reason; }
};

using ActivationOutcome = std::variant<OrderActivated, OrderRejected>;

// ============================================================================
// Order endings: how an executing order stopped executing
// ============================================================================

/**
 * @brief Why an executing order stopped executing
 */
enum class EndReason {
  COMPLETED,    // The snake reached the target and survived the step
  INTERRUPTED,  // A replacement was activated before the target was reached
  ELIMINATED,   // The snake died before reaching the target
  ROUND_ENDED   // Execution of the round ended before the target was reached
};

/**
 * @brief An executing order stopped executing
 *
 * `replacement` names the interrupting order for INTERRUPTED.
 */
struct OrderEnded {
  OrderId order;
  EndReason reason;
  std::optional<OrderId> replacement;

  bool operator==(const OrderEnded& other) const noexcept {
    return order == other.order && reason == other.reason && replacement == other.replacement;
  }
};

}  // namespace orbital
}  // namespace snake
