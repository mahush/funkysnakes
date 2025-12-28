#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <utility>

#include "snake/state_with_effect.hpp"

namespace snake {

/**
 * @brief Lift a pure state transformation into the effectful context
 *
 * Helper to convert functions that only modify state (no effects)
 * into the StateWithEffect format with empty effects.
 *
 * @tparam State The state type
 * @tparam Effect The effect type
 * @tparam F Function type: State -> State
 * @param f Pure state transformation
 * @return Effectful transformation that produces empty effects
 */
template <typename State, typename Effect, typename F>
auto with_empty_effect(F f) {
  return [f](State s) -> StateWithEffect<State, Effect> { return {f(s), Effect::empty()}; };
}

/**
 * @brief Lens to operate on direction filters with access to snakes
 *
 * Extracts direction_command_state and snakes from GameState, passes them to
 * an operation function along with forwarded arguments, then updates the filters.
 *
 * Example usage:
 *   state = over_direction_filters_with_snakes(
 *       state,
 *       direction_command_filter::try_add,
 *       direction_cmd
 *   );
 *
 * @tparam Op Function type: (filters, snakes, args...) -> updated_filters
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Updated game state with modified filters
 */
template <typename Op, typename... Args>
GameState over_direction_filters_with_snakes(GameState state, Op op, Args&&... args) {
  state.direction_command_state = op(state.direction_command_state, state.snakes, std::forward<Args>(args)...);
  return state;
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
