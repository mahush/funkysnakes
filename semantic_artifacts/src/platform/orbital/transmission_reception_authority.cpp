#include "platform/orbital/transmission_reception_authority.hpp"

#include <map>
#include <utility>
#include <variant>

#include "platform/orbital/transmission_participation_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace transmission_reception_authority {

namespace {

using transmission_participation_policy::Participation;
using transmission_participation_policy::Superseded;
using transmission_participation_policy::TakesPartLate;
using transmission_participation_policy::Waits;

Order orderOf(const OrderTransmitted& transmitted) {
  return Order{transmitted.round, transmitted.order, transmitted.player, transmitted.target};
}

}  // namespace

State startRound() { return State{}; }

std::tuple<State, Receipt> receive(State state, const OrderTransmitted& transmitted) {
  if (state.concluded_) {
    return {std::move(state), Receipt::ROUND_CONCLUDED};
  }
  state.waiting_.push_back(transmitted);
  return {std::move(state), Receipt::RECEIVED};
}

std::tuple<State, std::vector<Order>, std::vector<LateTransmission>, std::vector<SupersededTransmission>> release(
    State state, StepId step) {
  std::vector<OrderTransmitted> still_waiting;
  std::map<PlayerId, Order> released;
  std::vector<LateTransmission> late;
  std::vector<SupersededTransmission> superseded;
  for (const OrderTransmitted& sent : state.waiting_) {
    const Participation participation = transmission_participation_policy::participationOf(sent, state.waiting_, step);
    if (std::holds_alternative<Waits>(participation)) {
      still_waiting.push_back(sent);
    } else if (const auto* newer = std::get_if<Superseded>(&participation)) {
      superseded.push_back(SupersededTransmission{sent, newer->by, step});
    } else {
      released.emplace(sent.player, orderOf(sent));
      if (std::holds_alternative<TakesPartLate>(participation)) {
        late.push_back(LateTransmission{sent, step});
      }
    }
  }
  state.waiting_ = std::move(still_waiting);

  std::vector<Order> orders;
  for (const auto& [player, order] : released) {
    orders.push_back(order);
  }
  return {std::move(state), std::move(orders), std::move(late), std::move(superseded)};
}

std::tuple<State, std::vector<DiscardedTransmission>> conclude(State state) {
  std::vector<DiscardedTransmission> discarded;
  for (const OrderTransmitted& sent : state.waiting_) {
    discarded.push_back(DiscardedTransmission{sent});
  }
  state.waiting_.clear();
  state.concluded_ = true;
  return {std::move(state), std::move(discarded)};
}

std::vector<OrderTransmitted> waiting(const State& state) { return state.waiting_; }

}  // namespace transmission_reception_authority
}  // namespace platform
}  // namespace orbital
}  // namespace snake
