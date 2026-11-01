#pragma once

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "common/game_time.hpp"
#include "common/game_types.hpp"
#include "common/random_source.hpp"
#include "orbital/arena_events.hpp"
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
 * step: activation, movement along routes (or straight on), collisions with dropped segments,
 * food collection, order completion and food replenishment. Growth and collision rules are classic's. The nested
 * snake_model owns the body and life of one snake.
 *
 * Out of scope: pending orders and eligibility (Order Authority), points, round duration and
 * conclusion (Game Authority). The Arena reports events; it does not value them.
 *
 * Within the events of one step:
 * - interruption of an executing order precedes collection under its replacement;
 * - collisions precede collection; an order whose snake died ends ELIMINATED right after them,
 *   so a lethal arrival at the target does not complete the order;
 * - collection precedes completion of the order that collected;
 * - an order completed at activation (head already at target) precedes collection without order.
 */

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
  FoodItems food;  // Food placed at start; topped up to the Food Policy's minimum
};

struct View;

/**
 * @brief Arena state owned by the Orbital Arena Authority
 *
 * A value that callers store, pass and replace with the results of the transitions below. Its
 * fields are private: the world is read through the queries and evolves only through the
 * transitions.
 */
class State {
 private:
  State(RoundId round, Board board) : round_{round}, board_{board} {}

  RoundId round_;
  Board board_;
  PerPlayerSnakes snakes_;
  FoodItems food_;
  std::map<PlayerId, ActiveOrder> active_;  // Executing order per snake
  StepId step_{0};                          // Last completed step
  bool ended_{false};                       // Execution of the round has ended

  // Construction
  friend State startRound(const RandomIntGeneratorFn& random_int_fn, const RoundSetup& setup);

  // Transitions
  friend std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tick(const RandomIntGeneratorFn& random_int_fn,
                                                                             State state,
                                                                             const std::vector<Order>& eligible);
  friend std::tuple<State, ArenaEvents> endRound(State state);

  // Queries
  friend bool anySnakeAlive(const State& state);
  friend std::set<PlayerId> alivePlayers(const State& state);
  friend StepId lastCompletedStep(const State& state);
  friend std::optional<std::vector<Direction>> routeIfActivatedNow(const State& state,
                                                                   const PlayerId& player,
                                                                   Point target);
  friend View view(const State& state);
};

// ============================================================================
// Construction: establish a valid initial state from explicit setup
// ============================================================================

/**
 * @brief Setup the Board Setup Policy decides: its board, and Player A and Player B at their starts
 *
 * @param round Identity of the round
 * @return Setup without initial food (food is spawned at round start)
 */
RoundSetup makeOrbitalSetup(RoundId round);

/**
 * @brief Start the world of a round
 *
 * @param random_int_fn Random number source for food placement
 * @param setup Configuration of the world
 * @return World without executing orders, food topped up to the Food Policy's minimum
 */
State startRound(const RandomIntGeneratorFn& random_int_fn, const RoundSetup& setup);

// ============================================================================
// Transitions: evolve the state; return it with outputs it does not encode itself
// ============================================================================

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
 * @param random_int_fn Random number source for food placement
 * @param state Current world
 * @param eligible Orders to try to activate, at most one per player, tried in the given order
 * @return Tuple of (world after the step, activation outcomes, events of the step in order)
 */
std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tick(const RandomIntGeneratorFn& random_int_fn,
                                                                    State state,
                                                                    const std::vector<Order>& eligible);

/**
 * @brief End execution of the round
 *
 * Executing orders end with ROUND_ENDED; the world stays as it is. No movement, no collection.
 *
 * @param state Current world
 * @return Tuple of (ended world, OrderEnded events for orders that were still executing)
 */
std::tuple<State, ArenaEvents> endRound(State state);

// ============================================================================
// Queries: read the state without evolving it
// ============================================================================

/**
 * @brief Whether at least one snake is still alive
 *
 * @param state Current world
 * @return True if a snake is alive
 */
bool anySnakeAlive(const State& state);

/**
 * @brief Players whose snake is still alive
 *
 * @param state Current world
 * @return Players with a living snake
 */
std::set<PlayerId> alivePlayers(const State& state);

/**
 * @brief The last step the world completed
 *
 * @param state Current world
 * @return Identity of the last completed step (0 before the first step)
 */
StepId lastCompletedStep(const State& state);

/**
 * @brief Route an order of the player to the target would get if it activated now
 *
 * Activation happens at the start of a step, before movement, so this is the route the next step
 * would plan from the snake's current head and heading.
 *
 * @param state Current world
 * @param player Player whose snake would execute the order
 * @param target Order target
 * @return Moves of the route; empty if the head is at the target; nothing if the player has no
 *         living snake, the target is off the board or execution has ended
 */
std::optional<std::vector<Direction>> routeIfActivatedNow(const State& state, const PlayerId& player, Point target);

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
