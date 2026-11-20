#pragma once

#include <variant>
#include <vector>

#include "common/arena_events.hpp"

namespace snake {
namespace classic_arena_authority {

/**
 * Classic arena events - what happened during a classic arena step, in order
 *
 * Collects the events reported by the shared arena rules: eating and collisions.
 */
using ArenaEvent = std::variant<FoodEaten, Bitten, SelfBitten, MutualBite>;
using ArenaEvents = std::vector<ArenaEvent>;

}  // namespace classic_arena_authority
}  // namespace snake
