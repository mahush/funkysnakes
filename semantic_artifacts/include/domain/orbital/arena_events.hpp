#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "domain/common/arena_events.hpp"
#include "domain/common/game_primitives.hpp"
#include "domain/common/players.hpp"
#include "domain/orbital/identities.hpp"
#include "domain/orbital/order.hpp"

namespace snake {
namespace orbital {

/**
 * Arena events - what happened in the Orbital arena during a step, in order
 *
 * The Arena Authority reports physical and execution events only; what they are worth is decided by the
 * Scoring Policy.
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
  std::optional<OrderNumber> during;

  bool operator==(const FoodCollected& other) const noexcept {
    return player == other.player && position == other.position && during == other.during;
  }
};

using ArenaEvent = std::variant<OrderEnded, FoodCollected, Bitten, SelfBitten, MutualBite>;
using ArenaEvents = std::vector<ArenaEvent>;

}  // namespace orbital
}  // namespace snake
