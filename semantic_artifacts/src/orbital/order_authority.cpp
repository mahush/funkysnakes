#include "orbital/order_authority.hpp"

#include <tuple>

#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

namespace {

using Orders = std::map<OrderId, OrderRecord>;
using SelectedOrders = std::map<PlayerId, OrderId>;
using TransmittedOrders = std::set<OrderId>;

Status statusAfterEnding(EndReason reason) {
  switch (reason) {
    case EndReason::COMPLETED:
      return Status::COMPLETED;
    case EndReason::INTERRUPTED:
      return Status::INTERRUPTED;
    case EndReason::ELIMINATED:
      return Status::FAILED;
    case EndReason::ROUND_ENDED:
      return Status::ENDED_BY_ROUND;
  }
  return Status::FAILED;
}

// Drops the player's selected entry if it is this order; a newer selected order of the player is kept
SelectedOrders dropSelectedOrder(SelectedOrders selected, const PlayerId& player, OrderId order) {
  auto it = selected.find(player);
  if (it != selected.end() && it->second == order) {
    selected.erase(it);
  }
  return selected;
}

// The Arena Authority actually activated the order: it executes, whatever this Authority expected
std::tuple<Orders, SelectedOrders, TransmittedOrders> applyOutcome(Orders orders,
                                                                   SelectedOrders selected,
                                                                   TransmittedOrders transmitted,
                                                                   const OrderActivated& activated) {
  auto it = orders.find(activated.order);
  if (it == orders.end()) {
    return {std::move(orders), std::move(selected), std::move(transmitted)};
  }
  it->second.status = Status::EXECUTING;
  selected = dropSelectedOrder(std::move(selected), it->second.order.player, it->second.order.id);
  transmitted.erase(activated.order);
  return {std::move(orders), std::move(selected), std::move(transmitted)};
}

// A refusal resolves a transmitted order; any other order keeps its status, because nothing happened
// to it in the world
std::tuple<Orders, SelectedOrders, TransmittedOrders> applyOutcome(Orders orders,
                                                                   SelectedOrders selected,
                                                                   TransmittedOrders transmitted,
                                                                   const OrderRejected& rejected) {
  auto it = orders.find(rejected.order);
  if (it == orders.end() || it->second.status != Status::TRANSMITTED) {
    return {std::move(orders), std::move(selected), std::move(transmitted)};
  }
  it->second.status = Status::REJECTED;
  it->second.rejection = rejected.reason;
  transmitted.erase(rejected.order);
  return {std::move(orders), std::move(selected), std::move(transmitted)};
}

Orders applyEvent(Orders orders, const OrderEnded& ended) {
  auto it = orders.find(ended.order);
  if (it == orders.end()) {
    return orders;
  }
  it->second.status = statusAfterEnding(ended.reason);
  return orders;
}

// World events carry no order status; eliminations arrive as OrderEnded. Each alternative is listed so that a new
// arena event needs a deliberate interpretation here.
Orders applyEvent(Orders orders, const FoodCollected& /*collected*/) { return orders; }

Orders applyEvent(Orders orders, const Bitten& /*bitten*/) { return orders; }

Orders applyEvent(Orders orders, const SelfBitten& /*self_bitten*/) { return orders; }

Orders applyEvent(Orders orders, const MutualBite& /*mutual_bite*/) { return orders; }

}  // namespace

State startRound(RoundId round) {
  State state;
  state.round_ = round;
  return state;
}

std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission) {
  const OrderId id{state.next_order_id_};
  ++state.next_order_id_;

  std::optional<OrderId> replaced;
  auto selected = state.selected_.find(submission.player);
  if (selected != state.selected_.end()) {
    state.orders_.at(selected->second).status = Status::REPLACED;
    replaced = selected->second;
  }

  const Order order{state.round_, id, submission.player, submission.target};
  state.orders_.emplace(
      id, OrderRecord{order, submission.submitted_at, std::nullopt, replaced, Status::SELECTED, std::nullopt});
  state.selected_[submission.player] = id;
  const OrderSubmitted submitted{state.round_, id, submission.player, submission.submitted_at, replaced};
  return {std::move(state), submitted};
}

std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at) {
  std::vector<OrderTransmitted> transmissions;
  for (const auto& [player, id] : state.selected_) {
    OrderRecord& record = state.orders_.at(id);
    record.status = Status::TRANSMITTED;
    record.transmitted_at = at;
    state.transmitted_.insert(id);
    transmissions.push_back(OrderTransmitted{state.round_, id, player, record.submitted_at, at, record.replaced});
  }
  state.selected_.clear();
  return {std::move(state), std::move(transmissions)};
}

std::vector<Order> transmittedOrders(const State& state, StepId at) {
  std::vector<Order> result;
  for (OrderId id : state.transmitted_) {
    const OrderRecord& record = state.orders_.at(id);
    if (record.transmitted_at &&
        transmission_schedule_policy::firstStepAfterTransmission(*record.transmitted_at) <= at) {
      result.push_back(record.order);
    }
  }
  return result;
}

State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events) {
  for (const ActivationOutcome& outcome : outcomes) {
    std::tie(state.orders_, state.selected_, state.transmitted_) = std::visit(
        [&state](const auto& item) {
          return applyOutcome(
              std::move(state.orders_), std::move(state.selected_), std::move(state.transmitted_), item);
        },
        outcome);
  }
  for (const ArenaEvent& event : events) {
    state.orders_ =
        std::visit([&state](const auto& item) { return applyEvent(std::move(state.orders_), item); }, event);
  }
  return state;
}

State cancel(State state) {
  for (const auto& [player, id] : state.selected_) {
    state.orders_.at(id).status = Status::CANCELLED;
  }
  for (OrderId id : state.transmitted_) {
    state.orders_.at(id).status = Status::CANCELLED;
  }
  state.selected_.clear();
  state.transmitted_.clear();
  return state;
}

View view(const State& state) {
  View result;
  for (const auto& [id, record] : state.orders_) {
    result.orders.push_back(record);
    PlayerOrders& player = result.players[record.order.player];
    if (record.status == Status::EXECUTING) {
      player.executing = record;
    }
  }
  for (OrderId id : state.transmitted_) {
    const OrderRecord& record = state.orders_.at(id);
    result.players[record.order.player].transmitted = record;
  }
  for (const auto& [player, id] : state.selected_) {
    result.players[player].selected = state.orders_.at(id);
  }
  return result;
}

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
