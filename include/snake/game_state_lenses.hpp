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
 * @brief Lens to operate on direction command state with access to snakes
 *
 * Extracts direction_command_state and snakes from GameState, passes them to
 * an operation function along with forwarded arguments, then updates the command state.
 *
 * Example usage:
 *   state = over_direction_command_and_snakes(
 *       state,
 *       direction_command_filter::try_add,
 *       direction_cmd
 *   );
 *
 * @tparam Op Function type: (command_state, snakes, args...) -> updated_command_state
 * @tparam Args Additional argument types to forward
 * @param state Current game state
 * @param op Operation to apply
 * @param args Additional arguments to forward to op
 * @return Updated game state with modified command state
 */
template <typename Op, typename... Args>
GameState over_direction_command_and_snakes(GameState state, Op op, Args&&... args) {
  state.direction_command = op(state.direction_command, state.snakes, std::forward<Args>(args)...);
  return state;
}

/**
 * @brief Lens for consuming directions from command queues
 *
 * Applies a consume operation to direction command state and returns both the updated
 * state and the consumed directions.
 *
 * Example usage:
 *   auto [new_state, consumed] = over_direction_command_consuming(
 *       state,
 *       direction_command_filter::try_consume_next
 *   );
 *
 * @tparam Op Function type: (command_state) -> ConsumeResult
 * @param state Current game state
 * @param op Consume operation to apply
 * @return Pair of (updated state, consumed directions map)
 */
template <typename Op>
std::pair<GameState, std::map<PlayerId, Direction>> over_direction_command_consuming(GameState state, Op op) {
  auto result = op(state.direction_command);
  state.direction_command = result.filters;
  return {state, result.consumed_directions};
}

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
