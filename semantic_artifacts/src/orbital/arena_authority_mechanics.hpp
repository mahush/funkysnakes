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

namespace detail {

// The Authority's own access to the private representation of its state
struct Access {
  using Representation = State::Representation;

  static Representation& rep(State& state) { return state.rep_; }
  static const Representation& rep(const State& state) { return state.rep_; }

  // Applies a transformation of the representation to the state, forwarding additional outputs
  template <typename TOp>
  static auto over_representation(TOp op_fn) {
    return [op_fn = std::move(op_fn)](State state, auto&&... args) mutable {
      auto result = op_fn(std::move(state.rep_), std::forward<decltype(args)>(args)...);
      if constexpr (std::is_same_v<decltype(result), Representation>) {
        state.rep_ = std::move(result);
        return state;
      } else {
        state.rep_ = std::move(std::get<0>(result));
        return std::tuple_cat(std::make_tuple(std::move(state)), tailOf(std::move(result)));
      }
    };
  }

 private:
  template <typename TFirst, typename... TRest>
  static std::tuple<TRest...> tailOf(std::tuple<TFirst, TRest...> tuple) {
    return std::apply([](TFirst&&, TRest&&... rest) { return std::make_tuple(std::move(rest)...); }, std::move(tuple));
  }
};

}  // namespace detail

namespace mechanics {

using Rep = detail::Access::Representation;

inline Rep& rep(State& state) { return detail::Access::rep(state); }
inline const Rep& rep(const State& state) { return detail::Access::rep(state); }

// ============================================================================
// Arena Lenses
// ============================================================================

template <auto... TMutate, auto... TRead, typename TOp>
auto over_state(mutate_t<TMutate...> mutate_tag, read_t<TRead...> read_tag, TOp op_fn) {
  return detail::Access::over_representation(lens(mutate_tag, read_tag, std::move(op_fn)));
}

template <typename TOp>
auto over_active(TOp op_fn) {
  return over_state(mutate<&Rep::active>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_active_viewing_snakes(TOp op_fn) {
  return over_state(mutate<&Rep::active>, read<&Rep::snakes>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_viewing_board_and_food(TOp op_fn) {
  return over_state(mutate<&Rep::snakes>, read<&Rep::board, &Rep::food>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_and_food(TOp op_fn) {
  return over_state(mutate<&Rep::snakes, &Rep::food>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_snakes_and_active(TOp op_fn) {
  return over_state(mutate<&Rep::food>, read<&Rep::snakes, &Rep::active>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_board_and_snakes(TOp op_fn) {
  return over_state(mutate<&Rep::food>, read<&Rep::board, &Rep::snakes>, std::move(op_fn));
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
  ++rep(state).step.value;
  return state;
}

}  // namespace mechanics
}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
