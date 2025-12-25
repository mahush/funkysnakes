#ifndef SNAKE_GAME_STATE_LENSES_HPP
#define SNAKE_GAME_STATE_LENSES_HPP

#include <functional>
#include <vector>

#include "snake/state_with_effect.hpp"

namespace snake {

/**
 * @brief Apply a function to a specific snake by index
 *
 * This lens focuses on a single snake within the game state, applies
 * an effectful transformation, and properly combines the effects.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param snake_index Index of the snake to transform
 * @param f Function to apply to the snake
 * @return Updated game state with combined effects
 */
template <typename F>
GameStateAndScoreDelta over_snake_with_effects(GameStateAndScoreDelta x, size_t snake_index, F f) {
  if (snake_index >= x.state.snakes.size()) {
    return x;  // Index out of bounds, return unchanged
  }

  auto res = f(x.state.snakes[snake_index]);
  x.state.snakes[snake_index] = res.state;
  x.effect = combine(x.effect, res.effect);
  return x;
}

/**
 * @brief Apply a function to all snakes in the game state
 *
 * This lens maps an effectful transformation over all snakes,
 * collecting and combining all effects.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each snake
 * @return Updated game state with combined effects from all snakes
 */
template <typename F>
GameStateAndScoreDelta over_all_snakes_with_effects(GameStateAndScoreDelta x, F f) {
  for (size_t i = 0; i < x.state.snakes.size(); ++i) {
    auto res = f(x.state.snakes[i]);
    x.state.snakes[i] = res.state;
    x.effect = combine(x.effect, res.effect);
  }
  return x;
}

/**
 * @brief Apply a function to only alive snakes
 *
 * This lens filters to alive snakes before applying the transformation.
 *
 * @tparam F Function type: SnakeState -> SnakeStateWithScoreEffect
 * @param x Current game state with accumulated effects
 * @param f Function to apply to each alive snake
 * @return Updated game state with combined effects from all alive snakes
 */
template <typename F>
GameStateAndScoreDelta over_alive_snakes_with_combining_scores(GameStateAndScoreDelta x, F f) {
  for (size_t i = 0; i < x.state.snakes.size(); ++i) {
    if (x.state.snakes[i].alive) {
      auto res = f(x.state.snakes[i]);
      x.state.snakes[i] = res.state;
      x.effect = combine(x.effect, res.effect);
    }
  }
  return x;
}

/**
 * @brief Apply a function to a pair of snakes
 *
 * This lens focuses on two specific snakes for operations like
 * collision detection that need to consider snake interactions.
 *
 * The function takes two snake states and returns both updated states
 * plus combined effects.
 *
 * @tparam F Function type: (SnakeState, SnakeState) -> (SnakeState, SnakeState, ScoreDelta)
 * @param x Current game state with accumulated effects
 * @param index_a Index of first snake
 * @param index_b Index of second snake
 * @param f Function to apply to the snake pair
 * @return Updated game state with combined effects
 */
template <typename F>
GameStateAndScoreDelta over_snake_pair_with_effects(GameStateAndScoreDelta x, size_t index_a, size_t index_b, F f) {
  if (index_a >= x.state.snakes.size() || index_b >= x.state.snakes.size() || index_a == index_b) {
    return x;  // Invalid indices, return unchanged
  }

  auto [new_a, new_b, effect] = f(x.state.snakes[index_a], x.state.snakes[index_b]);
  x.state.snakes[index_a] = new_a;
  x.state.snakes[index_b] = new_b;
  x.effect = combine(x.effect, effect);
  return x;
}

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
