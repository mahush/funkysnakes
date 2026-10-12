#pragma once

// Mechanics of the Orbital Arena Authority: state plumbing that makes no gameplay decision.
// Private to the Authority's implementation; the step sequence, its guards and rules stay in arena_authority.cpp.

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "common/arena_events.hpp"
#include "orbital/arena_authority.hpp"
#include "snake/generic_lens.hpp"

namespace snake {
namespace orbital {
namespace arena_authority {
namespace mechanics {

// ============================================================================
// Arena Lenses
// ============================================================================

template <typename TOp>
auto over_active(TOp op_fn) {
  return lens(mutate<&State::active>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_active_viewing_snakes(TOp op_fn) {
  return lens(mutate<&State::active>, read<&State::snakes>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_viewing_board_and_food(TOp op_fn) {
  return lens(mutate<&State::snakes>, read<&State::board, &State::food>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_and_food(TOp op_fn) {
  return lens(mutate<&State::snakes, &State::food>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_snakes_and_active(TOp op_fn) {
  return lens(mutate<&State::food>, read<&State::snakes, &State::active>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_board_and_snakes(TOp op_fn) {
  return lens(mutate<&State::food>, read<&State::board, &State::snakes>, std::move(op_fn));
}

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

// Lifts a stage State -> State or State -> (State, its events) to (State, events so far) -> (State, events so far
// followed by the stage's events)
template <typename TStageFn>
auto with_event_accumulation(TStageFn stage_fn) {
  return [stage_fn = std::move(stage_fn)](State state, ArenaEvents events) mutable {
    auto result = stage_fn(std::move(state));
    if constexpr (std::is_same_v<decltype(result), State>) {
      return std::make_tuple(std::move(result), std::move(events));
    } else {
      auto [next, stage_events] = std::move(result);
      return std::make_tuple(std::move(next), appendEvents(std::move(events), stage_events));
    }
  };
}

// ============================================================================
// Step counter
// ============================================================================

inline State advanceStep(State state) {
  ++state.step.value;
  return state;
}

}  // namespace mechanics
}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
