#include "orbital/order_replica_authority.hpp"

#include <variant>

#include "orbital/late_transmission_policy.hpp"
#include "orbital/order_resolution_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace order_replica_authority {

namespace {

using order_authority::OrderRecord;
using order_authority::Status;

// Whether a forwarded Order and the fact it travels with name the same order of this round
bool belongsTo(RoundId round, const Order& order, RoundId fact_round, OrderNumber fact_order, const PlayerId& player) {
  return order.round == round && fact_round == round && order.id == fact_order && order.player == player;
}

OrderRecord* find(std::map<PlayerId, std::map<OrderNumber, OrderRecord>>& orders,
                  const PlayerId& player,
                  OrderNumber order) {
  auto player_orders = orders.find(player);
  if (player_orders == orders.end()) {
    return nullptr;
  }
  auto record = player_orders->second.find(order);
  return record == player_orders->second.end() ? nullptr : &record->second;
}

void dropSelected(std::map<PlayerId, OrderNumber>& selected, const PlayerId& player, OrderNumber order) {
  auto it = selected.find(player);
  if (it != selected.end() && it->second == order) {
    selected.erase(it);
  }
}

void dropTransmitted(std::map<PlayerId, std::map<OrderNumber, StepId>>& transmitted,
                     const PlayerId& player,
                     OrderNumber order) {
  auto player_transmitted = transmitted.find(player);
  if (player_transmitted != transmitted.end()) {
    player_transmitted->second.erase(order);
    if (player_transmitted->second.empty()) {
      transmitted.erase(player_transmitted);
    }
  }
}

}  // namespace

State startRound(RoundId round, StepId first_step) {
  State state;
  state.round_ = round;
  state.first_unprocessed_step_ = first_step;
  return state;
}

State recordSubmission(State state, const ForwardedSubmission& forwarded) {
  const Order& order = forwarded.order;
  const OrderSubmitted& submitted = forwarded.submitted;
  if (state.concluded_ ||
      !belongsTo(state.round_, order, submitted.round, submitted.order, submitted.player) ||
      find(state.orders_, order.player, order.id) != nullptr) {
    return state;
  }

  if (submitted.replaced) {
    OrderRecord* replaced = find(state.orders_, order.player, *submitted.replaced);
    if (replaced != nullptr && replaced->status == Status::SELECTED) {
      replaced->status = Status::REPLACED;
      dropSelected(state.selected_, order.player, *submitted.replaced);
    }
  }
  state.orders_[order.player].emplace(
      order.id,
      OrderRecord{order, submitted.submitted_at, std::nullopt, submitted.replaced, Status::SELECTED, std::nullopt});
  state.selected_[order.player] = order.id;
  return state;
}

std::tuple<State, std::optional<LateTransmission>> recordTransmission(State state,
                                                                      const ForwardedTransmission& forwarded) {
  const Order& order = forwarded.order;
  const OrderTransmitted& transmitted = forwarded.transmitted;
  if (state.concluded_ || !belongsTo(state.round_, order, transmitted.round, transmitted.order, transmitted.player)) {
    return {std::move(state), std::nullopt};
  }

  OrderRecord* record = find(state.orders_, order.player, order.id);
  if (record == nullptr) {
    record = &state.orders_[order.player]
                  .emplace(order.id,
                           OrderRecord{order,
                                       transmitted.submitted_at,
                                       std::nullopt,
                                       transmitted.replaced,
                                       Status::SELECTED,
                                       std::nullopt})
                  .first->second;
  } else if (record->status != Status::SELECTED) {
    return {std::move(state), std::nullopt};
  }

  record->status = Status::TRANSMITTED;
  record->transmitted_at = transmitted.transmitted_at;
  dropSelected(state.selected_, order.player, order.id);

  const late_transmission_policy::Placement placement =
      late_transmission_policy::placementOf(transmitted.transmitted_at, state.first_unprocessed_step_);
  state.transmitted_[order.player][order.id] = placement.first_step;

  std::optional<LateTransmission> late;
  if (placement.isLate()) {
    late = LateTransmission{
        order.player, order.id, transmitted.transmitted_at, placement.scheduled_step, placement.first_step};
  }
  return {std::move(state), late};
}

