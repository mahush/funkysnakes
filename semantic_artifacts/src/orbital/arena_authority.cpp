#include "orbital/arena_authority.hpp"

#include <algorithm>

#include "arena_authority_mechanics.hpp"
#include "common/game_logic.hpp"
#include "common/snake_model_evolve.hpp"
#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "orbital/pursuit_policy.hpp"
#include "snake/generic_lens.hpp"

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

std::optional<RejectionReason> rejectionOf(
    bool ended, RoundId round, const PerPlayerSnakes& snakes, const Board& board, const Order& order) {
  if (ended || order.round != round) {
    return RejectionReason::ROUND_ENDED;
  }
  auto snake = snakes.find(order.player);
  if (snake == snakes.end() || !snake_model::alive(snake->second)) {
    return RejectionReason::SNAKE_DEAD;
  }
  if (!isOnBoard(order.target, board)) {
    return RejectionReason::TARGET_OFF_BOARD;
  }
  return std::nullopt;
}

// A valid order replaces the executing one; a head already at the target completes at once
std::tuple<ActiveOrders, ActivationOutcome, ArenaEvents> tryActivate(ActiveOrders active,
                                                                     bool ended,
                                                                     RoundId round,
                                                                     const PerPlayerSnakes& snakes,
                                                                     const Board& board,
                                                                     const Order& order) {
  if (std::optional<RejectionReason> reason = rejectionOf(ended, round, snakes, board, order)) {
    return {std::move(active), OrderRejected{order.id, *reason}, {}};
  }

  const Snake& snake = snakes.at(order.player);
  std::vector<Direction> route =
      pursuit_policy::planRoute(snake_model::head(snake), snake_model::currentDirection(snake), order.target, board);

  ArenaEvents events;
  std::optional<OrderId> replaced;
  auto executing = active.find(order.player);
  if (executing != active.end()) {
    replaced = executing->second.id;
    events.push_back(OrderEnded{executing->second.id, EndReason::INTERRUPTED, order.id});
    active.erase(executing);
  }
  OrderActivated activated{order.id, replaced, route};

  if (route.empty()) {
    events.push_back(OrderEnded{order.id, EndReason::COMPLETED, std::nullopt});
  } else {
    active.emplace(order.player, ActiveOrder{order.id, order.target, std::move(route)});
  }
  return {std::move(active), std::move(activated), std::move(events)};
}

// Eligible orders are tried in the order supplied; each sees the executing orders the previous one left
std::tuple<ActiveOrders, std::vector<ActivationOutcome>, ArenaEvents> tryActivateEach(
    ActiveOrders active,
    bool ended,
    RoundId round,
    const PerPlayerSnakes& snakes,
    const Board& board,
    const std::vector<Order>& eligible) {
  std::vector<ActivationOutcome> outcomes;
  ArenaEvents events;
  for (const Order& order : eligible) {
    auto [next, outcome, activation_events] = tryActivate(std::move(active), ended, round, snakes, board, order);
    active = std::move(next);
    outcomes.push_back(std::move(outcome));
    events = appendEvents(std::move(events), activation_events);
  }
  return {std::move(active), std::move(outcomes), std::move(events)};
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

StepId nextStep(StepId step) { return StepId{step.value + 1}; }

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
  state.round_ = setup.round;
  state.board_ = setup.board;
  for (const auto& [player, snake] : setup.snakes) {
    state.snakes_.emplace(player, snake_model::initial(snake.head, snake.heading, snake.length));
  }
  state.food_ = replenishFood(random_int_fn, MIN_FOOD_COUNT, setup.food, state.board_, state.snakes_);
  return state;
}

std::tuple<State, std::vector<ActivationOutcome>, ArenaEvents> tick(const RandomIntGeneratorFn& random_int_fn,
                                                                    State state,
                                                                    const std::vector<Order>& eligible) {
  auto [active, outcomes, activation_events] =
      tryActivateEach(std::move(state.active_), state.ended_, state.round_, state.snakes_, state.board_, eligible);
  state.active_ = std::move(active);
  if (state.ended_) {
    return {std::move(state), std::move(outcomes), std::move(activation_events)};
  }

  // clang-format off
  auto step_pipeline_fn = makePipe(
      with_event_accumulation<State>(makePipe(
          lens(mutate<&State::active_>, read<>, takeNextMoves),                                            // → (state, moves)
          lens(mutate<&State::snakes_>, read<&State::board_, &State::food_>, moveSnakes))),                // → state
      with_event_accumulation<State>(
          lens(mutate<&State::snakes_, &State::food_>, read<>, resolveCollisions)),                        // → (state, events)
      with_event_accumulation<State>(
          lens(mutate<&State::active_>, read<&State::snakes_>, endOrdersOfDeadSnakes)),                    // → (state, events)
      with_event_accumulation<State>(
          lens(mutate<&State::food_>, read<&State::snakes_, &State::active_>, collectFood)),               // → (state, events)
      with_event_accumulation<State>(
          lens(mutate<&State::active_>, read<&State::snakes_>, completeFulfilledOrders)),                  // → (state, events)
      with_event_accumulation<State>(lens(mutate<&State::food_>,
                                          read<&State::board_, &State::snakes_>,
                                          bindFront(replenishFood, random_int_fn, MIN_FOOD_COUNT))),
      with_event_accumulation<State>(lens(mutate<&State::step_>, read<>, nextStep)));
  // clang-format on

  auto [stepped, step_events] = step_pipeline_fn(std::move(state), activation_events);
  return {std::move(stepped), std::move(outcomes), std::move(step_events)};
}

std::tuple<State, ArenaEvents> endRound(State state) {
  ArenaEvents events;
  for (const auto& [player, order] : state.active_) {
    events.push_back(OrderEnded{order.id, EndReason::ROUND_ENDED, std::nullopt});
  }
  state.active_.clear();
  state.ended_ = true;
  return {std::move(state), std::move(events)};
}

bool anySnakeAlive(const State& state) {
  return std::any_of(
      state.snakes_.begin(), state.snakes_.end(), [](const auto& entry) { return snake_model::alive(entry.second); });
}

StepId lastCompletedStep(const State& state) { return state.step_; }

std::optional<std::vector<Direction>> routeIfActivatedNow(const State& state, const PlayerId& player, Point target) {
  auto snake = state.snakes_.find(player);
  if (state.ended_ || snake == state.snakes_.end() || !snake_model::alive(snake->second) ||
      !isOnBoard(target, state.board_)) {
    return std::nullopt;
  }
  return pursuit_policy::planRoute(
      snake_model::head(snake->second), snake_model::currentDirection(snake->second), target, state.board_);
}

View view(const State& state) {
  return View{state.board_, state.snakes_, state.food_, state.active_, state.step_, state.ended_};
}

}  // namespace arena_authority
}  // namespace orbital
}  // namespace snake
