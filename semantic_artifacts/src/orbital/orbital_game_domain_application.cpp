#include "orbital/orbital_game_domain_application.hpp"

#include <stdexcept>
#include <tuple>
#include <variant>
#include <vector>

#include "orbital/admission_policy.hpp"
#include "orbital/board_setup_policy.hpp"
#include "orbital/transmission_schedule_policy.hpp"
#include "orbital_game_domain_application_mechanics.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {

namespace {

using admission_policy::GamePhase;
using game_boundary::StartRejected;
using game_boundary::StepReport;
using game_boundary::SubmissionAccepted;
using game_boundary::SubmissionRejected;
using game_boundary::Views;
using mechanics::appendObservations;
using mechanics::Observations;

bool isStarted(const State& state) { return state.game.has_value(); }

// Phase of the game, read from whether one started and from the Game Authority
GamePhase phaseOf(const State& state) {
  if (!isStarted(state)) {
    return GamePhase::NOT_STARTED;
  }
  return game_authority::isRunning(*state.game) ? GamePhase::RUNNING : GamePhase::CONCLUDED;
}

// Route each order of one kind would get from the snake's current position
std::map<PlayerId, std::vector<Direction>> routesOf(
    const arena_authority::State& arena,
    const order_authority::View& orders,
    std::optional<order_authority::OrderRecord> order_authority::PlayerOrders::* order) {
  std::map<PlayerId, std::vector<Direction>> routes;
  for (const auto& [player, player_orders] : orders.players) {
    const std::optional<order_authority::OrderRecord>& record = player_orders.*order;
    if (!record) {
      continue;
    }
    if (auto route = arena_authority::routeIfActivatedNow(arena, player, record->order.target)) {
      routes[player] = std::move(*route);
    }
  }
  return routes;
}

// Every transmission opportunity of the game, as the Transmission Schedule Policy decides them
std::vector<GameTime> transmissionSchedule() {
  std::vector<GameTime> schedule;
  for (GameTime opportunity = transmission_schedule_policy::FIRST_OPPORTUNITY;
       opportunity <= pacing_policy::GAME_DURATION;
       opportunity += transmission_schedule_policy::intervalAfter(opportunity)) {
    schedule.push_back(opportunity);
  }
  return schedule;
}

Observations withViews(Observations observations, const State& state) {
  if (isStarted(state)) {
    order_authority::View orders = order_authority::view(*state.orders);
    std::map<PlayerId, std::vector<Direction>> selected_routes =
        routesOf(*state.arena, orders, &order_authority::PlayerOrders::selected);
    std::map<PlayerId, std::vector<Direction>> transmitted_routes =
        routesOf(*state.arena, orders, &order_authority::PlayerOrders::transmitted);
    observations.views = Views{arena_authority::view(*state.arena),
                               std::move(orders),
                               game_authority::view(*state.game),
                               state.clock.now,
                               pacing_policy::STEP_INTERVAL,
                               transmissionSchedule(),
                               std::move(selected_routes),
                               std::move(transmitted_routes)};
  }
  return observations;
}

std::vector<PlayerId> playersOf(const arena_authority::RoundSetup& setup) {
  std::vector<PlayerId> players;
  for (const auto& [player, snake] : setup.layout.snakes) {
    players.push_back(player);
  }
  return players;
}

// Carry out the Game Authority's conclusion intent: end execution, then cancel what is still selected or transmitted
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

  std::vector<Order> released = order_authority::transmittedOrders(*state.orders);
  auto [arena, activations, events] = arena_authority::tick(random_int_fn, std::move(*state.arena), released);
  state.arena = std::move(arena);
  state.orders = order_authority::observe(std::move(*state.orders), activations, events);
  auto [game, concluded] = game_authority::observeStep(
      std::move(*state.game), at, activations, events, arena_authority::alivePlayers(*state.arena));
  state.game = std::move(game);

  Observations observations;
  observations.steps.push_back(
      StepReport{arena_authority::lastCompletedStep(*state.arena), at, std::move(activations), std::move(events), {}});
  if (concluded) {
    auto [concluded_state, conclusion] = conclude(std::move(state), *concluded);
    return {std::move(concluded_state), appendObservations(std::move(observations), std::move(conclusion))};
  }
  if (transmission_schedule_policy::isOpportunity(at)) {
    auto [orders, transmissions] = order_authority::transmit(std::move(*state.orders), at);
    state.orders = std::move(orders);
    for (const OrderTransmitted& transmitted : transmissions) {
      state.game = game_authority::observeTransmission(std::move(*state.game), transmitted);
    }
    observations.steps.back().transmissions = std::move(transmissions);
  }
  return {std::move(state), std::move(observations)};
}

