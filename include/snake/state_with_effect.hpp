#ifndef SNAKE_STATE_WITH_EFFECT_HPP
#define SNAKE_STATE_WITH_EFFECT_HPP

#include <map>
#include <optional>
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
   * @brief Add a score delta for a player (mutating)
   */
  void addForPlayer(const PlayerId& player_id, int delta) { deltas[player_id] += delta; }

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
   * @brief Add a single food item (mutating)
   */
  void add(Point location) { additions.push_back(location); }

  /**
   * @brief Add multiple food items (mutating)
   */
  void add(const std::vector<Point>& locations) {
    additions.insert(additions.end(), locations.begin(), locations.end());
  }

  /**
   * @brief Remove a single food item (mutating)
   */
  void remove(Point location) { removals.push_back(location); }

  /**
   * @brief Remove multiple food items (mutating)
   */
  void remove(const std::vector<Point>& locations) {
    removals.insert(removals.end(), locations.begin(), locations.end());
  }

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
 * @brief Snake operation type
 *
 * Operations that can be applied to snakes. Operations compose naturally
 * and are applied in sequence.
 */
enum class SnakeOp {
  MOVE,         // Move head in current direction, remove last tail segment
  GROW,         // Move head in current direction, keep last tail segment
  CUT_TAIL_AT,  // Remove tail from point onwards
  KILL,         // Set alive = false
  LEFT,         // Set direction to LEFT
  RIGHT,        // Set direction to RIGHT
  UP,           // Set direction to UP
  DOWN          // Set direction to DOWN
};

/**
 * @brief Snake operation with optional parameter
 */
struct SnakeOperation {
  SnakeOp op;
  std::optional<Point> point;  // For CUT_TAIL_AT

  // Factory methods
  static SnakeOperation move() { return {SnakeOp::MOVE, std::nullopt}; }
  static SnakeOperation grow() { return {SnakeOp::GROW, std::nullopt}; }
  static SnakeOperation cutTailAt(Point p) { return {SnakeOp::CUT_TAIL_AT, p}; }
  static SnakeOperation kill() { return {SnakeOp::KILL, std::nullopt}; }
  static SnakeOperation left() { return {SnakeOp::LEFT, std::nullopt}; }
  static SnakeOperation right() { return {SnakeOp::RIGHT, std::nullopt}; }
  static SnakeOperation up() { return {SnakeOp::UP, std::nullopt}; }
  static SnakeOperation down() { return {SnakeOp::DOWN, std::nullopt}; }
};

/**
 * @brief Snake update effect - tracks operations to apply to snakes
 *
 * Used as an "effect" type to accumulate snake operations through
 * the game state update pipeline. Operations are applied in sequence
 * and compose naturally.
 */
struct SnakeUpdateEffect {
  std::map<PlayerId, std::vector<SnakeOperation>> operations;

  SnakeUpdateEffect() = default;

  /**
   * @brief Add an operation for a specific player (mutating)
   */
  void addOp(const PlayerId& player_id, SnakeOperation op) { operations[player_id].push_back(op); }

  /**
   * @brief Create an empty (no-op) update effect
   */
  static SnakeUpdateEffect empty() { return SnakeUpdateEffect{}; }
};

/**
 * @brief Combine two snake update effects
 *
 * Concatenates operation lists for each player. Operations are applied
 * in the order they were added (FIFO).
 *
 * @param a First update effect
 * @param b Second update effect
 * @return Combined update effect with concatenated operations
 */
inline SnakeUpdateEffect combine(const SnakeUpdateEffect& a, const SnakeUpdateEffect& b) {
  SnakeUpdateEffect result = a;
  for (const auto& [player_id, ops] : b.operations) {
    result.operations[player_id].insert(result.operations[player_id].end(), ops.begin(), ops.end());
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
  static GameEffect empty() { return {ScoreDelta::empty(), FoodEffect::empty(), SnakeUpdateEffect::empty()}; }

  /**
   * @brief Create effect with only score delta
   */
  static GameEffect withScores(ScoreDelta scores) { return {scores, FoodEffect::empty(), SnakeUpdateEffect::empty()}; }

  /**
   * @brief Create effect with only food changes
   */
  static GameEffect withFood(FoodEffect food) { return {ScoreDelta::empty(), food, SnakeUpdateEffect::empty()}; }

  /**
   * @brief Create effect with only snake updates
   */
  static GameEffect withSnakes(SnakeUpdateEffect snakes) { return {ScoreDelta::empty(), FoodEffect::empty(), snakes}; }
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
 * @tparam TState The state type (e.g., Snake, GameState)
 * @tparam TEffect The effect type (e.g., ScoreDelta)
 */
template <typename TState, typename TEffect>
struct StateWithEffect {
  const TState state;
  TEffect effect;

  /**
   * @brief Create from state with empty effect
   */
  static StateWithEffect fromState(TState s) { return {s, TEffect::empty()}; }

  /**
   * @brief Create from state with a specific effect
   */
  static StateWithEffect create(TState s, TEffect e) { return {s, e}; }
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
 * where f :: TState -> StateWithEffect<TState, TEffect>
 *
 * @tparam TState The state type
 * @tparam TEffect The effect type
 * @tparam TF Function type: TState -> StateWithEffect<TState, TEffect>
 * @param x Current state with accumulated effects
 * @param f Function to apply to the state
 * @return New state with combined effects
 */
template <typename TState, typename TEffect, typename TF>
auto with_combining_effects(StateWithEffect<TState, TEffect> x, TF f) -> StateWithEffect<TState, TEffect> {
  auto res = f(x.state);
  return StateWithEffect<TState, TEffect>{res.state, combine(x.effect, res.effect)};
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
