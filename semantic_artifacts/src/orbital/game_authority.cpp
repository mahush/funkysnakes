#include "orbital/game_authority.hpp"

#include <algorithm>

namespace snake {
namespace orbital {
namespace game_authority {

namespace {

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

}  // namespace

State startRound(RoundId round, const std::vector<PlayerId>& players) {
  State state;
  state.round = round;
  for (const PlayerId& player : players) {
    state.scores.credited[player] = 0;
  }
  return state;
}

std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                            GameTime now,
                                                            const ArenaEvents& events,
                                                            bool any_snake_alive) {
  if (!isRunning(state)) {
    return {std::move(state), std::nullopt};
  }

  state.now = now;
  state.scores = scoring_policy::applyScoring(std::move(state.scores), events);

  if (now < state.duration && any_snake_alive) {
    return {std::move(state), std::nullopt};
  }

  state.scores = scoring_policy::loseUnbanked(std::move(state.scores));
  state.result = decideResult(state.scores.credited);
  return {state, GameConcluded{state.round, *state.result}};
}

bool isRunning(const State& state) { return !state.result.has_value(); }

View view(const State& state) {
  View result;
  for (const auto& [player, credited] : state.scores.credited) {
    result.players[player] = PlayerScore{credited, 0};
  }
  for (const auto& [order, unbanked] : state.scores.unbanked) {
    result.players[unbanked.player].unbanked += unbanked.points;
  }
  result.remaining = std::max(GameTime{0}, state.duration - state.now);
  result.result = state.result;
  return result;
}

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