// Begin the game with a given world, as far as the Admission Policy admits a start
std::tuple<State, Observations> startGame(const RandomIntGeneratorFn& random_int_fn,
                                          State state,
                                          const arena_authority::RoundSetup& setup) {
  if (std::optional<game_boundary::StartRejectionReason> reason = admission_policy::startRejectionIn(phaseOf(state))) {
    Observations observations;
    observations.start_rejected = StartRejected{*reason};
    observations = withViews(std::move(observations), state);
    return {std::move(state), std::move(observations)};
  }

  state.arena = arena_authority::startRound(random_int_fn, setup);
  state.orders = order_authority::startRound(setup.round);
  state.game = game_authority::startRound(setup.round, playersOf(setup));
  state.clock = VirtualClock{};
  Observations observations = withViews({}, state);
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int_fn, State state, const Start& start) {
  return startGame(
      random_int_fn, std::move(state), arena_authority::RoundSetup{start.round, board_setup_policy::startLayout(), {}});
}

std::tuple<State, Observations> apply(State state, const Submit& submit) {
  Observations observations;
  if (std::optional<game_boundary::SubmissionRejectionReason> reason =
          admission_policy::submissionRejectionIn(phaseOf(state))) {
    observations.submission = SubmissionRejected{*reason};
    observations = withViews(std::move(observations), state);
    return {std::move(state), std::move(observations)};
  }

  auto [orders, submitted] = order_authority::submit(
      std::move(*state.orders), order_authority::Submission{submit.player, submit.target, state.clock.now});
  state.orders = std::move(orders);
  observations.submission = SubmissionAccepted{submitted.order, submitted.submitted_at};
  observations.submitted = submitted;
  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

// Elapsed time is not negative here; DomainApplication::elapse refuses negative time
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
    state.clock.now = until;
  }

  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

// The transition each kind of input takes
std::tuple<State, Observations> takeEffect(const RandomIntGeneratorFn& random_int_fn, State state, const Start& start) {
  return apply(random_int_fn, std::move(state), start);
}

std::tuple<State, Observations> takeEffect(const RandomIntGeneratorFn& /* random_int_fn */,
                                           State state,
                                           const Submit& submit) {
  return apply(std::move(state), submit);
}

std::tuple<State, Observations> takeEffect(const RandomIntGeneratorFn& random_int_fn,
                                           State state,
                                           const TimeElapsed& elapsed) {
  return apply(random_int_fn, std::move(state), elapsed);
}

std::tuple<State, Observations> takeEffect(const RandomIntGeneratorFn& random_int_fn,
                                           State state,
                                           const arena_authority::RoundSetup& setup) {
  return startGame(random_int_fn, std::move(state), setup);
}

}  // namespace

DomainApplication::DomainApplication(game_boundary::PlayersEndOutbound& players, RandomIntGeneratorFn random_int_fn)
    : ExecutionInterface{std::move(random_int_fn)}, players_{players} {}

void DomainApplication::start(const Start& start) { take(start); }

void DomainApplication::submit(const Submit& submit) { take(submit); }

void DomainApplication::elapse(const TimeElapsed& elapsed) {
  if (elapsed.duration < std::chrono::milliseconds{0}) {
    throw std::invalid_argument{"elapsed time must not be negative"};
  }
  take(elapsed);
}

void DomainApplication::take(Input input) {
  waiting_.push_back(std::move(input));
  if (reporting_) {
    return;  // A callback of the report under way called; the input waits for its turn
  }

  reporting_ = true;
  try {
    while (!waiting_.empty()) {
      const Input next = std::move(waiting_.front());
      waiting_.pop_front();
      // The transition gets a copy, so an owner that throws leaves the game as it was
      auto [state, observations] =
          std::visit([this](const auto& item) { return takeEffect(randomIntFn(), state_, item); }, next);
      state_ = std::move(state);
      mechanics::deliver(observations, players_);
    }
  } catch (...) {
    waiting_.clear();
    reporting_ = false;
    throw;
  }
  reporting_ = false;
}

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
