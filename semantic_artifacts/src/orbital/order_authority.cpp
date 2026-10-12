#include "orbital/order_authority.hpp"

#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital {
namespace order_authority {

namespace detail {

// The Authority's own access to the private representation of its state
struct Access {
  static auto& rep(State& state) { return state.rep_; }
  static const auto& rep(const State& state) { return state.rep_; }
};

}  // namespace detail

namespace {

auto& rep(State& state) { return detail::Access::rep(state); }
const auto& rep(const State& state) { return detail::Access::rep(state); }

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
  auto it = rep(state).pending.find(record.order.player);
  if (it != rep(state).pending.end() && it->second == record.order.id) {
    rep(state).pending.erase(it);
  }
}

// The Arena actually activated the order: it executes, whatever this Authority expected
State applyOutcome(State state, const OrderActivated& activated) {
  auto it = rep(state).orders.find(activated.order);
  if (it == rep(state).orders.end()) {
    return state;
  }
  it->second.status = Status::EXECUTING;
  forgetPending(state, it->second);
  return state;
}

// A refusal resolves a pending order; an order already superseded or cancelled keeps that status,
// because nothing happened to it in the world
State applyOutcome(State state, const OrderRejected& rejected) {
  auto it = rep(state).orders.find(rejected.order);
  if (it == rep(state).orders.end() || it->second.status != Status::PENDING) {
    return state;
  }
  it->second.status = Status::REJECTED;
  it->second.rejection = rejected.reason;
  forgetPending(state, it->second);
  return state;
}

State applyEvent(State state, const OrderEnded& ended) {
  auto it = rep(state).orders.find(ended.order);
  if (it == rep(state).orders.end()) {
    return state;
  }
  it->second.status = statusAfterEnding(ended.reason);
  return state;
}

// World events carry no order status; eliminations arrive as OrderEnded. Each alternative is listed so that a new
// arena event needs a deliberate interpretation here.
State applyEvent(State state, const FoodCollected& /*collected*/) { return state; }

State applyEvent(State state, const Bitten& /*bitten*/) { return state; }

State applyEvent(State state, const SelfBitten& /*self_bitten*/) { return state; }

State applyEvent(State state, const MutualBite& /*mutual_bite*/) { return state; }

}  // namespace

State startRound(RoundId round) {
  State state;
  rep(state).round = round;
  return state;
}

std::tuple<State, OrderId> submit(State state, const Submission& submission) {
  const OrderId id{rep(state).next_order_id};
  ++rep(state).next_order_id;

  auto pending = rep(state).pending.find(submission.player);
  if (pending != rep(state).pending.end()) {
    rep(state).orders.at(pending->second).status = Status::SUPERSEDED;
  }

  const Order order{rep(state).round, id, submission.player, submission.target};
  rep(state).orders.emplace(id,
                            OrderRecord{order,
                                        submission.issued_at,
                                        latency_policy::deadlineFor(submission.issued_at),
                                        Status::PENDING,
                                        std::nullopt});
  rep(state).pending[submission.player] = id;
  return {std::move(state), id};
}

std::vector<Order> eligible(const State& state, GameTime now) {
  std::vector<Order> result;
  for (const auto& [player, id] : rep(state).pending) {
    const OrderRecord& record = rep(state).orders.at(id);
    if (record.deadline <= now) {
      result.push_back(record.order);
    }
  }
  return result;
}

State observe(State state, const std::vector<ActivationOutcome>& outcomes, const ArenaEvents& events) {
  for (const ActivationOutcome& outcome : outcomes) {
    state = std::visit([&state](const auto& item) { return applyOutcome(std::move(state), item); }, outcome);
  }
  for (const ArenaEvent& event : events) {
    state = std::visit([&state](const auto& item) { return applyEvent(std::move(state), item); }, event);
  }
  return state;
}

State cancel(State state) {
  for (const auto& [player, id] : rep(state).pending) {
    rep(state).orders.at(id).status = Status::CANCELLED;
  }
  rep(state).pending.clear();
  return state;
}

View view(const State& state) {
  View result;
  for (const auto& [id, record] : rep(state).orders) {
    result.orders.push_back(record);
    PlayerOrders& player = result.players[record.order.player];
    if (record.status == Status::EXECUTING) {
      player.executing = record;
    }
  }
  for (const auto& [player, id] : rep(state).pending) {
    result.players[player].pending = rep(state).orders.at(id);
  }
  return result;
}

}  // namespace order_authority
}  // namespace orbital
}  // namespace snake
