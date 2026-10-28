#include "orbital/game_authority.hpp"

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

#include "game_authority_mechanics.hpp"

namespace snake {
namespace orbital {
namespace game_authority {

namespace {

// An ended order's unbanked points are banked or forfeited as the Scoring Policy decides for its end reason
Scores settleOrder(Scores scores, OrderId order, EndReason reason) {
  auto it = scores.unbanked.find(order);
  if (it == scores.unbanked.end()) {
    return scores;
  }
  switch (scoring_policy::settlementOf(reason)) {
    case scoring_policy::Settlement::BANK:
      scores.credited[it->second.player] += it->second.points;
      break;
    case scoring_policy::Settlement::FORFEIT:
      break;
  }
  scores.unbanked.erase(it);
  return scores;
}

// Score a step's events in order: food collected during an order is held before a later ending of
// that order in the same step settles it
Scores scoreEvents(Scores scores, const ArenaEvents& events) {
  for (const ArenaEvent& event : events) {
    if (const auto* ended = std::get_if<OrderEnded>(&event)) {
      scores = settleOrder(std::move(scores), ended->order, ended->reason);
      continue;
    }
    for (const scoring_policy::PointEffect& effect : scoring_policy::effectsOf(event)) {
      scores = mechanics::applyEffect(std::move(scores), effect);
    }
  }
  return scores;
}

// Conclusion ends the round: every order still holding points ends with it
Scores settleAtConclusion(Scores scores) {
  std::vector<OrderId> executing;
  for (const auto& [order, unbanked] : scores.unbanked) {
    executing.push_back(order);
  }
  for (OrderId order : executing) {
    scores = settleOrder(std::move(scores), order, EndReason::ROUND_ENDED);
  }
  return scores;
}

// Highest credited score wins; equal highest scores draw
Result decideResult(const PerPlayerScores& credited) {
  std::optional<PlayerId> leader;
  bool tied = false;
  for (const auto& [player, score] : credited) {
    if (!leader || score > credited.at(*leader)) {
      leader = player;
      tied = false;
    } else if (score == credited.at(*leader)) {
      tied = true;
    }
  }
  return Result{tied ? std::nullopt : leader};
}

using Commitments = std::map<PlayerId, std::vector<OrderCommitted>>;

// Player of every order whose commitment is known
std::map<OrderId, PlayerId> playersOfOrders(const Commitments& commitments) {
  std::map<OrderId, PlayerId> players;
  for (const auto& [player, committed] : commitments) {
    for (const OrderCommitted& commitment : committed) {
      players.emplace(commitment.order, player);
    }
  }
  return players;
}

// The player's latest commitment before the given time; identities break ties in commitment order
std::optional<OrderId> latestCommittedBefore(const std::vector<OrderCommitted>& committed, GameTime at) {
  std::optional<OrderCommitted> latest;
  for (const OrderCommitted& commitment : committed) {
    if (commitment.committed_at < at &&
        (!latest || latest->committed_at < commitment.committed_at ||
         (latest->committed_at == commitment.committed_at && latest->order < commitment.order))) {
      latest = commitment;
    }
  }
  return latest ? std::optional<OrderId>{latest->order} : std::nullopt;
}

// The player's commitments from the given time on and before the other, apart from the given order
std::vector<OrderId> committedBetween(const std::vector<OrderCommitted>& committed,
                                      OrderId except,
                                      GameTime from,
                                      GameTime until) {
  std::vector<OrderId> orders;
  for (const OrderCommitted& commitment : committed) {
    if (commitment.order != except && from <= commitment.committed_at && commitment.committed_at < until) {
      orders.push_back(commitment.order);
    }
  }
  return orders;
}

OrderId orderOf(const ActivationOutcome& outcome) {
  return std::visit([](const auto& item) { return item.order; }, outcome);
}

// The player's chain as the known histories determine it, with the bonus it earned over the game
Chain deriveChain(const PlayerId& player,
                  const std::vector<OrderCommitted>& committed,
                  const std::map<OrderId, PlayerId>& players,
                  const std::vector<ObservedStep>& steps) {
  auto belongsToPlayer = [&players, &player](OrderId order) {
    auto it = players.find(order);
    return it != players.end() && it->second == player;
  };

  Chain chain;
  std::set<OrderId> resolved;                // Orders activated or rejected so far: no longer pending
  std::map<OrderId, GameTime> activated_at;  // Step time at which each order became executing
  for (const ObservedStep& step : steps) {
    for (const ActivationOutcome& outcome : step.activations) {
      const OrderId order = orderOf(outcome);
      resolved.insert(order);
      if (std::holds_alternative<OrderActivated>(outcome)) {
        activated_at.emplace(order, step.at);
      }
      // Only a refusal of the prepared successor ends the chain; a superseded draft was never prepared
      if (std::holds_alternative<OrderRejected>(outcome) && belongsToPlayer(order) &&
          latestCommittedBefore(committed, step.at) == order) {
        chain.length = 0;
      }
    }
    for (const ArenaEvent& event : step.events) {
      const auto* ended = std::get_if<OrderEnded>(&event);
      if (ended == nullptr || !belongsToPlayer(ended->order)) {
        continue;
      }
      // Successors prepared for the ended order are the commitments made while it executed; a commitment
      // made before its activation would have superseded it. A second one revised the prepared successor,
      // which forfeits the handover of this order however the later successor fares
      auto activation = activated_at.find(ended->order);
      const std::vector<OrderId> successors =
          activation == activated_at.end() ? std::vector<OrderId>{}
                                           : committedBetween(committed, ended->order, activation->second, step.at);
      const bool hands_over =
          ended->reason == EndReason::COMPLETED && successors.size() == 1 && resolved.count(successors.front()) == 0;
      if (hands_over) {
        ++chain.length;
        chain.bonus_credited += scoring_policy::handoverBonus(chain.length);
      } else {
        chain.length = 0;
      }
    }
    // Elimination ends the chain, also when the snake died with no order executing
    if (step.alive.count(player) == 0) {
      chain.length = 0;
    }
  }
  return chain;
}

// Re-derive every player's chain from the known histories and credit the change of their bonus
std::tuple<PerPlayerScores, std::map<PlayerId, Chain>> creditHandovers(PerPlayerScores credited,
                                                                       std::map<PlayerId, Chain> chains,
                                                                       const Commitments& commitments,
                                                                       const std::vector<ObservedStep>& steps) {
  const std::map<OrderId, PlayerId> players = playersOfOrders(commitments);
  for (auto& [player, score] : credited) {
    auto committed = commitments.find(player);
    const Chain derived = deriveChain(
        player, committed == commitments.end() ? std::vector<OrderCommitted>{} : committed->second, players, steps);
    Chain& chain = chains[player];
    score += derived.bonus_credited - chain.bonus_credited;
    chain = derived;
  }
  return {std::move(credited), std::move(chains)};
}

}  // namespace

State startRound(RoundId round, const std::vector<PlayerId>& players) {
  State state;
  state.round_ = round;
  for (const PlayerId& player : players) {
    state.scores_.credited[player] = 0;
  }
  return state;
}

std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                            GameTime now,
                                                            const std::vector<ActivationOutcome>& activations,
                                                            const ArenaEvents& events,
                                                            const std::set<PlayerId>& alive) {
  if (!isRunning(state)) {
    return {std::move(state), std::nullopt};
  }

  state.now_ = now;
  state.scores_ = scoreEvents(std::move(state.scores_), events);
  state.steps_.push_back(ObservedStep{now, activations, events, alive});
  std::tie(state.scores_.credited, state.chains_) =
      creditHandovers(std::move(state.scores_.credited), std::move(state.chains_), state.commitments_, state.steps_);

  if (now < state.duration_ && !alive.empty()) {
    return {std::move(state), std::nullopt};
  }

  state.scores_ = settleAtConclusion(std::move(state.scores_));
  for (auto& [player, chain] : state.chains_) {
    chain.length = 0;
  }
  state.result_ = decideResult(state.scores_.credited);
  return {state, GameConcluded{state.round_, *state.result_}};
}

State observeCommitment(State state, const OrderCommitted& committed) {
  if (!isRunning(state) || committed.round != state.round_ || state.scores_.credited.count(committed.player) == 0) {
    return state;
  }
  std::vector<OrderCommitted>& known = state.commitments_[committed.player];
  auto same_order = [&committed](const OrderCommitted& other) { return other.order == committed.order; };
  if (std::any_of(known.begin(), known.end(), same_order)) {
    return state;
  }
  known.push_back(committed);
  std::tie(state.scores_.credited, state.chains_) =
      creditHandovers(std::move(state.scores_.credited), std::move(state.chains_), state.commitments_, state.steps_);
  return state;
}

bool isRunning(const State& state) { return !state.result_.has_value(); }

View view(const State& state) {
  View result;
  for (const auto& [player, credited] : state.scores_.credited) {
    auto chain = state.chains_.find(player);
    const int length = chain == state.chains_.end() ? 0 : chain->second.length;
    result.players[player] = PlayerScore{credited, 0, length, scoring_policy::handoverBonus(length + 1)};
  }
  for (const auto& [order, unbanked] : state.scores_.unbanked) {
    result.players[unbanked.player].unbanked += unbanked.points;
  }
  result.remaining = std::max(GameTime{0}, state.duration_ - state.now_);
  result.result = state.result_;
  return result;
}

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
