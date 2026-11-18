#include "orbital/game_authority.hpp"

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

#include "game_authority_mechanics.hpp"
#include "orbital/pacing_policy.hpp"
#include "orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace game_authority {

namespace {

// An ended order's held points are banked or dropped as the Scoring Policy decides for its end reason
Scores settleOrder(Scores scores, OrderId order, EndReason reason) {
  auto it = scores.held.find(order);
  if (it == scores.held.end()) {
    return scores;
  }
  switch (scoring_policy::settlementOf(reason)) {
    case scoring_policy::Settlement::BANK:
      scores.banked[it->second.player] += it->second.points;
      break;
    case scoring_policy::Settlement::DROP:
      break;
  }
  scores.held.erase(it);
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
  for (const auto& [order, held] : scores.held) {
    executing.push_back(order);
  }
  for (OrderId order : executing) {
    scores = settleOrder(std::move(scores), order, EndReason::ROUND_ENDED);
  }
  return scores;
}

// Highest banked score wins; equal highest scores draw
Result decideResult(const PerPlayerScores& banked) {
  std::optional<PlayerId> leader;
  bool tied = false;
  for (const auto& [player, score] : banked) {
    if (!leader || score > banked.at(*leader)) {
      leader = player;
      tied = false;
    } else if (score == banked.at(*leader)) {
      tied = true;
    }
  }
  return Result{tied ? std::nullopt : leader};
}

using Transmissions = std::map<PlayerId, std::vector<OrderTransmitted>>;

// Player of every order whose transmission is known
std::map<OrderId, PlayerId> playersOfOrders(const Transmissions& transmissions) {
  std::map<OrderId, PlayerId> players;
  for (const auto& [player, transmitted] : transmissions) {
    for (const OrderTransmitted& transmission : transmitted) {
      players.emplace(transmission.order, player);
    }
  }
  return players;
}

// How a player's order executed, as far as the observed steps tell
struct Execution {
  StepId activated_at;
  std::optional<StepId> ended_at;
  std::optional<EndReason> end_reason;
};

// An activating order hands over when its predecessor completed, and the order, submitted at or after the
// predecessor activated and replacing no earlier selection, was sent at the first opportunity at or after
// the predecessor's completion: a choice made earlier would have been sent before and interrupted it
bool handsOver(const OrderTransmitted& successor, const std::optional<Execution>& predecessor) {
  return predecessor && predecessor->end_reason == EndReason::COMPLETED && !successor.replaced &&
         predecessor->activated_at <= successor.submitted_at &&
         successor.transmitted_at == transmission_schedule_policy::firstOpportunityFrom(*predecessor->ended_at);
}

// The player's chain as the known histories determine it, with the bonus it earned over the game
Chain deriveChain(const PlayerId& player,
                  const std::vector<OrderTransmitted>& transmitted,
                  const std::map<OrderId, PlayerId>& players,
                  const std::vector<ObservedStep>& steps) {
  auto belongsToPlayer = [&players, &player](OrderId order) {
    auto it = players.find(order);
    return it != players.end() && it->second == player;
  };
  std::map<OrderId, OrderTransmitted> transmissions;
  for (const OrderTransmitted& transmission : transmitted) {
    transmissions.emplace(transmission.order, transmission);
  }

  Chain chain;
  std::map<OrderId, int> held_bonuses;  // Handover bonus held per handing over order until it ends
  std::map<OrderId, Execution> executions;
  std::optional<OrderId> executed_last;                      // The player's order that executed most recently
  std::map<OrderId, std::optional<Execution>> predecessors;  // Predecessor of each order when it activated
  for (const ObservedStep& step : steps) {
    for (const ActivationOutcome& outcome : step.activations) {
      const auto* activated = std::get_if<OrderActivated>(&outcome);
      if (activated == nullptr || !belongsToPlayer(activated->order)) {
        continue;
      }
      // An order activating over an executing one interrupts it, so it has no completed predecessor
      std::optional<Execution> predecessor;
      if (!activated->replaced && executed_last) {
        predecessor = executions.at(*executed_last);
      }
      predecessors[activated->order] = predecessor;
      // Taking over is where a handover is decided; its bonus is held until the order ends
      if (handsOver(transmissions.at(activated->order), predecessor)) {
        ++chain.length;
        held_bonuses[activated->order] = scoring_policy::handoverBonus(chain.length);
        chain.handovers[activated->order] =
            HandoverGranted{player, activated->order, chain.length, held_bonuses[activated->order]};
      } else {
        chain.length = 0;
      }
      executions[activated->order] = Execution{step.at, std::nullopt, std::nullopt};
      chain.last_activation = step.at;
      executed_last = activated->order;
      chain.open_completion.reset();
    }
    for (const ArenaEvent& event : step.events) {
      const auto* ended = std::get_if<OrderEnded>(&event);
      if (ended == nullptr || !belongsToPlayer(ended->order)) {
        continue;
      }
      auto execution = executions.find(ended->order);
      if (execution != executions.end()) {
        execution->second.ended_at = step.at;
        execution->second.end_reason = ended->reason;
      }
      // A handover's held bonus is banked by completion; any other ending drops it and breaks the chain
      auto held = held_bonuses.find(ended->order);
      if (held != held_bonuses.end()) {
        if (ended->reason == EndReason::COMPLETED) {
          chain.bonus_banked += held->second;
        }
        held_bonuses.erase(held);
      }
      if (ended->reason != EndReason::COMPLETED) {
        chain.length = 0;
      }
      chain.open_completion = ended->reason == EndReason::COMPLETED ? std::optional<StepId>{step.at} : std::nullopt;
    }
    // Elimination ends the chain, also when the snake died with no order executing
    if (step.alive.count(player) == 0) {
      chain.length = 0;
      chain.open_completion.reset();
    }
  }
  for (const auto& [order, bonus] : held_bonuses) {
    chain.bonus_held += bonus;
  }
  return chain;
}

// Re-derive every player's chain from the known histories, bank the change of their bonus and report the
// handovers counted for the first time
std::tuple<PerPlayerScores, std::map<PlayerId, Chain>, std::vector<HandoverGranted>> bankHandovers(
    PerPlayerScores banked,
    std::map<PlayerId, Chain> chains,
    const Transmissions& transmissions,
    const std::vector<ObservedStep>& steps) {
  const std::map<OrderId, PlayerId> players = playersOfOrders(transmissions);
  std::vector<HandoverGranted> granted;
  for (auto& [player, score] : banked) {
    auto transmitted = transmissions.find(player);
    const Chain derived =
        deriveChain(player,
                    transmitted == transmissions.end() ? std::vector<OrderTransmitted>{} : transmitted->second,
                    players,
                    steps);
    Chain& chain = chains[player];
    score += derived.bonus_banked - chain.bonus_banked;
    for (const auto& [order, handover] : derived.handovers) {
      auto known = chain.handovers.find(order);
      if (known == chain.handovers.end() || !(known->second == handover)) {
        granted.push_back(handover);
      }
    }
    chain = derived;
  }
  return {std::move(banked), std::move(chains), std::move(granted)};
}

}  // namespace

