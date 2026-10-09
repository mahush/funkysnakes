#include "snake/classic_arena_authority.hpp"

#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/functional_utils.hpp"
#include "snake/game_logic.hpp"
#include "snake/game_state_lenses.hpp"

namespace snake {
namespace classic_arena_authority {

using funkypipes::bindFront;
using funkypipes::makePipe;

namespace {

bool isBiteDropFoodMode(const GameState& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

bool shouldRepositionFood(const GameState& state) { return state.should_reposition_food; }

GameState clearRepositionFlag(GameState state) {
  state.should_reposition_food = false;
  return state;
}

}  // namespace

GameState initial(const RandomIntGeneratorFn& random_int, GameState state) {
  auto setup =
      makePipe(over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_A}, Point{5, 10}, Direction::RIGHT, 7)),
               over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_B}, Point{5, 15}, Direction::RIGHT, 7)),
               over_food_viewing_board_and_snakes(bindFront(initializeFood, random_int, MIN_FOOD_COUNT)));
  return setup(std::move(state));
}

GameState steer(GameState state, const DirectionCommand& cmd) {
  return over_direction_command_filter_state_viewing_snakes(direction_command_filter::try_add)(std::move(state), cmd);
}

GameState requestFoodReposition(GameState state) {
  state.should_reposition_food = true;
  return state;
}

GameState tick(const RandomIntGeneratorFn& random_int, GameState state) {
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args

  // clang-format off
  auto tick_pipeline = makePipe(
      over_direction_command_filter_state(direction_command_filter::try_consume_next),                // → (state, next_directions)
      over_snakes_viewing_board_and_food(moveSnakes),                                                 // → state
      over_snakes_and_scores(handleCollisions),                                                       // → (state, cut_tails)
      when<0>(isBiteDropFoodMode, over_food(dropCutTailsAsFood)),                                     // → state
      when(isBiteDropFoodMode, over_food_viewing_snakes(dropDeadSnakesAsFood)),                       // → state
      over_food_and_scores_viewing_snakes(handleFoodEating),                                          // → state
      over_food_viewing_board_and_snakes(bindFront(replenishFood, random_int, MIN_FOOD_COUNT)),       // → state
      when(shouldRepositionFood,
           over_food_viewing_board_and_snakes(bindFront(repositionRandomFood, random_int))),          // → state
      clearRepositionFlag);                                                                           // → state
  // clang-format on

  return tick_pipeline(std::move(state));
}

}  // namespace classic_arena_authority
}  // namespace snake
