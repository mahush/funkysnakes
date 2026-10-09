#pragma once

namespace snake {
namespace orbital {

/**
 * Identities shared by the Orbital Orders artifacts
 *
 * Distinct types keep a round, an order and a step from being mixed up.
 */

// Identifies one round
struct RoundId {
  int value;

  bool operator==(const RoundId& other) const noexcept { return value == other.value; }
  bool operator!=(const RoundId& other) const noexcept { return value != other.value; }
  bool operator<(const RoundId& other) const noexcept { return value < other.value; }
};

// Identifies one accepted order submission; each submission gets a new identity
struct OrderId {
  int value;

  bool operator==(const OrderId& other) const noexcept { return value == other.value; }
  bool operator!=(const OrderId& other) const noexcept { return value != other.value; }
  bool operator<(const OrderId& other) const noexcept { return value < other.value; }
};

// Identifies one arena step (movement tick) within a round
struct StepId {
  int value;

  bool operator==(const StepId& other) const noexcept { return value == other.value; }
  bool operator!=(const StepId& other) const noexcept { return value != other.value; }
  bool operator<(const StepId& other) const noexcept { return value < other.value; }
};

}  // namespace orbital
}  // namespace snake
