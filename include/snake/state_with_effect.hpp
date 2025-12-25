#ifndef SNAKE_STATE_WITH_EFFECT_HPP
#define SNAKE_STATE_WITH_EFFECT_HPP

#include <map>
#include <string>

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Score delta effect - tracks score changes per player
 *
 * Used as an "effect" type to accumulate score changes through
 * the game state update pipeline.
 */
struct ScoreDelta {
  std::map<PlayerId, int> deltas;

  ScoreDelta() = default;

  /**
   * @brief Create a score delta for a single player
   */
  static ScoreDelta forPlayer(const PlayerId& player_id, int delta) {
    ScoreDelta result;
    result.deltas[player_id] = delta;
    return result;
  }

  /**
   * @brief Create an empty (no-op) score delta
   */
  static ScoreDelta empty() { return ScoreDelta{}; }
};

/**
 * @brief Combine two score deltas by summing per-player changes
 *
 * This is the effect combination operation. It merges two score deltas
 * by adding up the score changes for each player.
 *
 * @param a First score delta
 * @param b Second score delta
 * @return Combined score delta
 */
inline ScoreDelta combine(const ScoreDelta& a, const ScoreDelta& b) {
  ScoreDelta result = a;
  for (const auto& [player_id, delta] : b.deltas) {
    result.deltas[player_id] += delta;
  }
  return result;
}

/**
 * @brief Template wrapper carrying state plus accumulated effects
 *
 * This is the core building block of the composable state update framework.
 * Each subsystem operates on state and produces effects, which are automatically
 * combined through the update pipeline.
 *
 * @tparam State The state type (e.g., SnakeState, GameState)
 * @tparam Effect The effect type (e.g., ScoreDelta)
 */
template <typename State, typename Effect>
struct StateWithEffect {
  State state;
  Effect effect;

  /**
   * @brief Create from state with empty effect
   */
  static StateWithEffect fromState(State s) { return {s, Effect::empty()}; }

  /**
   * @brief Create from state with a specific effect
   */
  static StateWithEffect create(State s, Effect e) { return {s, e}; }
};

/**
 * @brief Combinator that chains effectful state updates
 *
 * This is analogous to monadic bind/flatMap but with domain-specific naming.
 * It applies a function to the current state, then combines the resulting
 * effect with the accumulated effect.
 *
 * Usage pattern:
 *   auto result = with_combining_effects(x, f);
 * where f :: State -> StateWithEffect<State, Effect>
 *
 * @tparam State The state type
 * @tparam Effect The effect type
 * @tparam F Function type: State -> StateWithEffect<State, Effect>
 * @param x Current state with accumulated effects
 * @param f Function to apply to the state
 * @return New state with combined effects
 */
template <typename State, typename Effect, typename F>
auto with_combining_effects(StateWithEffect<State, Effect> x, F f) -> StateWithEffect<State, Effect> {
  auto res = f(x.state);
  return StateWithEffect<State, Effect>{res.state, combine(x.effect, res.effect)};
}

// Common type aliases for this domain

/**
 * @brief Snake state with score effect
 */
using SnakeStateWithScoreEffect = StateWithEffect<SnakeState, ScoreDelta>;

/**
 * @brief Game state with score effect
 */
using GameStateAndScoreDelta = StateWithEffect<GameState, ScoreDelta>;

}  // namespace snake

#endif  // SNAKE_STATE_WITH_EFFECT_HPP
