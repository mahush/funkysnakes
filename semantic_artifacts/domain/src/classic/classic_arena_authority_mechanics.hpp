#pragma once

// Mechanics of the Classic Arena Authority: state plumbing that makes no gameplay decision.
// Private to the Authority's implementation; the step sequence, its guards and rules stay in
// classic_arena_authority.cpp.

#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "classic/classic_arena_authority.hpp"
#include "classic/scoring_policy.hpp"
#include "common/game_logic.hpp"
#include "snake/generic_lens.hpp"

namespace snake {
namespace classic_arena_authority {
namespace mechanics {

// ============================================================================
// Arena Lenses
// ============================================================================
// Each lens focuses one game-rule helper on part of the arena state.

template <typename TOp>
auto over_direction_command_filter_state(TOp op_fn) {
  return lens(mutate<&State::direction_command_filter_state>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_direction_command_filter_state_viewing_snakes(TOp op_fn) {
  return lens(mutate<&State::direction_command_filter_state>, read<&State::snakes>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_viewing_board_and_food(TOp op_fn) {
  return lens(mutate<&State::snakes>, read<&State::board, &State::food_items>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes_and_scores(TOp op_fn) {
  return lens(mutate<&State::snakes, &State::scores>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_snakes(TOp op_fn) {
  return lens(mutate<&State::snakes>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_events(TOp op_fn) {
  return lens(mutate<&State::events>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_snakes(TOp op_fn) {
  return lens(mutate<&State::food_items>, read<&State::snakes>, std::move(op_fn));
}

template <typename TOp>
auto over_scores_viewing_events(TOp op_fn) {
  return lens(mutate<&State::scores>, read<&State::events>, std::move(op_fn));
}

template <typename TOp>
auto over_food(TOp op_fn) {
  return lens(mutate<&State::food_items>, read<>, std::move(op_fn));
}

template <typename TOp>
auto over_food_viewing_board_and_snakes(TOp op_fn) {
  return lens(mutate<&State::food_items>, read<&State::board, &State::snakes>, std::move(op_fn));
}

// ============================================================================
// Flags and events of the step
// ============================================================================

inline State clearRepositionFlag(State state) {
  state.should_reposition_food = false;
  return state;
}

inline State clearEvents(State state) {
  state.events.clear();
  return state;
}

// Collision events as arena events, in order
inline std::tuple<PerPlayerSnakes, ArenaEvents, std::vector<Point>> handleCollisionsOfStep(PerPlayerSnakes snakes) {
  auto [new_snakes, collision_events, dropped_segments] = handleCollisions(std::move(snakes));
  ArenaEvents events;
  for (const CollisionEvent& event : collision_events) {
    std::visit([&events](const auto& item) { events.push_back(item); }, event);
  }
  return {std::move(new_snakes), std::move(events), std::move(dropped_segments)};
}

// Eating events as arena events, in order
inline std::tuple<FoodItems, ArenaEvents> handleFoodEatingOfStep(FoodItems food_items, const PerPlayerSnakes& snakes) {
  auto [new_food, eating_events] = handleFoodEating(std::move(food_items), snakes);
  return {std::move(new_food), ArenaEvents{eating_events.begin(), eating_events.end()}};
}

// Adds what a score change names
inline PerPlayerScores applyScoreChange(PerPlayerScores scores, const scoring_policy::ScoreChange& change) {
  scores[change.player] += change.points;
  return scores;
}

// The step's events so far, followed by those a stage reported
inline ArenaEvents appendEvents(ArenaEvents events, const ArenaEvents& more) {
  events.insert(events.end(), more.begin(), more.end());
  return events;
}

// Moves the events a stage reported (pipeline argument 1) into the step's events, forwarding the rest
template <typename... TRest>
auto appendStageEvents(State state, ArenaEvents stage_events, TRest&&... rest) {
  State next = over_events(appendEvents)(std::move(state), stage_events);
  if constexpr (sizeof...(TRest) == 0) {
    return next;
  } else {
    return std::make_tuple(std::move(next), std::forward<TRest>(rest)...);
  }
}

}  // namespace mechanics
}  // namespace classic_arena_authority
}  // namespace snake