State startRound(RoundId round, const std::vector<PlayerId>& players) {
  State state;
  state.round_ = round;
  for (const PlayerId& player : players) {
    state.scores_.banked[player] = 0;
  }
  return state;
}

std::tuple<State, std::vector<HandoverGranted>, std::optional<GameConcluded>> observeStep(
    State state,
    StepId now,
    const std::vector<ActivationOutcome>& activations,
    const ArenaEvents& events,
    const std::set<PlayerId>& alive) {
  if (!isRunning(state)) {
    return {std::move(state), {}, std::nullopt};
  }

  state.now_ = now;
  state.scores_ = scoreEvents(std::move(state.scores_), events);
  state.steps_.push_back(ObservedStep{now, activations, events, alive});
  std::vector<HandoverGranted> granted;
  std::tie(state.scores_.banked, state.chains_, granted) =
      bankHandovers(std::move(state.scores_.banked), std::move(state.chains_), state.transmissions_, state.steps_);

  if (now < pacing_policy::LAST_STEP && !alive.empty()) {
    return {std::move(state), std::move(granted), std::nullopt};
  }

  state.scores_ = settleAtConclusion(std::move(state.scores_));
  for (auto& [player, chain] : state.chains_) {
    chain.length = 0;
    chain.bonus_held = 0;
  }
  state.result_ = decideResult(state.scores_.banked);
  return {state, std::move(granted), GameConcluded{state.round_, *state.result_}};
}

std::tuple<State, std::vector<HandoverGranted>> observeTransmission(State state, const OrderTransmitted& transmitted) {
  if (!isRunning(state) || transmitted.round != state.round_ || state.scores_.banked.count(transmitted.player) == 0) {
    return {std::move(state), {}};
  }
  std::vector<OrderTransmitted>& known = state.transmissions_[transmitted.player];
  auto same_order = [&transmitted](const OrderTransmitted& other) { return other.order == transmitted.order; };
  if (std::any_of(known.begin(), known.end(), same_order)) {
    return {std::move(state), {}};
  }
  known.push_back(transmitted);
  std::vector<HandoverGranted> granted;
  std::tie(state.scores_.banked, state.chains_, granted) =
      bankHandovers(std::move(state.scores_.banked), std::move(state.chains_), state.transmissions_, state.steps_);
  return {std::move(state), std::move(granted)};
}

bool isRunning(const State& state) { return !state.result_.has_value(); }

View view(const State& state) {
  View result;
  for (const auto& [player, banked] : state.scores_.banked) {
    auto chain = state.chains_.find(player);
    const int length = chain == state.chains_.end() ? 0 : chain->second.length;
    result.players[player] = PlayerScore{banked, 0, length, scoring_policy::handoverBonus(length + 1)};
  }
  for (const auto& [order, held] : state.scores_.held) {
    result.players[held.player].held += held.points;
  }
  for (const auto& [player, chain] : state.chains_) {
    result.players[player].held += chain.bonus_held;
  }
  result.remaining_steps = std::max(0, pacing_policy::LAST_STEP.value - state.now_.value);
  result.result = state.result_;
  return result;
}

HandoverProspect handoverProspect(const State& state, const PlayerId& player, const ProspectContext& context) {
  auto chain = state.chains_.find(player);
  if (!isRunning(state) || chain == state.chains_.end()) {
    return HandoverProspect::NONE;
  }
  const std::optional<StepId> completion =
      context.expected_completion ? context.expected_completion : chain->second.open_completion;
  if (!completion) {
    return HandoverProspect::NONE;
  }
  // A choice is sent at the first opportunity after it, which must be the first one at or after the completion
  const StepId closes = transmission_schedule_policy::firstOpportunityFrom(*completion);
  const StepId opens = transmission_schedule_policy::lastOpportunityBefore(closes).value_or(StepId{0});
  // The game concludes before an opportunity at or after its end could transmit the choice
  if (closes >= pacing_policy::LAST_STEP || context.now < opens || context.now >= closes ||
      context.transmission_pending) {
    return HandoverProspect::NONE;
  }
  if (!context.selected_at) {
    return HandoverProspect::CHOOSE_NOW;
  }
  const std::optional<StepId>& activated = chain->second.last_activation;
  const bool qualifies = !context.selection_replaced && activated && *activated <= *context.selected_at;
  return qualifies ? HandoverProspect::SUCCESSOR_READY : HandoverProspect::NONE;
}

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
