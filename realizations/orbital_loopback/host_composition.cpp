#include "orbital_loopback/host_composition.hpp"

#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "domain/orbital/admission_policy.hpp"
#include "domain/orbital/board_setup_policy.hpp"
#include "domain/orbital/pacing_policy.hpp"
#include "domain/orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

namespace reception = platform::transmission_reception_authority;
using admission_policy::GamePhase;
using State = HostComposition::State;

// Everything one input caused, in order of occurrence
struct Observations {
  std::optional<game_boundary::StartRejected> start_rejected;
  std::vector<game_boundary::StepReport> steps;
  ArenaEvents end_of_round_events;  // Orders ended because the game concluded
  std::optional<game_authority::GameConcluded> concluded;
  std::optional<game_boundary::Views> views;           // Views after the input, once started
  std::optional<placement_link::GameStarted> started;  // For the devices, after the report
};

template <typename TItem>
std::vector<TItem> appended(std::vector<TItem> list, std::vector<TItem> items) {
  list.insert(list.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
  return list;
}

// Phase of the game, read from whether one started and from the Game Authority
GamePhase phaseOf(const State& state) {
  if (!state.game) {
    return GamePhase::NOT_STARTED;
  }
  return game_authority::isRunning(*state.game) ? GamePhase::RUNNING : GamePhase::CONCLUDED;
}

// ----------------------------------------------------------------------------
// Views, built from the host's own owners
// ----------------------------------------------------------------------------

// Each player's target, of several transmissions of a player the latest
std::map<PlayerId, Point> targetsOf(const std::vector<OrderTransmitted>& transmitted) {
  std::map<PlayerId, Point> targets;
  for (const OrderTransmitted& sent : transmitted) {
    targets[sent.player] = sent.target;
  }
  return targets;
}

std::map<PlayerId, Point> targetsOf(const order_selection_authority::View& selections) {
  std::map<PlayerId, Point> targets;
  for (const auto& [player, selection] : selections.selections) {
    targets[player] = selection.order.target;
  }
  return targets;
}

// Route each target would get from the player's snake now, as the Arena Authority answers
std::map<PlayerId, std::vector<Direction>> routesNowTo(const arena_authority::State& arena,
                                                       const std::map<PlayerId, Point>& targets) {
  std::map<PlayerId, std::vector<Direction>> routes;
  for (const auto& [player, target] : targets) {
    if (std::optional<std::vector<Direction>> route = arena_authority::routeIfActivatedNow(arena, player, target)) {
      routes[player] = std::move(*route);
    }
  }
  return routes;
}

// Step each executing order is expected to complete in, as the Arena Authority expects it
std::map<PlayerId, StepId> completionsOf(const arena_authority::State& arena) {
  std::map<PlayerId, StepId> completions;
  for (const PlayerId& player : arena_authority::alivePlayers(arena)) {
    if (std::optional<StepId> completion = arena_authority::expectedCompletionStep(arena, player)) {
      completions[player] = *completion;
    }
  }
  return completions;
}

// What choosing a successor would earn each player now, as the Game Authority answers from the executing order
// (Arena Authority), the shown selection and the waiting transmissions (Transmission Reception Authority)
std::map<PlayerId, game_authority::HandoverProspect> prospectsOf(const State& state,
                                                                 const std::map<PlayerId, StepId>& completions,
                                                                 const std::vector<OrderTransmitted>& waiting) {
  std::map<PlayerId, game_authority::HandoverProspect> prospects;
  for (const PlayerId& player : arena_authority::alivePlayers(*state.arena)) {
    game_authority::ProspectContext context{
        arena_authority::lastCompletedStep(*state.arena), std::nullopt, std::nullopt, false, false};
    if (auto completion = completions.find(player); completion != completions.end()) {
      context.expected_completion = completion->second;
    }
    if (auto selection = state.selections.selections.find(player); selection != state.selections.selections.end()) {
      context.selected_at = selection->second.submitted_at;
      context.selection_replaced = selection->second.replaced.has_value();
    }
    for (const OrderTransmitted& sent : waiting) {
      context.transmission_pending = context.transmission_pending || sent.player == player;
    }
    prospects[player] = game_authority::handoverProspect(*state.game, player, context);
  }
  return prospects;
}

Observations withViews(Observations observations, const State& state) {
  if (!state.game) {
    return observations;
  }
  std::vector<OrderTransmitted> waiting = reception::waiting(state.reception);
  std::map<PlayerId, StepId> completions = completionsOf(*state.arena);
  std::map<PlayerId, game_authority::HandoverProspect> prospects = prospectsOf(state, completions, waiting);
  std::map<PlayerId, std::vector<Direction>> selected_routes = routesNowTo(*state.arena, targetsOf(state.selections));
  std::map<PlayerId, std::vector<Direction>> transmitted_routes = routesNowTo(*state.arena, targetsOf(waiting));
  observations.views = game_boundary::Views{arena_authority::view(*state.arena),
                                            state.selections,
                                            std::move(waiting),
                                            game_authority::view(*state.game),
                                            pacing_policy::STEP_INTERVAL,
                                            pacing_policy::LAST_STEP,
                                            transmission_schedule_policy::opportunitiesOfGame(),
                                            std::move(completions),
                                            std::move(selected_routes),
                                            std::move(transmitted_routes),
                                            std::move(prospects)};
  return observations;
}

// ----------------------------------------------------------------------------
// Inputs
// ----------------------------------------------------------------------------

std::vector<PlayerId> playersOf(const arena_authority::RoundSetup& setup) {
  std::vector<PlayerId> players;
  for (const auto& [player, snake] : setup.layout.snakes) {
    players.push_back(player);
  }
  return players;
}

// Begin the game, as far as the Admission Policy admits a start
std::tuple<State, Observations> started(const RandomIntGeneratorFn& random_int_fn,
                                        State state,
                                        const game_boundary::Start& start,
                                        std::chrono::milliseconds t0) {
  Observations observations;
  if (std::optional<game_boundary::StartRejectionReason> reason = admission_policy::startRejectionIn(phaseOf(state))) {
    observations.start_rejected = game_boundary::StartRejected{*reason};
    observations = withViews(std::move(observations), state);
    return {std::move(state), std::move(observations)};
  }

  const arena_authority::RoundSetup setup{start.round, board_setup_policy::startLayout(), {}};
  state.arena = arena_authority::startRound(random_int_fn, setup);
  state.game = game_authority::startRound(setup.round, playersOf(setup));
  state.reception = reception::startRound(std::move(state.reception));
  state.selections = order_selection_authority::View{};
  state.t0 = t0;
  observations.started = placement_link::GameStarted{start.round, t0};
  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

// Carry out the Game Authority's conclusion intent: end execution, stop showing selections, discard what still waits
std::tuple<State, Observations> concludedWith(State state,
                                              Observations observations,
                                              const game_authority::GameConcluded& concluded) {
  auto [arena, end_events] = arena_authority::endRound(std::move(*state.arena));
  state.arena = std::move(arena);
  state.selections = order_selection_authority::View{};
  auto [reception_state, discarded] = reception::conclude(std::move(state.reception));
  state.reception = std::move(reception_state);
  state.record.discarded = appended(std::move(state.record.discarded), std::move(discarded));

  observations.end_of_round_events = std::move(end_events);
  observations.concluded = concluded;
  return {std::move(state), std::move(observations)};
}

// One arena step of a running game, with every Authority observing its complete outcome
std::tuple<State, Observations> stepped(const RandomIntGeneratorFn& random_int_fn,
                                        State state,
                                        Observations observations) {
  const StepId step = arena_authority::upcomingStep(*state.arena);
  auto [reception_state, orders, late, superseded] = reception::release(std::move(state.reception), step);
  state.reception = std::move(reception_state);
  state.record.late = appended(std::move(state.record.late), std::move(late));
  state.record.superseded = appended(std::move(state.record.superseded), std::move(superseded));
  auto [arena, activations, events] = arena_authority::tick(random_int_fn, std::move(*state.arena), orders);
  state.arena = std::move(arena);
  auto [game, handovers, concluded] = game_authority::observeStep(
      std::move(*state.game), step, activations, events, arena_authority::alivePlayers(*state.arena));
  state.game = std::move(game);

  observations.steps.push_back(
      game_boundary::StepReport{step, std::move(activations), std::move(events), {}, std::move(handovers)});
  if (concluded) {
    std::tie(state, observations) = concludedWith(std::move(state), std::move(observations), *concluded);
  }
  return {std::move(state), std::move(observations)};
}

// Run every step whose time (Pacing Policy, from t0) the host time has reached, while the game is running
std::tuple<State, Observations> ranDueSteps(const RandomIntGeneratorFn& random_int_fn,
                                            State state,
                                            std::chrono::milliseconds host_time) {
  Observations observations;
  while (phaseOf(state) == GamePhase::RUNNING &&
         state.t0 + pacing_policy::timeOfStep(arena_authority::upcomingStep(*state.arena)) <= host_time) {
    std::tie(state, observations) = stepped(random_int_fn, std::move(state), std::move(observations));
  }
  observations = withViews(std::move(observations), state);
  return {std::move(state), std::move(observations)};
}

// Show the forwarded selection, as far as the Transmission Reception Authority receives it
std::tuple<State, Observations> selected(State state, const OrderSubmitted& submitted) {
  const reception::Receipt receipt = reception::receiptOf(state.reception, submitted);
  state.record.submission_receipts.push_back(SubmissionReceipt{submitted, receipt});
  if (receipt == reception::Receipt::RECEIVED) {
    state.selections.selections[submitted.player] = order_selection_authority::Selection{
        Order{submitted.round, submitted.order, submitted.player, submitted.target},
        submitted.submitted_at,
        submitted.replaced};
  }
  Observations observations = withViews({}, state);
  return {std::move(state), std::move(observations)};
}

// Receive the forwarded transmission; once received, it ends showing the player's selection and is learned
std::tuple<State, Observations> received(State state, const OrderTransmitted& transmitted) {
  auto [reception_state, receipt] = reception::receive(std::move(state.reception), transmitted);
  state.reception = std::move(reception_state);
  std::vector<game_authority::HandoverGranted> handovers;
  if (receipt == reception::Receipt::RECEIVED) {
    state.selections.selections.erase(transmitted.player);
    std::tie(state.game, handovers) = game_authority::observeTransmission(std::move(*state.game), transmitted);
  }
  state.record.receipts.push_back(TransmissionReceipt{transmitted, receipt, std::move(handovers)});
  Observations observations = withViews({}, state);
  return {std::move(state), std::move(observations)};
}

void deliver(const Observations& observations,
             game_boundary::PlayersEndOutbound& players,
             const std::vector<std::reference_wrapper<placement_link::HostToPhone>>& phones) {
  if (observations.start_rejected) {
    players.startRejected(*observations.start_rejected);
  }
  for (const game_boundary::StepReport& step : observations.steps) {
    players.stepReported(step);
  }
  if (!observations.end_of_round_events.empty()) {
    players.ordersEndedWithRound(observations.end_of_round_events);
  }
  if (observations.concluded) {
    players.concluded(*observations.concluded);
  }
  if (observations.views) {
    players.viewsUpdated(*observations.views);
  }
  for (placement_link::HostToPhone& phone : phones) {
    if (observations.started) {
      phone.gameStarted(*observations.started);
    }
    if (observations.concluded) {
      phone.concluded(*observations.concluded);
    }
  }
}

}  // namespace

HostComposition::HostComposition(game_boundary::PlayersEndOutbound& players,
                                 std::vector<std::reference_wrapper<placement_link::HostToPhone>> phones,
                                 RandomIntGeneratorFn random_int_fn)
    : players_{players}, phones_{std::move(phones)}, random_int_fn_{std::move(random_int_fn)} {
  if (!random_int_fn_) {
    throw std::invalid_argument{"a random number source is required"};
  }
}

void HostComposition::start(const game_boundary::Start& start, std::chrono::milliseconds t0) {
  take(StartAt{start, t0});
}

void HostComposition::runDueSteps(std::chrono::milliseconds host_time) { take(DueStepsAt{host_time}); }

void HostComposition::forward(const OrderSubmitted& submitted) { take(submitted); }

void HostComposition::forward(const OrderTransmitted& transmitted) { take(transmitted); }

const Record& HostComposition::record() const { return state_.record; }

void HostComposition::take(Input input) {
  waiting_.push_back(std::move(input));
  if (reporting_) {
    return;  // A callback of the report under way called; the input waits for its turn
  }

  reporting_ = true;
  while (!waiting_.empty()) {
    const Input next = std::move(waiting_.front());
    waiting_.pop_front();
    auto [state, observations] = std::visit(
        [this](const auto& item) {
          using TInput = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<TInput, StartAt>) {
            return started(random_int_fn_, std::move(state_), item.start, item.t0);
          } else if constexpr (std::is_same_v<TInput, DueStepsAt>) {
            return ranDueSteps(random_int_fn_, std::move(state_), item.host_time);
          } else if constexpr (std::is_same_v<TInput, OrderSubmitted>) {
            return selected(std::move(state_), item);
          } else {
            return received(std::move(state_), item);
          }
        },
        next);
    state_ = std::move(state);
    deliver(observations, players_, phones_);
  }
  reporting_ = false;
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
