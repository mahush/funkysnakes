#include "orbital/orbital_game_domain_application.hpp"

#include <vector>

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

Observations withViews(Observations observations, const State& state) {
  if (isStarted(state)) {
    observations.views = Views{
        arena_authority::view(*state.arena), order_authority::view(*state.orders), game_authority::view(*state.game)};
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

// What carrying out a conclusion intent made observable
struct Conclusion {
  ArenaEvents end_of_round_events;
  game_authority::GameConcluded concluded;
};

// Carry out the Game Authority's conclusion intent: end execution, then cancel what is still pending
std::tuple<State, Conclusion> conclude(State state, const game_authority::GameConcluded& concluded) {
  auto [arena, end_events] = arena_authority::endRound(std::move(*state.arena));
  state.arena = std::move(arena);
  state.orders = order_authority::cancel(order_authority::observe(std::move(*state.orders), {}, end_events));
  return {std::move(state), Conclusion{std::move(end_events), concluded}};
}

// One arena step, with every Authority observing its complete outcome before the next step
std::tuple<State, StepReport, std::optional<Conclusion>> runStep(const RandomIntGeneratorFn& random_int_fn,
                                                                 State state) {
  const GameTime at = state.clock.next_step;
  state.clock.next_step += STEP_INTERVAL;

  std::vector<Order> eligible = order_authority::eligible(*state.orders, at);
  auto [arena, activations, events] = arena_authority::tick(random_int_fn, std::move(*state.arena), eligible);
  state.arena = std::move(arena);
  state.orders = order_authority::observe(std::move(*state.orders), activations, events);
  auto [game, concluded] =
      game_authority::observeStep(std::move(*state.game), at, events, arena_authority::anySnakeAlive(*state.arena));
  state.game = std::move(game);

  StepReport report{arena_authority::lastCompletedStep(*state.arena), at, std::move(activations), std::move(events)};
  if (!concluded) {
    return {std::move(state), std::move(report), std::nullopt};
  }
  auto [concluded_state, conclusion] = conclude(std::move(state), *concluded);
  return {std::move(concluded_state), std::move(report), std::move(conclusion)};
}

}  // namespace

State initial() { return State{}; }

std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int_fn, State state, const Start& start) {
  return startWithSetup(random_int_fn, std::move(state), arena_authority::makeClassicSetup(start.round));
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

  auto [orders, order] = order_authority::submit(
      std::move(*state.orders), order_authority::Submission{submit.player, submit.target, state.clock.now});
  state.orders = std::move(orders);
  observations.submission = SubmissionAccepted{order, state.clock.now};
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
    auto [next, report, conclusion] = runStep(random_int_fn, std::move(state));
    state = std::move(next);
    observations.steps.push_back(std::move(report));
    if (conclusion) {
      observations.end_of_round_events = std::move(conclusion->end_of_round_events);
      observations.concluded = conclusion->concluded;
    }
  }
  state.clock.now = until;

  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
