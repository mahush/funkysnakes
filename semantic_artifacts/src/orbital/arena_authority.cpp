#include "orbital/arena_authority.hpp"

#include "common/game_logic.hpp"
#include "common/snake_model_evolve.hpp"
#include "orbital/pursuit_policy.hpp"

namespace snake {
namespace orbital {
namespace arena_authority {

namespace {

struct StepResult {
  State state;
  std::vector<ActivationOutcome> outcomes;
  ArenaEvents events;
};

bool isOnBoard(Point p, const Board& board) { return p.x >= 0 && p.x < board.width && p.y >= 0 && p.y < board.height; }

std::optional<RejectionReason> rejectionOf(const State& state, const Order& order) {
  if (state.ended || order.round != state.round) {
    return RejectionReason::ROUND_ENDED;
  }
  auto snake = state.snakes.find(order.player);
  if (snake == state.snakes.end() || !snake_model::alive(snake->second)) {
    return RejectionReason::SNAKE_DEAD;
  }
  if (!isOnBoard(order.target, state.board)) {
    return RejectionReason::TARGET_OFF_BOARD;
  }
  return std::nullopt;
}

// A valid order replaces the executing one; a head already at the target completes at once
StepResult activate(StepResult result, const Order& order) {
  if (std::optional<RejectionReason> reason = rejectionOf(result.state, order)) {
    result.outcomes.push_back(OrderRejected{order.id, *reason});
    return result;
  }

  const Snake& snake = result.state.snakes.at(order.player);
  std::vector<Direction> route = pursuit_policy::planRoute(
      snake_model::head(snake), snake_model::currentDirection(snake), order.target, result.state.board);

  std::optional<OrderId> replaced;
  auto active = result.state.active.find(order.player);
  if (active != result.state.active.end()) {
    replaced = active->second.id;
    result.events.push_back(OrderEnded{active->second.id, EndReason::INTERRUPTED, order.id});
    result.state.active.erase(active);
  }
  result.outcomes.push_back(OrderActivated{order.id, replaced, route});

  if (route.empty()) {
    result.events.push_back(OrderEnded{order.id, EndReason::COMPLETED, std::nullopt});
  } else {
    result.state.active.emplace(order.player, ActiveOrder{order.id, order.target, std::move(route)});
  }
  return result;
}

// Each executing order supplies its next move; snakes without one keep their heading
PerPlayerDirection takeNextMoves(std::map<PlayerId, ActiveOrder>& active) {
  PerPlayerDirection moves;
  for (auto& [player, order] : active) {
    if (!order.remaining_route.empty()) {
      moves[player] = order.remaining_route.front();
      order.remaining_route.erase(order.remaining_route.begin());
    }
  }
  return moves;
}

// Collision handling reports only collision events; eating is reported by collectFood
void appendCollisionEvent(ArenaEvents& events, const Bitten& event) { events.push_back(event); }
void appendCollisionEvent(ArenaEvents& events, const SelfBitten& event) { events.push_back(event); }
void appendCollisionEvent(ArenaEvents& events, const MutualBite& event) { events.push_back(event); }
void appendCollisionEvent(ArenaEvents& /*events*/, const FoodEaten& /*event*/) {}

// Classic collision rules: bites cut or kill, cut and dead segments are dropped as food.
// An order of a snake that died ends here, before collection, so it can neither collect nor complete.
StepResult resolveCollisions(StepResult result) {
  auto [snakes, collision_events, dropped] = handleCollisions(std::move(result.state.snakes), {});
  result.state.snakes = std::move(snakes);
  for (const snake::ArenaEvent& event : collision_events) {
    std::visit([&result](const auto& item) { appendCollisionEvent(result.events, item); }, event);
  }
  result.state.food = dropSegmentsAsFood(std::move(result.state.food), dropped);

  for (auto it = result.state.active.begin(); it != result.state.active.end();) {
    if (!snake_model::alive(result.state.snakes.at(it->first))) {
      result.events.push_back(OrderEnded{it->second.id, EndReason::ELIMINATED, std::nullopt});
      it = result.state.active.erase(it);
    } else {
      ++it;
    }
  }
  return result;
}

// Collection is attributed to the order executing during the move
StepResult collectFood(StepResult result) {
  auto [food, eating_events] = handleFoodEating(std::move(result.state.food), {}, result.state.snakes);
  result.state.food = std::move(food);
  for (const snake::ArenaEvent& event : eating_events) {
    const PlayerId& player = std::get<FoodEaten>(event).player;
    auto active = result.state.active.find(player);
    std::optional<OrderId> during =
        active != result.state.active.end() ? std::optional<OrderId>{active->second.id} : std::nullopt;
    result.events.push_back(FoodCollected{player, snake_model::head(result.state.snakes.at(player)), during});
  }
  return result;
}

// Completion is positional: the head reached the target and the snake is alive after the step
StepResult completeArrivedOrders(StepResult result) {
  for (auto it = result.state.active.begin(); it != result.state.active.end();) {
    const Snake& snake = result.state.snakes.at(it->first);
    if (snake_model::alive(snake) && snake_model::head(snake) == it->second.target) {
      result.events.push_back(OrderEnded{it->second.id, EndReason::COMPLETED, std::nullopt});
      it = result.state.active.erase(it);
    } else {
      ++it;
    }
  }
  return result;
}

}  // namespace

RoundSetup classicSetup(RoundId round) {
  return RoundSetup{round,
                    Board{},
                    {{PLAYER_A, SnakeSetup{Point{5, 10}, Direction::RIGHT, 7}},
                     {PLAYER_B, SnakeSetup{Point{5, 15}, Direction::RIGHT, 7}}},
                    {}};
}

State startRound(const RandomIntGeneratorFn& random_int, const RoundSetup& setup) {
  State state;
  state.round = setup.round;
  state.board = setup.board;
  for (const auto& [player, snake] : setup.snakes) {
    state.snakes.emplace(player, snake_model::initial(snake.head, snake.heading, snake.length));
  }
  state.food = replenishFood(random_int, MIN_FOOD_COUNT, setup.food, state.board, state.snakes);
  return state;
}

std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tick(const RandomIntGeneratorFn& random_int,
                                                                    State state,
                                                                    const std::vector<Order>& eligible) {
  StepResult result{std::move(state), {}, {}};

  for (const Order& order : eligible) {
    result = activate(std::move(result), order);
  }
  if (result.state.ended) {
    return {std::move(result.state), std::move(result.outcomes), std::move(result.events)};
  }

  PerPlayerDirection moves = takeNextMoves(result.state.active);
  result.state.snakes = moveSnakes(std::move(result.state.snakes), result.state.board, result.state.food, moves);
  result = resolveCollisions(std::move(result));
  result = collectFood(std::move(result));
  result = completeArrivedOrders(std::move(result));
  result.state.food =
      replenishFood(random_int, MIN_FOOD_COUNT, std::move(result.state.food), result.state.board, result.state.snakes);
  ++result.state.step.value;

  return {std::move(result.state), std::move(result.outcomes), std::move(result.events)};
}

std::tuple<State, ArenaEvents> endRound(State state) {
  ArenaEvents events;
  for (const auto& [player, order] : state.active) {
    events.push_back(OrderEnded{order.id, EndReason::ROUND_ENDED, std::nullopt});
  }
  state.active.clear();
  state.ended = true;
  return {std::move(state), std::move(events)};
}

View view(const State& state) {
  return View{state.board, state.snakes, state.food, state.active, state.step, state.ended};
}

}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
