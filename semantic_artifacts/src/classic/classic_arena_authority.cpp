#include "classic/classic_arena_authority.hpp"

#include "classic/scoring_policy.hpp"
#include "classic_arena_authority_mechanics.hpp"
#include "common/game_logic.hpp"
#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_callable.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/functional_utils.hpp"

namespace snake {
namespace classic_arena_authority {

using funkypipes::bindFront;
using funkypipes::makePipe;
using namespace mechanics;  // NOLINT(google-build-using-namespace): private plumbing of this Authority

namespace {

// ============================================================================
// Guards of the step
// ============================================================================

bool isBiteDropFoodMode(const State& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

bool shouldRepositionFood(const State& state) { return state.should_reposition_food; }

}  // namespace

State initial(const RandomIntGeneratorFn& random_int_fn, Board board) {
  State state;
  state.board = board;

  auto setup =
      makePipe(over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_A}, Point{5, 10}, Direction::RIGHT, 7)),
               over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_B}, Point{5, 15}, Direction::RIGHT, 7)),
               over_food_viewing_board_and_snakes(bindFront(initializeFood, random_int_fn, MIN_FOOD_COUNT)));
  return setup(std::move(state));
}

State steer(State state, const DirectionCommand& cmd) {
  return over_direction_command_filter_state_viewing_snakes(direction_command_filter::try_add)(std::move(state), cmd);
}

State requestFoodReposition(State state) {
  state.should_reposition_food = true;
  return state;
}

State tick(const RandomIntGeneratorFn& random_int_fn, State state) {
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args

  // clang-format off
  auto tick_pipeline_fn = makePipe(
      clearEvents,                                                                                    // → state
      over_direction_command_filter_state(direction_command_filter::try_consume_next),                // → (state, next_directions)
      over_snakes_viewing_board_and_food(moveSnakes),                                                 // → state
      over_snakes(handleCollisionsOfStep),                                                            // → (state, events, dropped_segments)
      MAKE_CALLABLE(appendStageEvents),                                                               // → (state, dropped_segments)
      when<0>(isBiteDropFoodMode, over_food(dropSegmentsAsFood)),                                     // → state
      over_food_viewing_snakes(handleFoodEatingOfStep),                                               // → (state, events)
      MAKE_CALLABLE(appendStageEvents),                                                               // → state
      over_scores_viewing_events(scoring_policy::applyScoring),                                       // → state
      over_food_viewing_board_and_snakes(bindFront(replenishFood, random_int_fn, MIN_FOOD_COUNT)),    // → state
      when(shouldRepositionFood,
           over_food_viewing_board_and_snakes(bindFront(repositionRandomFood, random_int_fn))),       // → state
      clearRepositionFlag);                                                                           // → state
  // clang-format on

  return tick_pipeline_fn(std::move(state));
}

}  // namespace classic_arena_authority
}  // namespace snake
