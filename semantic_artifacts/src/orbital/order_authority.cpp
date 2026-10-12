#include "orbital/order_authority.hpp"

#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

namespace {

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

void forgetPending(State& state, const OrderRecord& record) {
  auto it = state.pending.find(record.order.player);
  if (it != state.pending.end() && it->second == record.order.id) {
    state.pending.erase(it);
  }
}

// The Arena actually activated the order: it executes, whatever this Authority expected
State applyOutcome(State state, const OrderActivated& activated) {
  auto it = state.orders.find(activated.order);
  if (it == state.orders.end()) {
    return state;
  }
  it->second.status = Status::EXECUTING;
  forgetPending(state, it->second);
  return state;
}

// A refusal resolves a pending order; an order already superseded or cancelled keeps that status,
// because nothing happened to it in the world
State applyOutcome(State state, const OrderRejected& rejected) {
  auto it = state.orders.find(rejected.order);
  if (it == state.orders.end() || it->second.status != Status::PENDING) {
    return state;
  }
  it->second.status = Status::REJECTED;
  it->second.rejection = rejected.reason;
  forgetPending(state, it->second);
  return state;
}

State applyFact(State state, const OrderEnded& ended) {
  auto it = state.orders.find(ended.order);
  if (it == state.orders.end()) {
    return state;
  }
  it->second.status = statusAfterEnding(ended.reason);
  return state;
}

// World facts (collection, collisions) carry no order status; eliminations arrive as OrderEnded
template <typename TWorldFact>
State applyFact(State state, const TWorldFact& /*fact*/) {
  return state;
}

}  // namespace

State startRound(RoundId round) { return State{round, 1, {}, {}}; }

std::tuple<State, OrderId> submit(State state, const Submission& submission) {
  const OrderId id{state.next_order_id};
  ++state.next_order_id;

  auto pending = state.pending.find(submission.player);
  if (pending != state.pending.end()) {
    state.orders.at(pending->second).status = Status::SUPERSEDED;
  }

  const Order order{state.round, id, submission.player, submission.target};
  state.orders.emplace(id,
                       OrderRecord{order,
                                   submission.issued_at,
                                   latency_policy::deadlineFor(submission.issued_at),
                                   Status::PENDING,
                                   std::nullopt});
  state.pending[submission.player] = id;
  return {std::move(state), id};
}

std::vector<Order> eligible(const State& state, GameTime now) {
  std::vector<Order> result;
  for (const auto& [player, id] : state.pending) {
    const OrderRecord& record = state.orders.at(id);
    if (record.deadline <= now) {
      result.push_back(record.order);
    }
  }
  return result;
}

State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaFacts& facts) {
  for (const ActivationOutcome& outcome : outcomes) {
    state = std::visit([&state](const auto& item) { return applyOutcome(std::move(state), item); }, outcome);
  }
  for (const ArenaFact& fact : facts) {
    state = std::visit([&state](const auto& item) { return applyFact(std::move(state), item); }, fact);
  }
  return state;
}

State cancel(State state) {
  for (const auto& [player, id] : state.pending) {
    state.orders.at(id).status = Status::CANCELLED;
  }
  state.pending.clear();
  return state;
}

View view(const State& state) {
  View result;
  for (const auto& [id, record] : state.orders) {
    result.orders.push_back(record);
    PlayerOrders& player = result.players[record.order.player];
    if (record.status == Status::EXECUTING) {
      player.executing = record;
    }
  }
  for (const auto& [player, id] : state.pending) {
    result.players[player].pending = state.orders.at(id);
  }
  return result;
}

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
