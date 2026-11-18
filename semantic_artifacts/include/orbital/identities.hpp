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

// Identifies one arena step within a round; what time a step spans is the Pacing Policy's
struct StepId {
  int value;

  constexpr bool operator==(const StepId& other) const noexcept { return value == other.value; }
  constexpr bool operator!=(const StepId& other) const noexcept { return value != other.value; }
  constexpr bool operator<(const StepId& other) const noexcept { return value < other.value; }
  constexpr bool operator<=(const StepId& other) const noexcept { return value <= other.value; }
  constexpr bool operator>(const StepId& other) const noexcept { return value > other.value; }
  constexpr bool operator>=(const StepId& other) const noexcept { return value >= other.value; }
};

}  // namespace orbital
}  // namespace snake