namespace {

using Orders = std::map<PlayerId, std::map<OrderNumber, OrderRecord>>;
using SelectedOrders = std::map<PlayerId, OrderNumber>;
using TransmittedOrders = std::map<PlayerId, std::map<OrderNumber, StepId>>;

// What the Arena Authority reported, applied to the recorded orders as the Order Resolution Policy decides
struct Resolution {
  Orders& orders;
  SelectedOrders& selected;
  TransmittedOrders& transmitted;

  void operator()(const OrderActivated& activated) const {
    OrderRecord* record = find(orders, activated.player, activated.order);
    if (record == nullptr) {
      return;
    }
    record->status = order_resolution_policy::statusAfterActivation();
    dropSelected(selected, activated.player, activated.order);
    dropTransmitted(transmitted, activated.player, activated.order);
  }

  void operator()(const OrderRejected& rejected) const {
    OrderRecord* record = find(orders, rejected.player, rejected.order);
    if (record == nullptr) {
      return;
    }
    std::optional<Status> status = order_resolution_policy::statusAfterRejection(record->status);
    if (!status) {
      return;
    }
    record->status = *status;
    record->rejection = rejected.reason;
    dropTransmitted(transmitted, rejected.player, rejected.order);
  }

  void operator()(const OrderEnded& ended) const {
    if (OrderRecord* record = find(orders, ended.player, ended.order)) {
      record->status = order_resolution_policy::statusAfterEnding(ended.reason);
    }
  }

  // World events carry no order status; eliminations arrive as OrderEnded
  void operator()(const FoodCollected& /*collected*/) const {}
  void operator()(const Bitten& /*bitten*/) const {}
  void operator()(const SelfBitten& /*self_bitten*/) const {}
  void operator()(const MutualBite& /*mutual_bite*/) const {}

  void apply(const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events) const {
    for (const ActivationOutcome& outcome : outcomes) {
      std::visit(*this, outcome);
    }
    for (const ArenaEvent& event : events) {
      std::visit(*this, event);
    }
  }
};

}  // namespace

State observe(State state, StepId step, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events) {
  Resolution{state.orders_, state.selected_, state.transmitted_}.apply(outcomes, events);
  if (state.first_unprocessed_step_ <= step) {
    state.first_unprocessed_step_ = StepId{step.value + 1};
  }
  return state;
}

State conclude(State state, const ArenaEvents& end_events) {
  Resolution{state.orders_, state.selected_, state.transmitted_}.apply({}, end_events);
  for (const auto& [player, id] : state.selected_) {
    state.orders_.at(player).at(id).status = Status::CANCELLED;
  }
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (const auto& [id, first_step] : transmitted) {
      state.orders_.at(player).at(id).status = Status::CANCELLED;
    }
  }
  state.selected_.clear();
  state.transmitted_.clear();
  state.concluded_ = true;
  return state;
}

std::vector<Order> transmittedOrders(const State& state, StepId at) {
  std::vector<Order> result;
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (const auto& [id, first_step] : transmitted) {
      if (first_step <= at) {
        result.push_back(state.orders_.at(player).at(id).order);
      }
    }
  }
  return result;
}

order_authority::View view(const State& state) {
  order_authority::View result;
  for (const auto& [player, player_orders] : state.orders_) {
    order_authority::PlayerOrders& situation = result.players[player];
    for (const auto& [id, record] : player_orders) {
      result.orders.push_back(record);
      if (record.status == Status::EXECUTING) {
        situation.executing = record;
      }
    }
  }
  for (const auto& [player, transmitted] : state.transmitted_) {
    for (const auto& [id, first_step] : transmitted) {
      result.players[player].transmitted = state.orders_.at(player).at(id);
    }
  }
  for (const auto& [player, id] : state.selected_) {
    result.players[player].selected = state.orders_.at(player).at(id);
  }
  return result;
}

}  // namespace order_replica_authority
}  // namespace platform
}  // namespace orbital
}  // namespace snake
