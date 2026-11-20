#include "orbital_loopback/phone_composition.hpp"

#include <tuple>
#include <utility>

namespace snake {
namespace orbital {
namespace loopback {

PhoneComposition::PhoneComposition(PlayerId player, PhoneUplink& uplink)
    : player_{std::move(player)}, uplink_{uplink} {}

void PhoneComposition::roundStarted(const RoundStarted& started) {
  if (phase_ != admission_policy::GamePhase::NOT_STARTED) {
    return;
  }
  round_ = started;
  orders_ = order_authority::startRound(started.round);
  phase_ = admission_policy::GamePhase::RUNNING;
}

void PhoneComposition::stepReported(const game_boundary::StepReport& step) {
  if (orders_) {
    orders_ = order_authority::observe(std::move(*orders_), step.activations, step.events);
  }
}

void PhoneComposition::ordersEndedWithRound(const ArenaEvents& events) {
  if (orders_) {
    orders_ = order_authority::observe(std::move(*orders_), {}, events);
  }
}

void PhoneComposition::concluded(const game_authority::GameConcluded& /*concluded*/) {
  if (orders_) {
    orders_ = order_authority::cancel(std::move(*orders_));
  }
  phase_ = admission_policy::GamePhase::CONCLUDED;
}

game_boundary::SubmissionOutcome PhoneComposition::submit(Point target, std::chrono::milliseconds host_time) {
  if (std::optional<game_boundary::SubmissionRejectionReason> reason =
          admission_policy::submissionRejectionIn(phase_)) {
    return game_boundary::SubmissionRejected{player_, *reason};
  }

  // A submission between step k and step k+1 belongs to step k
  const std::chrono::milliseconds since_start = host_time - round_->t0;
  const StepId step{since_start.count() < 0 ? 0 : static_cast<int>(since_start / round_->step_interval)};
  auto [orders, submitted] =
      order_authority::submit(std::move(*orders_), order_authority::Submission{player_, target, step});
  orders_ = std::move(orders);
  uplink_.forward(platform::ForwardedSubmission{Order{round_->round, submitted.order, player_, target}, submitted});
  return game_boundary::SubmissionAccepted{player_, submitted.order, submitted.submitted_at};
}

void PhoneComposition::fireOpportunity(StepId at) {
  if (phase_ != admission_policy::GamePhase::RUNNING) {
    return;
  }
  auto [orders, transmissions] = order_authority::transmit(std::move(*orders_), at);
  orders_ = std::move(orders);
  const order_authority::View view = order_authority::view(*orders_);
  for (const OrderTransmitted& transmitted : transmissions) {
    for (const order_authority::OrderRecord& record : view.orders) {
      if (record.order.player == transmitted.player && record.order.id == transmitted.order) {
        uplink_.forward(platform::ForwardedTransmission{record.order, transmitted});
      }
    }
  }
}

std::optional<order_authority::View> PhoneComposition::orders() const {
  if (!orders_) {
    return std::nullopt;
  }
  return order_authority::view(*orders_);
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
