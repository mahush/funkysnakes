#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>

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

}  // namespace snake

#endif  // SNAKE_GAME_STATE_LENSES_HPP
