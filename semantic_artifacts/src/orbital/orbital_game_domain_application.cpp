#include "orbital/orbital_game_domain_application.hpp"

#include <algorithm>
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

// Route an order to each player's target would get from the snake's current position
std::map<PlayerId, std::vector<Direction>> routesTo(const arena_authority::State& arena,
                                                    const std::map<PlayerId, Point>& targets) {
  std::map<PlayerId, std::vector<Direction>> routes;
  for (const auto& [player, target] : targets) {
    if (auto route = arena_authority::routeIfActivatedNow(arena, player, target)) {
      routes[player] = std::move(*route);
    }
  }
  return routes;
}

// Split the transmitted orders into those taking part in the step, as the Transmission Schedule Policy answers, and
// those waiting for a later step
std::tuple<std::vector<Order>, std::vector<OrderTransmitted>> releasedOrders(std::vector<OrderTransmitted> transmitted,
                                                                             StepId step) {
  std::vector<Order> released;
  std::vector<OrderTransmitted> waiting;
  for (OrderTransmitted& sent : transmitted) {
    if (transmission_schedule_policy::takesPartIn(sent.transmitted_at, step)) {
      released.push_back(mechanics::orderOf(sent));
    } else {
      waiting.push_back(std::move(sent));
    }
  }
  return {std::move(released), std::move(waiting)};
}

// Step in which each executing order is expected to complete, as the Arena Authority expects it
std::map<PlayerId, StepId> expectedCompletions(const arena_authority::State& arena) {
  std::map<PlayerId, StepId> completions;
  for (const PlayerId& player : arena_authority::alivePlayers(arena)) {
    if (std::optional<StepId> completion = arena_authority::expectedCompletionStep(arena, player)) {
      completions[player] = *completion;
    }
  }
  return completions;
}

// What choosing a successor would earn each player now, as the Game Authority decides it from the player's
// executing order (Arena Authority), selection (Order Selection Authority) and transmitted orders
std::map<PlayerId, game_authority::HandoverProspect> handoverProspects(
    const State& state,
    const order_selection_authority::View& selections,
    const std::map<PlayerId, StepId>& completions) {
  std::map<PlayerId, game_authority::HandoverProspect> prospects;
  for (const PlayerId& player : arena_authority::alivePlayers(*state.arena)) {
    game_authority::ProspectContext context{
        arena_authority::lastCompletedStep(*state.arena), std::nullopt, std::nullopt, false, false};
    if (auto completion = completions.find(player); completion != completions.end()) {
      context.expected_completion = completion->second;
    }
    if (auto selection = selections.selections.find(player); selection != selections.selections.end()) {
      context.selected_at = selection->second.submitted_at;
      context.selection_replaced = selection->second.replaced.has_value();
    }
    context.transmission_pending =
        std::any_of(state.transmitted.begin(), state.transmitted.end(), [&player](const OrderTransmitted& sent) {
          return sent.player == player;
        });
    prospects[player] = game_authority::handoverProspect(*state.game, player, context);
  }
  return prospects;
}

Observations withViews(Observations observations, const State& state) {
  if (isStarted(state)) {
    order_selection_authority::View selections = order_selection_authority::view(*state.selections);
    std::map<PlayerId, std::vector<Direction>> selected_routes =
        routesTo(*state.arena, mechanics::selectedTargets(selections));
    std::map<PlayerId, std::vector<Direction>> transmitted_routes =
        routesTo(*state.arena, mechanics::transmittedTargets(state.transmitted));
    std::map<PlayerId, StepId> completions = expectedCompletions(*state.arena);
    std::map<PlayerId, game_authority::HandoverProspect> prospects = handoverProspects(state, selections, completions);
    observations.views = Views{arena_authority::view(*state.arena),
                               std::move(selections),
                               state.transmitted,
                               game_authority::view(*state.game),
                               pacing_policy::STEP_INTERVAL,
                               pacing_policy::LAST_STEP,
                               transmission_schedule_policy::opportunitiesOfGame(),
                               std::move(completions),
                               std::move(selected_routes),
                               std::move(transmitted_routes),
                               std::move(prospects)};
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

// Carry out the Game Authority's conclusion intent: end execution, then cancel the selections
std::tuple<State, Observations> conclude(State state, const game_authority::GameConcluded& concluded) {
  auto [arena, end_events] = arena_authority::endRound(std::move(*state.arena));
  state.arena = std::move(arena);
  state.selections = order_selection_authority::cancel(std::move(*state.selections));

  Observations observations;
  observations.end_of_round_events = std::move(end_events);
  observations.concluded = concluded;
  return {std::move(state), std::move(observations)};
}

// One arena step, with every Authority observing its complete outcome before the next step
std::tuple<State, Observations> runStep(const RandomIntGeneratorFn& random_int_fn, State state) {
  state.clock.next_step += pacing_policy::STEP_INTERVAL;

  const StepId step = arena_authority::upcomingStep(*state.arena);
  auto [released, waiting] = releasedOrders(std::move(state.transmitted), step);
  state.transmitted = std::move(waiting);
  auto [arena, activations, events] = arena_authority::tick(random_int_fn, std::move(*state.arena), released);
  state.arena = std::move(arena);
  auto [game, handovers, concluded] = game_authority::observeStep(
      std::move(*state.game), step, activations, events, arena_authority::alivePlayers(*state.arena));
  state.game = std::move(game);

  Observations observations;
  observations.steps.push_back(StepReport{step, std::move(activations), std::move(events), {}, std::move(handovers)});
  if (concluded) {
    auto [concluded_state, conclusion] = conclude(std::move(state), *concluded);
    return {std::move(concluded_state), appendObservations(std::move(observations), std::move(conclusion))};
  }
  if (transmission_schedule_policy::isOpportunity(step)) {
    auto [selections, transmissions] = order_selection_authority::transmit(std::move(*state.selections), step);
    state.selections = std::move(selections);
    state.transmitted.insert(state.transmitted.end(), transmissions.begin(), transmissions.end());
    for (const OrderTransmitted& transmitted : transmissions) {
      auto [game_with_transmission, granted] = game_authority::observeTransmission(std::move(*state.game), transmitted);
      state.game = std::move(game_with_transmission);
      std::vector<game_authority::HandoverGranted>& reported = observations.steps.back().handovers;
      reported.insert(reported.end(), granted.begin(), granted.end());
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
  state.selections = order_selection_authority::startRound(setup.round);
  state.transmitted.clear();
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
    observations.submission = SubmissionRejected{submit.player, *reason};
    observations = withViews(std::move(observations), state);
    return {std::move(state), std::move(observations)};
  }

  auto [selections, submitted] = order_selection_authority::submit(
      std::move(*state.selections),
      order_selection_authority::Submission{
          submit.player, submit.target, arena_authority::lastCompletedStep(*state.arena)});
  state.selections = std::move(selections);
  observations.submission = SubmissionAccepted{submitted.player, submitted.order, submitted.submitted_at};
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

  const std::chrono::milliseconds until = state.clock.now + elapsed.duration;
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
