#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "common/game_time.hpp"
#include "common/game_types.hpp"
#include "common/random_source.hpp"
#include "orbital/arena_facts.hpp"
#include "orbital/identities.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace arena_authority {

/**
 * Orbital Arena Authority - owns the actual world of an Orbital Orders round
 *
 * Owns the board, the snakes, the food and the executing order of each snake with its route.
 * It decides whether an eligible order actually activates, and owns the complete sequence of one
 * step: activation, movement along routes (or straight on), food collection, order completion and
 * food replenishment. The nested snake_model owns the body and life of one snake.
 *
 * Out of scope: pending orders and eligibility (Order Authority), points, round duration and
 * conclusion (Round Authority). The Arena reports facts; it does not value them.
 *
 * Within the facts of one step:
 * - interruption of an executing order precedes collection under its replacement;
 * - collection precedes completion of the order that collected;
 * - an order completed at activation (head already at target) precedes collection without order.
 */

// Minimum number of food items kept on the board
constexpr int MIN_FOOD_COUNT = 5;

/**
 * @brief The order a snake currently executes
 */
struct ActiveOrder {
  OrderId id;
  Point target;
  std::vector<Direction> remaining_route;  // Moves still to take, one per step
};

/**
 * @brief A snake at round start
 */
struct SnakeSetup {
  Point head;
  Direction heading;
  int length;
};

/**
 * @brief Configuration of the world at round start
 */
struct RoundSetup {
  RoundId round;
  Board board;
  std::map<PlayerId, SnakeSetup> snakes;
  FoodItems food;  // Food placed at start; topped up to MIN_FOOD_COUNT
};

/**
 * @brief Arena state owned by the Orbital Arena Authority
 *
 * Fields may be read freely. Changes must go through the transitions below only.
 */
struct State {
  RoundId round;
  Board board;
  PerPlayerSnakes snakes;
  FoodItems food;
  std::map<PlayerId, ActiveOrder> active;  // Executing order per snake
  StepId step{0};                          // Last completed step
  bool ended{false};                       // Execution of the round has ended
};

/**
 * @brief Default setup: classic board, start positions and snake length for Player A and Player B
 *
 * @param round Identity of the round
 * @return Setup without initial food (food is spawned at round start)
 */
RoundSetup classicSetup(RoundId round);

/**
 * @brief Start the world of a round
 *
 * @param random_int Random number source for food placement
 * @param setup Configuration of the world
 * @return World without executing orders, food topped up to MIN_FOOD_COUNT
 */
State startRound(const RandomIntGeneratorFn& random_int, const RoundSetup& setup);

/**
 * @brief Advance the world by one step
 *
 * Eligible orders (at most one per player) are evaluated first; a valid one replaces whatever its
 * snake executes. Then all snakes move and the step's consequences are established.
 *
 * Orders of another round, and every order once execution has ended, are rejected with
 * ROUND_ENDED; after the end, a step changes nothing else.
 *
 * Parameter order: bound parameters first (for bindFront), then state.
 *
 * @param random_int Random number source for food placement
 * @param state Current world
 * @param eligible Eligible orders supplied by the Order Authority
 * @return Tuple of (world after the step, activation outcomes, facts of the step in order)
 */
std::tuple<State, std::vector<ActivationOutcome>, ArenaFacts> tick(const RandomIntGeneratorFn& random_int,
                                                                   State state,
                                                                   const std::vector<Order>& eligible);

/**
 * @brief End execution of the round after the Round Authority concluded it
 *
 * Executing orders end with ROUND_ENDED; the world stays as it is. No movement, no collection.
 *
 * @param state Current world
 * @return Tuple of (ended world, OrderEnded facts for orders that were still executing)
 */
std::tuple<State, ArenaFacts> endRound(State state);

/**
 * @brief Projection of the world for presentation
 */
struct View {
  Board board;
  PerPlayerSnakes snakes;
  FoodItems food;
  std::map<PlayerId, ActiveOrder> active;
  StepId step;
  bool ended;
};

/**
 * @brief Project the world
 *
 * @param state Current world
 * @return Snakes, food, executing orders with their remaining routes, step and whether execution ended
 */
View view(const State& state);

}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
