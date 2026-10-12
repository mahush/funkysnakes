#pragma once

// Mechanics of the Orbital Arena Authority: plumbing that makes no gameplay decision.
// Private to the Authority's implementation; the step sequence, its guards and rules stay in arena_authority.cpp.

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "common/arena_events.hpp"
#include "orbital/arena_events.hpp"

namespace snake {
namespace orbital {
namespace arena_authority {
namespace mechanics {

// ============================================================================
// Event accumulation - each stage reports only its own events
// ============================================================================

inline ArenaEvents appendEvents(ArenaEvents events, const ArenaEvents& more) {
  events.insert(events.end(), more.begin(), more.end());
  return events;
}

// Collision events as arena events, in order
inline ArenaEvents toArenaEvents(const CollisionEvents& collision_events) {
  ArenaEvents events;
  for (const CollisionEvent& event : collision_events) {
    std::visit([&events](const auto& item) { events.push_back(item); }, event);
  }
  return events;
}

// Lifts a stage TState -> TState or TState -> (TState, its events) to (TState, events so far) -> (TState, events so
// far followed by the stage's events)
template <typename TState, typename TStageFn>
auto with_event_accumulation(TStageFn stage_fn) {
  return [stage_fn = std::move(stage_fn)](TState state, ArenaEvents events) mutable {
    auto result = stage_fn(std::move(state));
    if constexpr (std::is_same_v<decltype(result), TState>) {
      return std::make_tuple(std::move(result), std::move(events));
    } else {
      auto [next, stage_events] = std::move(result);
      return std::make_tuple(std::move(next), appendEvents(std::move(events), stage_events));
    }
  };
}

}  // namespace mechanics
}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
