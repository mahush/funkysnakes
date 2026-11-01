#include "orbital/orbital_game_domain_application.hpp"

#include <iterator>
#include <vector>

#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {

namespace {

using game_boundary::StepReport;
using game_boundary::SubmissionAccepted;
using game_boundary::SubmissionRejected;
using game_boundary::SubmissionRejectionReason;
using game_boundary::Views;

bool isStarted(const State& state) { return state.game.has_value(); }

// Route each pending order would get from the snake's current position
std::map<PlayerId, std::vector<Direction>> pendingRoutes(const arena_authority::State& arena,
                                                         const order_authority::View& orders) {
  std::map<PlayerId, std::vector<Direction>> routes;
  for (const auto& [player, player_orders] : orders.players) {
    if (!player_orders.pending) {
      continue;
    }
    if (auto route = arena_authority::routeIfActivatedNow(arena, player, player_orders.pending->order.target)) {
      routes[player] = std::move(*route);
    }
  }
  return routes;
}

Observations withViews(Observations observations, const State& state) {
  if (isStarted(state)) {
    order_authority::View orders = order_authority::view(*state.orders);
    std::map<PlayerId, std::vector<Direction>> pending_routes = pendingRoutes(*state.arena, orders);
    observations.views = Views{arena_authority::view(*state.arena),
                               std::move(orders),
                               game_authority::view(*state.game),
                               state.clock.now,
                               latency_policy::delayAt(state.clock.now),
                               std::move(pending_routes)};
  }
  return observations;
}

std::vector<PlayerId> playersOf(const arena_authority::RoundSetup& setup) {
  std::vector<PlayerId> players;
  for (const auto& [player, snake] : setup.snakes) {
    players.push_back(player);
  }
  return players;
}

// Observations produced so far, followed by a later batch
Observations appendObservations(Observations observations, Observations later) {
  observations.steps.insert(observations.steps.end(),
                            std::make_move_iterator(later.steps.begin()),
                            std::make_move_iterator(later.steps.end()));
  observations.end_of_round_events.insert(
      observations.end_of_round_events.end(), later.end_of_round_events.begin(), later.end_of_round_events.end());
  if (later.concluded) {
    observations.concluded = later.concluded;
  }
  return observations;
}

// Carry out the Game Authority's conclusion intent: end execution, then cancel what is still pending
std::tuple<State, Observations> conclude(State state, const game_authority::GameConcluded& concluded) {
  auto [arena, end_events] = arena_authority::endRound(std::move(*state.arena));
  state.arena = std::move(arena);
  state.orders = order_authority::cancel(order_authority::observe(std::move(*state.orders), {}, end_events));

  Observations observations;
  observations.end_of_round_events = std::move(end_events);
  observations.concluded = concluded;
  return {std::move(state), std::move(observations)};
}

// One arena step, with every Authority observing its complete outcome before the next step
std::tuple<State, Observations> runStep(const RandomIntGeneratorFn& random_int_fn, State state) {
  const GameTime at = state.clock.next_step;
  state.clock.next_step += pacing_policy::STEP_INTERVAL;

  std::vector<Order> eligible = order_authority::eligible(*state.orders, at);
  auto [arena, activations, events] = arena_authority::tick(random_int_fn, std::move(*state.arena), eligible);
  state.arena = std::move(arena);
  state.orders = order_authority::observe(std::move(*state.orders), activations, events);
  auto [game, concluded] = game_authority::observeStep(
      std::move(*state.game), at, activations, events, arena_authority::alivePlayers(*state.arena));
  state.game = std::move(game);

  Observations observations;
  observations.steps.push_back(
      StepReport{arena_authority::lastCompletedStep(*state.arena), at, std::move(activations), std::move(events)});
  if (!concluded) {
    return {std::move(state), std::move(observations)};
  }
  auto [concluded_state, conclusion] = conclude(std::move(state), *concluded);
  return {std::move(concluded_state), appendObservations(std::move(observations), std::move(conclusion))};
}

}  // namespace

State initial() { return State{}; }

std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int_fn, State state, const Start& start) {
  return startWithSetup(random_int_fn, std::move(state), arena_authority::makeOrbitalSetup(start.round));
}

std::tuple<State, Observations> startWithSetup(const RandomIntGeneratorFn& random_int_fn,
                                               State state,
                                               const arena_authority::RoundSetup& setup) {
  state.arena = arena_authority::startRound(random_int_fn, setup);
  state.orders = order_authority::startRound(setup.round);
  state.game = game_authority::startRound(setup.round, playersOf(setup));
  state.clock = VirtualClock{};
  Observations observations = withViews({}, state);
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> apply(State state, const Submit& submit) {
  Observations observations;
  if (!isStarted(state) || !game_authority::isRunning(*state.game)) {
    observations.submission = SubmissionRejected{SubmissionRejectionReason::ROUND_OVER};
    return {std::move(state), withViews(std::move(observations), state)};
  }

  auto [orders, committed] = order_authority::submit(
      std::move(*state.orders), order_authority::Submission{submit.player, submit.target, state.clock.now});
  state.orders = std::move(orders);
  state.game = game_authority::observeCommitment(std::move(*state.game), committed);
  observations.submission = SubmissionAccepted{committed.order, committed.committed_at};
  observations.commitment = committed;
  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int_fn,
                                      State state,
                                      const TimeElapsed& elapsed) {
  Observations observations;
  if (!isStarted(state)) {
    return {std::move(state), std::move(observations)};
  }

  const GameTime until = state.clock.now + elapsed.duration;
  while (game_authority::isRunning(*state.game) && state.clock.next_step <= until) {
    state.clock.now = state.clock.next_step;
    auto [next, step_observations] = runStep(random_int_fn, std::move(state));
    state = std::move(next);
    observations = appendObservations(std::move(observations), std::move(step_observations));
  }
  if (game_authority::isRunning(*state.game)) {
    state.clock.now = until;  // Once concluded, the clock stays at the step that ended the game
  }

  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
