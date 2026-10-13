#include "orbital/order_authority.hpp"

#include <tuple>

#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

namespace {

using Orders = std::map<OrderId, OrderRecord>;
using PendingOrders = std::map<PlayerId, OrderId>;

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

// Drops the player's pending entry if it is this order; a newer pending order of the player is kept
PendingOrders dropPendingOrder(PendingOrders pending, const PlayerId& player, OrderId order) {
  auto it = pending.find(player);
  if (it != pending.end() && it->second == order) {
    pending.erase(it);
  }
  return pending;
}

// The Arena actually activated the order: it executes, whatever this Authority expected
std::tuple<Orders, PendingOrders> applyOutcome(Orders orders, PendingOrders pending, const OrderActivated& activated) {
  auto it = orders.find(activated.order);
  if (it == orders.end()) {
    return {std::move(orders), std::move(pending)};
  }
  it->second.status = Status::EXECUTING;
  pending = dropPendingOrder(std::move(pending), it->second.order.player, it->second.order.id);
  return {std::move(orders), std::move(pending)};
}

// A refusal resolves a pending order; an order already superseded or cancelled keeps that status,
// because nothing happened to it in the world
std::tuple<Orders, PendingOrders> applyOutcome(Orders orders, PendingOrders pending, const OrderRejected& rejected) {
  auto it = orders.find(rejected.order);
  if (it == orders.end() || it->second.status != Status::PENDING) {
    return {std::move(orders), std::move(pending)};
  }
  it->second.status = Status::REJECTED;
  it->second.rejection = rejected.reason;
  pending = dropPendingOrder(std::move(pending), it->second.order.player, it->second.order.id);
  return {std::move(orders), std::move(pending)};
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

std::tuple<State, OrderId> submit(State state, const Submission& submission) {
  const OrderId id{state.next_order_id_};
  ++state.next_order_id_;

  auto pending = state.pending_.find(submission.player);
  if (pending != state.pending_.end()) {
    state.orders_.at(pending->second).status = Status::SUPERSEDED;
  }

  const Order order{state.round_, id, submission.player, submission.target};
  state.orders_.emplace(id,
                        OrderRecord{order,
                                    submission.issued_at,
                                    latency_policy::deadlineFor(submission.issued_at),
                                    Status::PENDING,
                                    std::nullopt});
  state.pending_[submission.player] = id;
  return {std::move(state), id};
}

std::vector<Order> eligible(const State& state, GameTime now) {
  std::vector<Order> result;
  for (const auto& [player, id] : state.pending_) {
    const OrderRecord& record = state.orders_.at(id);
    if (record.deadline <= now) {
      result.push_back(record.order);
    }
  }
  return result;
}

State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events) {
  for (const ActivationOutcome& outcome : outcomes) {
    std::tie(state.orders_, state.pending_) = std::visit(
        [&state](const auto& item) { return applyOutcome(std::move(state.orders_), std::move(state.pending_), item); },
        outcome);
  }
  for (const ArenaEvent& event : events) {
    state.orders_ =
        std::visit([&state](const auto& item) { return applyEvent(std::move(state.orders_), item); }, event);
  }
  return state;
}

State cancel(State state) {
  for (const auto& [player, id] : state.pending_) {
    state.orders_.at(id).status = Status::CANCELLED;
  }
  state.pending_.clear();
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
  for (const auto& [player, id] : state.pending_) {
    result.players[player].pending = state.orders_.at(id);
  }
  return result;
}

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
