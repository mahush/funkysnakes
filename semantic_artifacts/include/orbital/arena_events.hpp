#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "common/arena_events.hpp"
#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {

/**
 * Arena events - what happened in the Orbital arena during a step, in order
 *
 * The Arena reports physical and execution events only; what they are worth is decided by the
 * Scoring Policy, and their effect on order status by the Order Authority.
 *
 * Collisions use the shared arena event vocabulary (Bitten, SelfBitten, MutualBite), since the
 * collision rules are the same as in classic snake.
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

using ArenaEvent = std::variant<OrderEnded, FoodCollected, Bitten, SelfBitten, MutualBite>;
using ArenaEvents = std::vector<ArenaEvent>;

}  // namespace orbital
}  // namespace snake
