#ifndef SNAKE_STATE_WITH_EFFECT_HPP
#define SNAKE_STATE_WITH_EFFECT_HPP

#include <map>
#include <string>
#include <vector>

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
 * @brief Food effect - tracks food additions and removals
 *
 * Used as an "effect" type to accumulate food changes through
 * the game state update pipeline. Supports adding new food items
 * (e.g., from random spawn, dropped tails) and removing food items
 * (e.g., when eaten by snakes).
 */
struct FoodEffect {
  std::vector<Point> additions;  // Food items to add
  std::vector<Point> removals;   // Food items to remove

  FoodEffect() = default;

  /**
   * @brief Create effect that adds a single food item
   */
  static FoodEffect add(Point location) { return {{location}, {}}; }

  /**
   * @brief Create effect that adds multiple food items
   */
  static FoodEffect add(std::vector<Point> locations) { return {locations, {}}; }

  /**
   * @brief Create effect that removes a single food item
   */
  static FoodEffect remove(Point location) { return {{}, {location}}; }

  /**
   * @brief Create effect that removes multiple food items
   */
  static FoodEffect remove(std::vector<Point> locations) { return {{}, locations}; }

  /**
   * @brief Create an empty (no-op) food effect
   */
  static FoodEffect empty() { return {{}, {}}; }
};

/**
 * @brief Combine two food effects by merging additions and removals
 *
 * Merges all additions and removals from both effects.
 *
 * @param a First food effect
 * @param b Second food effect
 * @return Combined food effect
 */
inline FoodEffect combine(const FoodEffect& a, const FoodEffect& b) {
  FoodEffect result;
  result.additions.insert(result.additions.end(), a.additions.begin(), a.additions.end());
  result.additions.insert(result.additions.end(), b.additions.begin(), b.additions.end());
  result.removals.insert(result.removals.end(), a.removals.begin(), a.removals.end());
  result.removals.insert(result.removals.end(), b.removals.begin(), b.removals.end());
  return result;
}

/**
 * @brief Snake update effect - tracks complete snake state updates per player
 *
 * Used as an "effect" type to accumulate snake state changes through
 * the game state update pipeline. Each entry represents the final
 * desired state for a snake (position, direction, alive status, etc.).
 */
struct SnakeUpdateEffect {
  std::map<PlayerId, Snake> snake_updates;

  SnakeUpdateEffect() = default;

  /**
   * @brief Create effect that updates a single snake
   */
  static SnakeUpdateEffect updateSnake(const PlayerId& player_id, Snake snake) {
    SnakeUpdateEffect result;
    result.snake_updates[player_id] = snake;
    return result;
  }

  /**
   * @brief Create an empty (no-op) update effect
   */
  static SnakeUpdateEffect empty() { return SnakeUpdateEffect{}; }
};

/**
 * @brief Combine two snake update effects
 *
 * When multiple effects update the same snake, later updates overwrite
 * earlier ones (last write wins).
 *
 * @param a First update effect
 * @param b Second update effect
 * @return Combined update effect
 */
inline SnakeUpdateEffect combine(const SnakeUpdateEffect& a, const SnakeUpdateEffect& b) {
  SnakeUpdateEffect result = a;
  for (const auto& [player_id, snake] : b.snake_updates) {
    result.snake_updates[player_id] = snake;  // Last write wins
  }
  return result;
}

/**
 * @brief Composite game effect containing all effect types
 *
 * Combines multiple orthogonal effect types (scores, food, snake updates) into a single
 * composite effect that can be accumulated through the update pipeline.
 */
struct GameEffect {
  ScoreDelta scores;
  FoodEffect food;
  SnakeUpdateEffect snakes;

  /**
   * @brief Create an empty (no-op) composite effect
   */
  static GameEffect empty() {
    return {ScoreDelta::empty(), FoodEffect::empty(), SnakeUpdateEffect::empty()};
  }

  /**
   * @brief Create effect with only score delta
   */
  static GameEffect withScores(ScoreDelta scores) {
    return {scores, FoodEffect::empty(), SnakeUpdateEffect::empty()};
  }

  /**
   * @brief Create effect with only food changes
   */
  static GameEffect withFood(FoodEffect food) {
    return {ScoreDelta::empty(), food, SnakeUpdateEffect::empty()};
  }

  /**
   * @brief Create effect with only snake updates
   */
  static GameEffect withSnakes(SnakeUpdateEffect snakes) {
    return {ScoreDelta::empty(), FoodEffect::empty(), snakes};
  }
};

/**
 * @brief Combine two composite game effects
 *
 * Combines all sub-effects orthogonally.
 *
 * @param a First game effect
 * @param b Second game effect
 * @return Combined game effect
 */
inline GameEffect combine(const GameEffect& a, const GameEffect& b) {
  return {combine(a.scores, b.scores), combine(a.food, b.food), combine(a.snakes, b.snakes)};
}

/**
 * @brief Template wrapper carrying state plus accumulated effects
 *
 * This is the core building block of the composable state update framework.
 * Each subsystem operates on state and produces effects, which are automatically
 * combined through the update pipeline.
 *
 * @tparam State The state type (e.g., Snake, GameState)
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
 * @brief Snake with score effect (for snake-specific operations)
 */
using SnakeWithScoreEffect = StateWithEffect<Snake, ScoreDelta>;

/**
 * @brief Game state with composite game effect
 */
using GameStateWithEffect = StateWithEffect<GameState, GameEffect>;

// Legacy alias for backwards compatibility during transition
using GameStateAndScoreDelta = StateWithEffect<GameState, ScoreDelta>;

}  // namespace snake

#endif  // SNAKE_STATE_WITH_EFFECT_HPP
