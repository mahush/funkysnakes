#include "orbital/order_authority.hpp"

#include <tuple>

#include "orbital/order_resolution_policy.hpp"
#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

namespace {

using Orders = std::map<PlayerId, std::map<OrderNumber, OrderRecord>>;
using SelectedOrders = std::map<PlayerId, OrderNumber>;
using TransmittedOrders = std::map<PlayerId, std::set<OrderNumber>>;

// Drops the player's selected entry if it is this order; a newer selected order of the player is kept
SelectedOrders dropSelectedOrder(SelectedOrders selected, const PlayerId& player, OrderNumber order) {
  auto it = selected.find(player);
  if (it != selected.end() && it->second == order) {
    selected.erase(it);
  }
  return selected;
}

bool isKnown(const Orders& orders, const PlayerId& player, OrderNumber order) {
  auto player_orders = orders.find(player);
  return player_orders != orders.end() && player_orders->second.count(order) > 0;
}

TransmittedOrders dropTransmittedOrder(TransmittedOrders transmitted, const PlayerId& player, OrderNumber order) {
  auto player_transmitted = transmitted.find(player);
  if (player_transmitted != transmitted.end()) {
    player_transmitted->second.erase(order);
    if (player_transmitted->second.empty()) {
      transmitted.erase(player_transmitted);
    }
  }
  return transmitted;
}

// The Arena Authority actually activated the order: it executes as the Order Resolution Policy decides
std::tuple<Orders, SelectedOrders, TransmittedOrders> applyOutcome(Orders orders,
                                                                   SelectedOrders selected,
                                                                   TransmittedOrders transmitted,
                                                                   const OrderActivated& activated) {
  if (!isKnown(orders, activated.player, activated.order)) {
    return {std::move(orders), std::move(selected), std::move(transmitted)};
  }
  orders.at(activated.player).at(activated.order).status = order_resolution_policy::statusAfterActivation();
  selected = dropSelectedOrder(std::move(selected), activated.player, activated.order);
  transmitted = dropTransmittedOrder(std::move(transmitted), activated.player, activated.order);
  return {std::move(orders), std::move(selected), std::move(transmitted)};
}

// A refusal resolves the order only as far as the Order Resolution Policy decides
std::tuple<Orders, SelectedOrders, TransmittedOrders> applyOutcome(Orders orders,
                                                                   SelectedOrders selected,
                                                                   TransmittedOrders transmitted,
                                                                   const OrderRejected& rejected) {
  if (!isKnown(orders, rejected.player, rejected.order)) {
    return {std::move(orders), std::move(selected), std::move(transmitted)};
  }
  OrderRecord& record = orders.at(rejected.player).at(rejected.order);
  std::optional<Status> status = order_resolution_policy::statusAfterRejection(record.status);
  if (!status) {
    return {std::move(orders), std::move(selected), std::move(transmitted)};
  }
  record.status = *status;
  record.rejection = rejected.reason;
  transmitted = dropTransmittedOrder(std::move(transmitted), rejected.player, rejected.order);
  return {std::move(orders), std::move(selected), std::move(transmitted)};
}

Orders applyEvent(Orders orders, const OrderEnded& ended) {
  if (!isKnown(orders, ended.player, ended.order)) {
    return orders;
  }
  orders.at(ended.player).at(ended.order).status = order_resolution_policy::statusAfterEnding(ended.reason);
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
  const OrderNumber id{++state.submitted_count_[submission.player]};
  std::map<OrderNumber, OrderRecord>& player_orders = state.orders_[submission.player];

  std::optional<OrderNumber> replaced;
  auto selected = state.selected_.find(submission.player);
  if (selected != state.selected_.end()) {
    player_orders.at(selected->second).status = Status::REPLACED;
    replaced = selected->second;
  }

  const Order order{state.round_, id, submission.player, submission.target};
  player_orders.emplace(
      id, OrderRecord{order, submission.submitted_at, std::nullopt, replaced, Status::SELECTED, std::nullopt});
  state.selected_[submission.player] = id;
  const OrderSubmitted submitted{state.round_, id, submission.player, submission.submitted_at, replaced};
  return {std::move(state), submitted};
}

std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at) {
  std::vector<OrderTransmitted> transmissions;
  std::map<PlayerId, OrderNumber> still_selected;
  for (const auto& [player, id] : state.selected_) {
    OrderRecord& record = state.orders_.at(player).at(id);
    // An order waits for the opportunity of its submission (Transmission Schedule Policy)
    if (at < transmission_schedule_policy::opportunityOfSubmission(record.submitted_at)) {
      still_selected.emplace(player, id);
      continue;
    }
    record.status = Status::TRANSMITTED;
    record.transmitted_at = at;
    state.transmitted_[player].insert(id);
    transmissions.push_back(OrderTransmitted{state.round_, id, player, record.submitted_at, at, record.replaced});
  }
  state.selected_ = std::move(still_selected);
  return {std::move(state), std::move(transmissions)};
}

std::vector<Order> transmittedOrders(const State& state, StepId at) {
  std::vector<Order> result;
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (OrderNumber id : transmitted) {
      const OrderRecord& record = state.orders_.at(player).at(id);
      if (record.transmitted_at &&
          transmission_schedule_policy::firstStepAfterTransmission(*record.transmitted_at) <= at) {
        result.push_back(record.order);
      }
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
    state.orders_.at(player).at(id).status = Status::CANCELLED;
  }
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (OrderNumber id : transmitted) {
      state.orders_.at(player).at(id).status = Status::CANCELLED;
    }
  }
  state.selected_.clear();
  state.transmitted_.clear();
  return state;
}

View view(const State& state) {
  View result;
  for (const auto& [player, player_orders] : state.orders_) {
    PlayerOrders& situation = result.players[player];
    for (const auto& [id, record] : player_orders) {
      result.orders.push_back(record);
      if (record.status == Status::EXECUTING) {
        situation.executing = record;
      }
    }
  }
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (OrderNumber id : transmitted) {
      result.players[player].transmitted = state.orders_.at(player).at(id);
    }
  }
  for (const auto& [player, id] : state.selected_) {
    result.players[player].selected = state.orders_.at(player).at(id);
  }
  return result;
}

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
