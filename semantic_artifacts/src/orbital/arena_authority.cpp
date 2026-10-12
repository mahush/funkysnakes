#include "orbital/arena_authority.hpp"

#include <algorithm>

#include "arena_authority_mechanics.hpp"
#include "common/game_logic.hpp"
#include "common/snake_model_evolve.hpp"
#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "orbital/pursuit_policy.hpp"

namespace snake {
namespace orbital {
namespace arena_authority {

using funkypipes::bindFront;
using funkypipes::makePipe;

using namespace mechanics;  // NOLINT(google-build-using-namespace): private plumbing of this Authority

namespace {

using ActiveOrders = std::map<PlayerId, ActiveOrder>;

// ============================================================================
// Activation
// ============================================================================

bool isOnBoard(Point p, const Board& board) { return p.x >= 0 && p.x < board.width && p.y >= 0 && p.y < board.height; }

std::optional<RejectionReason> rejectionOf(const State& state, const Order& order) {
  if (rep(state).ended || order.round != rep(state).round) {
    return RejectionReason::ROUND_ENDED;
  }
  auto snake = rep(state).snakes.find(order.player);
  if (snake == rep(state).snakes.end() || !snake_model::alive(snake->second)) {
    return RejectionReason::SNAKE_DEAD;
  }
  if (!isOnBoard(order.target, rep(state).board)) {
    return RejectionReason::TARGET_OFF_BOARD;
  }
  return std::nullopt;
}

// A valid order replaces the executing one; a head already at the target completes at once
std::tuple<State, ActivationOutcome, ArenaEvents> tryActivate(State state, const Order& order) {
  if (std::optional<RejectionReason> reason = rejectionOf(state, order)) {
    return {std::move(state), OrderRejected{order.id, *reason}, {}};
  }

  const Snake& snake = rep(state).snakes.at(order.player);
  std::vector<Direction> route = pursuit_policy::planRoute(
      snake_model::head(snake), snake_model::currentDirection(snake), order.target, rep(state).board);

  ArenaEvents events;
  std::optional<OrderId> replaced;
  auto active = rep(state).active.find(order.player);
  if (active != rep(state).active.end()) {
    replaced = active->second.id;
    events.push_back(OrderEnded{active->second.id, EndReason::INTERRUPTED, order.id});
    rep(state).active.erase(active);
  }
  OrderActivated activated{order.id, replaced, route};

  if (route.empty()) {
    events.push_back(OrderEnded{order.id, EndReason::COMPLETED, std::nullopt});
  } else {
    rep(state).active.emplace(order.player, ActiveOrder{order.id, order.target, std::move(route)});
  }
  return {std::move(state), std::move(activated), std::move(events)};
}

// Eligible orders are tried in the order supplied; each sees the world the previous one left
std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tryActivateEach(State state,
                                                                               const std::vector<Order>& eligible) {
  std::vector<ActivationOutcome> outcomes;
  ArenaEvents events;
  for (const Order& order : eligible) {
    auto [next, outcome, activation_events] = tryActivate(std::move(state), order);
    state = std::move(next);
    outcomes.push_back(std::move(outcome));
    events = appendEvents(std::move(events), activation_events);
  }
  return {std::move(state), std::move(outcomes), std::move(events)};
}

// ============================================================================
// Step stages
// ============================================================================

// Each executing order supplies its next move; snakes without one keep their heading
std::tuple<ActiveOrders, PerPlayerDirection> takeNextMoves(ActiveOrders active) {
  PerPlayerDirection moves;
  for (auto& [player, order] : active) {
    if (!order.remaining_route.empty()) {
      moves[player] = order.remaining_route.front();
      order.remaining_route.erase(order.remaining_route.begin());
    }
  }
  return {std::move(active), std::move(moves)};
}

// Classic collision rules: bites cut or kill, cut and dead segments are dropped as food
std::tuple<PerPlayerSnakes, FoodItems, ArenaEvents> resolveCollisions(PerPlayerSnakes snakes, FoodItems food) {
  auto [new_snakes, collision_events, dropped] = handleCollisions(std::move(snakes));
  return {std::move(new_snakes), dropSegmentsAsFood(std::move(food), dropped), toArenaEvents(collision_events)};
}

// An order of a snake that died ends right after collisions, so it can neither collect nor complete
std::tuple<ActiveOrders, ArenaEvents> endOrdersOfDeadSnakes(ActiveOrders active, const PerPlayerSnakes& snakes) {
  ArenaEvents events;
  for (auto it = active.begin(); it != active.end();) {
    if (!snake_model::alive(snakes.at(it->first))) {
      events.push_back(OrderEnded{it->second.id, EndReason::ELIMINATED, std::nullopt});
      it = active.erase(it);
    } else {
      ++it;
    }
  }
  return {std::move(active), std::move(events)};
}

// Collection is attributed to the order executing during the move
std::tuple<FoodItems, ArenaEvents> collectFood(FoodItems food,
                                               const PerPlayerSnakes& snakes,
                                               const ActiveOrders& active) {
  auto [new_food, eating_events] = handleFoodEating(std::move(food), snakes);
  ArenaEvents events;
  for (const FoodEaten& eaten : eating_events) {
    const PlayerId& player = eaten.player;
    auto executing = active.find(player);
    std::optional<OrderId> during =
        executing != active.end() ? std::optional<OrderId>{executing->second.id} : std::nullopt;
    events.push_back(FoodCollected{player, snake_model::head(snakes.at(player)), during});
  }
  return {std::move(new_food), std::move(events)};
}

// Completion is positional: the head reached the target and the snake is alive after the step
std::tuple<ActiveOrders, ArenaEvents> completeFulfilledOrders(ActiveOrders active, const PerPlayerSnakes& snakes) {
  ArenaEvents events;
  for (auto it = active.begin(); it != active.end();) {
    const Snake& snake = snakes.at(it->first);
    if (snake_model::alive(snake) && snake_model::head(snake) == it->second.target) {
      events.push_back(OrderEnded{it->second.id, EndReason::COMPLETED, std::nullopt});
      it = active.erase(it);
    } else {
      ++it;
    }
  }
  return {std::move(active), std::move(events)};
}

}  // namespace

RoundSetup makeClassicSetup(RoundId round) {
  return RoundSetup{round,
                    Board{},
                    {{PLAYER_A, SnakeSetup{Point{5, 10}, Direction::RIGHT, 7}},
                     {PLAYER_B, SnakeSetup{Point{5, 15}, Direction::RIGHT, 7}}},
                    {}};
}

State startRound(const RandomIntGeneratorFn& random_int_fn, const RoundSetup& setup) {
  State state;
  rep(state).round = setup.round;
  rep(state).board = setup.board;
  for (const auto& [player, snake] : setup.snakes) {
    rep(state).snakes.emplace(player, snake_model::initial(snake.head, snake.heading, snake.length));
  }
  rep(state).food = replenishFood(random_int_fn, MIN_FOOD_COUNT, setup.food, rep(state).board, rep(state).snakes);
  return state;
}

std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tick(const RandomIntGeneratorFn& random_int_fn,
                                                                    State state,
                                                                    const std::vector<Order>& eligible) {
  auto [activated, outcomes, activation_events] = tryActivateEach(std::move(state), eligible);
  if (rep(activated).ended) {
    return {std::move(activated), std::move(outcomes), std::move(activation_events)};
  }

  // clang-format off
  auto step_pipeline_fn = makePipe(
      with_event_accumulation(makePipe(
          over_active(takeNextMoves),                                                                  // → (state, moves)
          over_snakes_viewing_board_and_food(moveSnakes))),                                            // → state
      with_event_accumulation(over_snakes_and_food(resolveCollisions)),                                // → (state, events)
      with_event_accumulation(over_active_viewing_snakes(endOrdersOfDeadSnakes)),                      // → (state, events)
      with_event_accumulation(over_food_viewing_snakes_and_active(collectFood)),                       // → (state, events)
      with_event_accumulation(over_active_viewing_snakes(completeFulfilledOrders)),                    // → (state, events)
      with_event_accumulation(
          over_food_viewing_board_and_snakes(bindFront(replenishFood, random_int_fn, MIN_FOOD_COUNT))),
      with_event_accumulation(advanceStep));
  // clang-format on

  auto [stepped, step_events] = step_pipeline_fn(std::move(activated), activation_events);
  return {std::move(stepped), std::move(outcomes), std::move(step_events)};
}

std::tuple<State, ArenaEvents> endRound(State state) {
  ArenaEvents events;
  for (const auto& [player, order] : rep(state).active) {
    events.push_back(OrderEnded{order.id, EndReason::ROUND_ENDED, std::nullopt});
  }
  rep(state).active.clear();
  rep(state).ended = true;
  return {std::move(state), std::move(events)};
}

bool anySnakeAlive(const State& state) {
  return std::any_of(rep(state).snakes.begin(), rep(state).snakes.end(), [](const auto& entry) {
    return snake_model::alive(entry.second);
  });
}

StepId lastCompletedStep(const State& state) { return rep(state).step; }

View view(const State& state) {
  return View{
      rep(state).board, rep(state).snakes, rep(state).food, rep(state).active, rep(state).step, rep(state).ended};
}

}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
