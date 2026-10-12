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
  state.round_ = round;
  for (const PlayerId& player : players) {
    state.scores_.credited[player] = 0;
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

  state.now_ = now;
  state.scores_ = scoring_policy::applyScoring(std::move(state.scores_), events);

  if (now < state.duration_ && any_snake_alive) {
    return {std::move(state), std::nullopt};
  }

  state.scores_ = scoring_policy::loseUnbanked(std::move(state.scores_));
  state.result_ = decideResult(state.scores_.credited);
  return {state, GameConcluded{state.round_, *state.result_}};
}

bool isRunning(const State& state) { return !state.result_.has_value(); }

View view(const State& state) {
  View result;
  for (const auto& [player, credited] : state.scores_.credited) {
    result.players[player] = PlayerScore{credited, 0};
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
