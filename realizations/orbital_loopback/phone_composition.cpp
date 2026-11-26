#include "orbital_loopback/phone_composition.hpp"

#include <tuple>
#include <utility>
#include <vector>

#include "domain/orbital/pacing_policy.hpp"
#include "domain/orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace loopback {

using admission_policy::GamePhase;

PhoneComposition::PhoneComposition(PlayerId player, placement_link::PhoneToHost& host)
    : player_{std::move(player)}, host_{host} {}

void PhoneComposition::gameStarted(const placement_link::GameStarted& started) {
  if (phase_ != GamePhase::NOT_STARTED) {
    return;
  }
  game_ = started;
  selections_ = order_selection_authority::startRound(started.round);
  phase_ = GamePhase::RUNNING;
}

void PhoneComposition::concluded(const game_authority::GameConcluded& /*concluded*/) {
  if (phase_ != GamePhase::RUNNING) {
    return;
  }
  selections_ = order_selection_authority::cancel(std::move(*selections_));
  phase_ = GamePhase::CONCLUDED;
}

void PhoneComposition::transmitAtDueOpportunities(std::chrono::milliseconds host_time) {
  if (phase_ != GamePhase::RUNNING) {
    return;
  }
  const std::vector<StepId>& opportunities = transmission_schedule_policy::opportunitiesOfGame();
  while (next_opportunity_ < opportunities.size() &&
         game_->t0 + pacing_policy::timeOfStep(opportunities[next_opportunity_]) <= host_time) {
    auto [selections, transmissions] =
        order_selection_authority::transmit(std::move(*selections_), opportunities[next_opportunity_]);
    selections_ = std::move(selections);
    ++next_opportunity_;
    for (const OrderTransmitted& transmitted : transmissions) {
      host_.forward(transmitted);
    }
  }
}

game_boundary::SubmissionOutcome PhoneComposition::submit(Point target, std::chrono::milliseconds host_time) {
  transmitAtDueOpportunities(host_time);
  if (std::optional<game_boundary::SubmissionRejectionReason> reason =
          admission_policy::submissionRejectionIn(phase_)) {
    return game_boundary::SubmissionRejected{player_, *reason};
  }

  auto [selections, submitted] = order_selection_authority::submit(
      std::move(*selections_),
      order_selection_authority::Submission{player_, target, pacing_policy::stepAt(host_time - game_->t0)});
  selections_ = std::move(selections);
  host_.forward(submitted);
  return game_boundary::SubmissionAccepted{submitted.player, submitted.order, submitted.submitted_at};
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
