#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {

/**
 * Arena facts - what happened in the Orbital arena during a step, in order
 *
 * The Arena reports physical and execution facts only; what they are worth is decided by the
 * Scoring Policy, and their effect on order status by the Order Authority.
 */

/**
 * @brief A snake's head collected food at a position
 *
 * `during` names the order executing at that moment, or nothing if no order was executing.
 */
struct FoodCollected {
  PlayerId player;
  Point position;
  std::optional<OrderId> during;

  bool operator==(const FoodCollected& other) const noexcept {
    return player == other.player && position == other.position && during == other.during;
  }
};

using ArenaFact = std::variant<OrderEnded, FoodCollected>;
using ArenaFacts = std::vector<ArenaFact>;

}  // namespace orbital
}  // namespace snake
