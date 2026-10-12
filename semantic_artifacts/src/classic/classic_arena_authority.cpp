#include "classic/classic_arena_authority.hpp"

#include "classic/scoring_policy.hpp"
#include "common/game_logic.hpp"
#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/functional_utils.hpp"
#include "snake/generic_lens.hpp"

namespace snake {
namespace classic_arena_authority {

using funkypipes::bindFront;
using funkypipes::makePipe;

namespace {

// ============================================================================
// Arena Lenses - internal to the Authority
// ============================================================================
// Each lens focuses one game-rule helper on part of the arena state.
// They are kept private so the arena transition can only be composed here.

template <typename TOp>
auto over_direction_command_filter_state(TOp op) {
  return lens(mutate<&State::direction_command_filter_state>, read<>, std::move(op));
}

template <typename TOp>
auto over_direction_command_filter_state_viewing_snakes(TOp op) {
  return lens(mutate<&State::direction_command_filter_state>, read<&State::snakes>, std::move(op));
}

template <typename TOp>
auto over_snakes_viewing_board_and_food(TOp op) {
  return lens(mutate<&State::snakes>, read<&State::board, &State::food_items>, std::move(op));
}

template <typename TOp>
auto over_snakes_and_scores(TOp op) {
  return lens(mutate<&State::snakes, &State::scores>, read<>, std::move(op));
}

template <typename TOp>
auto over_snakes_and_events(TOp op) {
  return lens(mutate<&State::snakes, &State::events>, read<>, std::move(op));
}

template <typename TOp>
auto over_food_and_events_viewing_snakes(TOp op) {
  return lens(mutate<&State::food_items, &State::events>, read<&State::snakes>, std::move(op));
}

template <typename TOp>
auto over_scores_viewing_events(TOp op) {
  return lens(mutate<&State::scores>, read<&State::events>, std::move(op));
}

template <typename TOp>
auto over_food(TOp op) {
  return lens(mutate<&State::food_items>, read<>, std::move(op));
}

template <typename TOp>
auto over_food_viewing_board_and_snakes(TOp op) {
  return lens(mutate<&State::food_items>, read<&State::board, &State::snakes>, std::move(op));
}

// ============================================================================
// Step Helpers
// ============================================================================

bool isBiteDropFoodMode(const State& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

bool shouldRepositionFood(const State& state) { return state.should_reposition_food; }

State clearRepositionFlag(State state) {
  state.should_reposition_food = false;
  return state;
}

// Collision handling as a step of the arena: its events are appended to the step's events
std::tuple<PerPlayerSnakes, ArenaEvents, std::vector<Point>> handleCollisionsOfStep(PerPlayerSnakes snakes,
                                                                                    ArenaEvents events) {
  auto [new_snakes, collision_events, dropped_segments] = handleCollisions(std::move(snakes));
  for (const CollisionEvent& event : collision_events) {
    std::visit([&events](const auto& item) { events.push_back(item); }, event);
  }
  return {std::move(new_snakes), std::move(events), std::move(dropped_segments)};
}

// Eating as a step of the arena: its events are appended to the step's events
std::tuple<FoodItems, ArenaEvents> handleFoodEatingOfStep(FoodItems food_items,
                                                          ArenaEvents events,
                                                          const PerPlayerSnakes& snakes) {
  auto [new_food, eating_events] = handleFoodEating(std::move(food_items), snakes);
  events.insert(events.end(), eating_events.begin(), eating_events.end());
  return {std::move(new_food), std::move(events)};
}

State clearEvents(State state) {
  state.events.clear();
  return state;
}

}  // namespace

State initial(const RandomIntGeneratorFn& random_int, Board board) {
  State state;
  state.board = board;

  auto setup =
      makePipe(over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_A}, Point{5, 10}, Direction::RIGHT, 7)),
               over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_B}, Point{5, 15}, Direction::RIGHT, 7)),
               over_food_viewing_board_and_snakes(bindFront(initializeFood, random_int, MIN_FOOD_COUNT)));
  return setup(std::move(state));
}

State steer(State state, const DirectionCommand& cmd) {
  return over_direction_command_filter_state_viewing_snakes(direction_command_filter::try_add)(std::move(state), cmd);
}

State requestFoodReposition(State state) {
  state.should_reposition_food = true;
  return state;
}

State tick(const RandomIntGeneratorFn& random_int, State state) {
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args

  // clang-format off
  auto tick_pipeline = makePipe(
      clearEvents,                                                                                    // → state
      over_direction_command_filter_state(direction_command_filter::try_consume_next),                // → (state, next_directions)
      over_snakes_viewing_board_and_food(moveSnakes),                                                 // → state
      over_snakes_and_events(handleCollisionsOfStep),                                               // → (state, dropped_segments)
      when<0>(isBiteDropFoodMode, over_food(dropSegmentsAsFood)),                                     // → state
      over_food_and_events_viewing_snakes(handleFoodEatingOfStep),                                  // → state
      over_scores_viewing_events(scoring_policy::applyScoring),                                       // → state
      over_food_viewing_board_and_snakes(bindFront(replenishFood, random_int, MIN_FOOD_COUNT)),       // → state
      when(shouldRepositionFood,
           over_food_viewing_board_and_snakes(bindFront(repositionRandomFood, random_int))),          // → state
      clearRepositionFlag);                                                                           // → state
  // clang-format on

  return tick_pipeline(std::move(state));
}

}  // namespace classic_arena_authority
}  // namespace snake
