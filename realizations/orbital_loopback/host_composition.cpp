#include "orbital_loopback/host_composition.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

#include "orbital/admission_policy.hpp"
#include "orbital/board_setup_policy.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/pacing_policy.hpp"
#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace replica = platform::order_replica_authority;

HostComposition::HostComposition(HostOutbound& outbound, RandomIntGeneratorFn random_int_fn)
    : outbound_{outbound}, random_int_fn_{std::move(random_int_fn)} {
  if (!random_int_fn_) {
    throw std::invalid_argument{"a random number source is required"};
  }
}

bool HostComposition::isRunning() const { return game_ && game_authority::isRunning(game_->game); }

bool HostComposition::start(RoundId round, std::chrono::milliseconds t0) {
  const admission_policy::GamePhase phase = !game_        ? admission_policy::GamePhase::NOT_STARTED
                                            : isRunning() ? admission_policy::GamePhase::RUNNING
                                                          : admission_policy::GamePhase::CONCLUDED;
  if (admission_policy::startRejectionIn(phase)) {
    return false;
  }

  const arena_authority::RoundSetup setup{round, board_setup_policy::startLayout(), {}};
  std::vector<PlayerId> players;
  for (const auto& [player, snake] : setup.layout.snakes) {
    players.push_back(player);
  }
  arena_authority::State arena = arena_authority::startRound(random_int_fn_, setup);
  replica::State orders = replica::startRound(round, arena_authority::upcomingStep(arena));
  game_ = Game{std::move(arena), std::move(orders), game_authority::startRound(round, players)};

  outbound_.roundStarted(
      RoundStarted{round, t0, pacing_policy::STEP_INTERVAL, transmission_schedule_policy::opportunitiesOfGame()});
  reportViews();
  return true;
}

void HostComposition::receive(const platform::ForwardedSubmission& submission) {
  if (!isRunning()) {
    return;
  }
  game_->orders = replica::recordSubmission(std::move(game_->orders), submission);
  outbound_.submissionReceived(submission);
  reportViews();
}

void HostComposition::receive(const platform::ForwardedTransmission& transmission) {
  if (!isRunning()) {
    return;
  }
  auto [orders, late] = replica::recordTransmission(std::move(game_->orders), transmission);
  game_->orders = std::move(orders);
  auto [game, handovers] = game_authority::observeTransmission(std::move(game_->game), transmission.transmitted);
  game_->game = std::move(game);

  outbound_.transmissionReceived(transmission, handovers);
  if (late) {
    outbound_.lateTransmission(*late);
  }
  reportViews();
}

void HostComposition::runStep() {
  if (!isRunning()) {
    return;
  }

  const StepId step = arena_authority::upcomingStep(game_->arena);
  std::vector<Order> released = replica::transmittedOrders(game_->orders, step);
  auto [arena, activations, events] = arena_authority::tick(random_int_fn_, std::move(game_->arena), released);
  game_->arena = std::move(arena);
  game_->orders = replica::observe(std::move(game_->orders), step, activations, events);
  auto [game, handovers, concluded] = game_authority::observeStep(
      std::move(game_->game), step, activations, events, arena_authority::alivePlayers(game_->arena));
  game_->game = std::move(game);

  outbound_.stepReported(game_boundary::StepReport{step, std::move(activations), std::move(events), {}, handovers});
  if (concluded) {
    auto [ended_arena, end_events] = arena_authority::endRound(std::move(game_->arena));
    game_->arena = std::move(ended_arena);
    game_->orders = replica::conclude(std::move(game_->orders), end_events);
    if (!end_events.empty()) {
      outbound_.ordersEndedWithRound(end_events);
    }
    outbound_.concluded(*concluded);
  }
  reportViews();
}

void HostComposition::reportViews() {
  outbound_.viewsUpdated(
      orbital_game_domain_application::viewsOf(game_->arena, replica::view(game_->orders), game_->game));
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
